/******************************************************************************
 * Copyright (c) 2013, doubleTwist Corporation and AllSeen Alliance. All rights reserved.
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

#include "RawCodec.h"

#include <qcc/Debug.h>

#define QCC_MODULE "ALLJOYN_AUDIO"

using namespace ajn;

namespace ajn {
namespace services {

RawDecoder::RawDecoder() {
}

RawDecoder::~RawDecoder() {
}

void RawDecoder::GetCapability(Capability* capability) {
    capability->type = MIMETYPE_AUDIO_RAW;
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

QStatus RawDecoder::Configure(Capability* capability) {
    return ER_OK;
}

void RawDecoder::Decode(uint8_t** buffer, uint32_t* numBytes) {
    /* Nothing to do */
}

RawEncoder::RawEncoder() {
}

RawEncoder::~RawEncoder() {
}

QStatus RawEncoder::Configure(DataSource* dataSource) {
    mChannelsPerFrame = dataSource->GetChannelsPerFrame();
    mSampleRate = dataSource->GetSampleRate();
    return ER_OK;
}

void RawEncoder::GetConfiguration(Capability* configuration) {
    configuration->type = MIMETYPE_AUDIO_RAW;
    configuration->numParameters = 3;
    configuration->parameters = new MsgArg[configuration->numParameters];
    configuration->parameters[0].Set("{sv}", "Channels", new MsgArg("y", (uint8_t)mChannelsPerFrame));
    configuration->parameters[0].SetOwnershipFlags(MsgArg::OwnsArgs, true);
    configuration->parameters[1].Set("{sv}", "Rate", new MsgArg("q", (uint16_t)mSampleRate));
    configuration->parameters[1].SetOwnershipFlags(MsgArg::OwnsArgs, true);
    configuration->parameters[2].Set("{sv}", "Format", new MsgArg("s", "s16le"));
    configuration->parameters[2].SetOwnershipFlags(MsgArg::OwnsArgs, true);
}

void RawEncoder::Encode(uint8_t** buffer, uint32_t* numBytes) {
    /* Nothing to do */
}

}
}
