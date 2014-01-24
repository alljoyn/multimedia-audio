/******************************************************************************
 * Copyright (c) 2013-2014, doubleTwist Corporation and AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/
#define __STDC_FORMAT_MACROS

#include "AlacCodec.h"

#include "../Clock.h"
#include "ALACBitUtilities.h"
#include "EndianPortable.h"
#include <qcc/Debug.h>
#include <inttypes.h>

#define QCC_MODULE "ALLJOYN_AUDIO"

using namespace ajn;

namespace ajn {
namespace services {

// Adapted from CoreAudioTypes.h
enum {
    kTestFormatFlag_16BitSourceData    = 1,
    kTestFormatFlag_20BitSourceData    = 2,
    kTestFormatFlag_24BitSourceData    = 3,
    kTestFormatFlag_32BitSourceData    = 4
};

MsgArg* GetParameterValue(MsgArg* parameters, size_t numParameters, const char* name) {
    for (size_t i = 0; i < numParameters; i++)
        if (0 == strcmp(parameters[i].v_dictEntry.key->v_string.str, name)) {
            return parameters[i].v_dictEntry.val->v_variant.val;
        }
    return NULL;
}

AlacDecoder::AlacDecoder() :
    mDecoder(NULL), mDecodeBuffer(NULL) {
}

AlacDecoder::~AlacDecoder() {
    if (mDecodeBuffer != NULL) {
        free((void*)mDecodeBuffer);
        mDecodeBuffer = NULL;
    }

    if (mDecoder != NULL) {
        delete mDecoder;
        mDecoder = NULL;
    }
}

void AlacDecoder::GetCapability(Capability* capability) {
    capability->type = MIMETYPE_AUDIO_ALAC;
    capability->numParameters = 3;
    capability->parameters = new MsgArg[capability->numParameters];
    static uint8_t channels[2] = { 1, 2 };
    capability->parameters[0].Set("{sv}", "Channels", new MsgArg("ay", 2, channels));
    capability->parameters[0].SetOwnershipFlags(MsgArg::OwnsArgs, true);
    static uint16_t rates[2] = { 44100, 48000 };
    capability->parameters[1].Set("{sv}", "Rate", new MsgArg("aq", 2, rates));
    capability->parameters[1].SetOwnershipFlags(MsgArg::OwnsArgs, true);
    static const char* formats[1] = { "s16le" };
    capability->parameters[2].Set("{sv}", "Format", new MsgArg("as", 1, formats));
    capability->parameters[2].SetOwnershipFlags(MsgArg::OwnsArgs, true);
}

QStatus AlacDecoder::Configure(Capability* configuration) {
    MsgArg* channelsArg = GetParameterValue(configuration->parameters, configuration->numParameters, "Channels");
    MsgArg* magicCookieArg = GetParameterValue(configuration->parameters, configuration->numParameters, "MagicCookie");
    MsgArg* framesPerPacketArg = GetParameterValue(configuration->parameters, configuration->numParameters, "FramesPerPacket");
    if (channelsArg == NULL || magicCookieArg == NULL || framesPerPacketArg == NULL) {
        QCC_LogError(ER_FAIL, ("Configure is missing Channels, MagicCookie or FramesPerPacket parameter"));
        return ER_FAIL;
    }

    mChannelsPerFrame = channelsArg->v_byte;

    uint32_t bitsPerChannel = 16;
    mBytesPerFrame = (bitsPerChannel >> 3) * mChannelsPerFrame;

    void* magicCookie = NULL;
    size_t magicCookieSize = 0;
    QStatus status = magicCookieArg->Get("ay", &magicCookieSize, &magicCookie);
    if (status != ER_OK) {
        QCC_LogError(status, ("Configure bad MagicCookie param"));
        return status;
    }

    status = framesPerPacketArg->Get("u", &mFramesPerPacket);
    if (status != ER_OK) {
        QCC_LogError(status, ("Configure bad FramesPerPacket param"));
        return status;
    }

    mDecoder = new ALACDecoder();
    mDecoder->Init(magicCookie, magicCookieSize);

    mDecodeBuffer = (uint8_t*)calloc(GetFrameSize() * mBytesPerFrame, 1);
    return ER_OK;
}

void AlacDecoder::Decode(uint8_t** buffer, uint32_t* numBytes) {
    uint32_t numFrames = 0;
    BitBuffer inputBuffer;
    BitBufferInit(&inputBuffer, *buffer, *numBytes);
    uint64_t start = GetCurrentTimeNanos();
    mDecoder->Decode(&inputBuffer, mDecodeBuffer, mFramesPerPacket, mChannelsPerFrame, &numFrames);
    QCC_DbgTrace(("Decoded %u alac frames, took %" PRIu64 " nanos", numFrames, GetCurrentTimeNanos() - start));

#ifdef TARGET_RT_BIG_ENDIAN
    uint16_t* theShort = (uint16_t*)mDecodeBuffer;
    for (int32_t i = 0; i < (numBytes >> 1); ++i) {
        Swap16(&(theShort[i]));
    }
#endif

    free((void*)*buffer);
    *buffer = NULL;
    *numBytes = numFrames * mBytesPerFrame;

    *buffer = (uint8_t*)malloc(*numBytes);
    memcpy(*buffer, mDecodeBuffer, *numBytes);
}

AlacEncoder::AlacEncoder() :
    mEncoder(NULL), mEncodeBuffer(NULL) {
}

AlacEncoder::~AlacEncoder() {
    if (mEncodeBuffer != NULL) {
        free((void*)mEncodeBuffer);
        mEncodeBuffer = NULL;
    }

    if (mEncoder != NULL) {
        delete mEncoder;
        mEncoder = NULL;
    }
}

QStatus AlacEncoder::Configure(DataSource* dataSource) {
    mInputFormat.mFormatID = kALACFormatLinearPCM;
    mInputFormat.mChannelsPerFrame = dataSource->GetChannelsPerFrame();
    mInputFormat.mSampleRate = dataSource->GetSampleRate();
    mInputFormat.mBitsPerChannel = dataSource->GetBitsPerChannel();
    mInputFormat.mFormatFlags = kALACFormatFlagIsSignedInteger | kALACFormatFlagIsPacked; // always little endian
    mInputFormat.mBytesPerPacket = mInputFormat.mBytesPerFrame = dataSource->GetBytesPerFrame();
    mInputFormat.mFramesPerPacket = 1;
    mInputFormat.mReserved = 0;

    mOutputFormat.mFormatID = kALACFormatAppleLossless;
    mOutputFormat.mSampleRate = dataSource->GetSampleRate();
    mOutputFormat.mFormatFlags = kTestFormatFlag_16BitSourceData;
    mOutputFormat.mFramesPerPacket = FRAMES_PER_PACKET;
    mOutputFormat.mChannelsPerFrame = dataSource->GetChannelsPerFrame();
    // mBytesPerPacket == 0 because we are VBR
    // mBytesPerFrame and mBitsPerChannel == 0 because there are no discernable bits assigned to a particular sample
    // mReserved is always 0
    mOutputFormat.mBytesPerPacket = mOutputFormat.mBytesPerFrame = mOutputFormat.mBitsPerChannel = mOutputFormat.mReserved = 0;

    mEncoder = new ALACEncoder();
    mEncoder->SetFrameSize(mOutputFormat.mFramesPerPacket);
    mEncoder->InitializeEncoder(mOutputFormat);

    uint32_t inputPacketBytes = mInputFormat.mBytesPerFrame * mOutputFormat.mFramesPerPacket;
    mEncodeBuffer = (uint8_t*)calloc(inputPacketBytes + kALACMaxEscapeHeaderBytes, 1);
    return ER_OK;
}

void AlacEncoder::GetConfiguration(Capability* configuration) {
    configuration->type = MIMETYPE_AUDIO_ALAC;
    configuration->numParameters = 5;
    configuration->parameters = new MsgArg[configuration->numParameters];
    configuration->parameters[0].Set("{sv}", "Channels", new MsgArg("y", (uint8_t)mOutputFormat.mChannelsPerFrame));
    configuration->parameters[0].SetOwnershipFlags(MsgArg::OwnsArgs, true);
    configuration->parameters[1].Set("{sv}", "Rate", new MsgArg("q", (uint16_t)mOutputFormat.mSampleRate));
    configuration->parameters[1].SetOwnershipFlags(MsgArg::OwnsArgs, true);
    configuration->parameters[2].Set("{sv}", "Format", new MsgArg("s", "s16le"));
    configuration->parameters[2].SetOwnershipFlags(MsgArg::OwnsArgs, true);
    uint32_t magicCookieSize = mEncoder->GetMagicCookieSize(mOutputFormat.mChannelsPerFrame);
    uint8_t* magicCookie = new uint8_t[magicCookieSize];
    memset(magicCookie, 0, magicCookieSize);
    mEncoder->GetMagicCookie(magicCookie, &magicCookieSize);
    MsgArg* magicCookieArg = new MsgArg("ay", magicCookieSize, magicCookie);
    magicCookieArg->SetOwnershipFlags(MsgArg::OwnsData, true);
    configuration->parameters[3].Set("{sv}", "MagicCookie", magicCookieArg);
    configuration->parameters[3].SetOwnershipFlags(MsgArg::OwnsArgs, true);
    configuration->parameters[4].Set("{sv}", "FramesPerPacket", new MsgArg("u", mOutputFormat.mFramesPerPacket));
    configuration->parameters[4].SetOwnershipFlags(MsgArg::OwnsArgs, true);
}

void AlacEncoder::Encode(uint8_t** buffer, uint32_t* numBytes) {
    if ((mInputFormat.mFormatFlags & 0x02) != kALACFormatFlagsNativeEndian) {
        uint16_t* theShort = (uint16_t*)(*buffer);
        for (int32_t i = 0; i < (*numBytes >> 1); ++i) {
            Swap16(&(theShort[i]));
        }
    }
    mEncoder->Encode(mInputFormat, mOutputFormat, *buffer, mEncodeBuffer, (int32_t*)numBytes);
    *buffer = mEncodeBuffer;
}

}
}
