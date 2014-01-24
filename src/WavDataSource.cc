/**
 * @file
 * Handle feeding input into the SinkPlayer
 */

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

#include <alljoyn/audio/WavDataSource.h>

#include <qcc/Debug.h>
#include <qcc/Util.h>

#define QCC_MODULE "ALLJOYN_AUDIO"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define DATA_IDENTIFIER 0x64617461 // 'data'
#define RIFF_IDENTIFIER 0x52494646 // 'RIFF'
#define WAVE_IDENTIFIER 0x57415645 // 'WAVE'
#define FMT_IDENTIFIER  0x666d7420 // 'fmt '

namespace ajn {
namespace services {

WavDataSource::WavDataSource() : DataSource(), mInputFileMutex(new qcc::Mutex()), mInputFile(NULL) {
}

WavDataSource::~WavDataSource() {
    Close();
    delete mInputFileMutex;
}

bool WavDataSource::Open(FILE* inputFile) {
    if (mInputFile) {
        QCC_LogError(ER_FAIL, ("already open"));
        return false;
    }

    mInputFile = inputFile;
    if (!ReadHeader()) {
        QCC_LogError(ER_FAIL, ("file is not a PCM wave file"));
        Close();
        return false;
    }

    if (!(mSampleRate == 44100 || mSampleRate == 48000) ||
        mBitsPerChannel != 16 ||
        !(mChannelsPerFrame == 1 || mChannelsPerFrame == 2)) {
        QCC_LogError(ER_FAIL, ("file is not s16le, 44100|48000, 1|2"));
        QCC_DbgHLPrintf(("mSampleRate=%f\n"         \
                         "mChannelsPerFrame=%d\n"   \
                         "mBytesPerFrame=%d\n"      \
                         "mBitsPerChannel=%d",
                         mSampleRate, mChannelsPerFrame,
                         mBytesPerFrame, mBitsPerChannel));
        Close();
        return false;
    }
    return true;
}

bool WavDataSource::Open(const char* filePath) {
    if (mInputFile) {
        QCC_LogError(ER_FAIL, ("already open"));
        return false;
    }

    FILE*inputFile = fopen(filePath, "rb");
    if (inputFile == NULL) {
        QCC_LogError(ER_FAIL, ("can't open file '%s'", filePath));
        return false;
    }

    return Open(inputFile);
}

void WavDataSource::Close() {
    if (mInputFile != NULL) {
        fclose(mInputFile);
        mInputFile = NULL;
    }
}

bool WavDataSource::ReadHeader() {
    uint8_t buffer[20];
    size_t n = fread(buffer, 1, 4, mInputFile);
    if (n != 4 || betoh32(*((uint32_t*)buffer)) != RIFF_IDENTIFIER) {
        return false;
    }

    n = fread(buffer, 1, 8, mInputFile);
    if (n != 8 || betoh32(*((uint32_t*)&buffer[4])) != WAVE_IDENTIFIER) {
        return false;
    }

    while (true) {
        n = fread(buffer, 1, 8, mInputFile);
        if (n != 8) {
            return false;
        }
        uint32_t chunkSize = ((int32_t)(buffer[7]) << 24) + ((int32_t)(buffer[6]) << 16) + ((int32_t)(buffer[5]) << 8) + buffer[4];

        switch (betoh32(*((uint32_t*)buffer))) {
        case FMT_IDENTIFIER:
            n = fread(buffer, 1, 16, mInputFile);
            if (n != 16 || buffer[0] != 1 || buffer[1] != 0) {            // if not PCM
                return false;
            }
            mChannelsPerFrame = buffer[2];
            mSampleRate = ((int32_t)(buffer[7]) << 24) + ((int32_t)(buffer[6]) << 16) + ((int32_t)(buffer[5]) << 8) + buffer[4];
            mBitsPerChannel = buffer[14];
            mBytesPerFrame = (mBitsPerChannel >> 3) * mChannelsPerFrame;
            fseek(mInputFile, chunkSize - 16, SEEK_CUR);
            break;

        case DATA_IDENTIFIER:
            mInputSize = ((int32_t)(buffer[7]) << 24) + ((int32_t)(buffer[6]) << 16) + ((int32_t)(buffer[5]) << 8) + buffer[4];
            mInputDataStart = ftell(mInputFile);
            return true;

        default:
            // skip
            fseek(mInputFile, chunkSize, SEEK_CUR);
            break;
        }
    }

    return false;
}

size_t WavDataSource::ReadData(uint8_t* buffer, size_t offset, size_t length) {
    mInputFileMutex->Lock();
    size_t r = 0;
    if (mInputFile) {
        fseek(mInputFile, mInputDataStart + offset, SEEK_SET);
        length = MIN(mInputSize - offset, length);
        r = fread(buffer, 1, length, mInputFile);
    }
    mInputFileMutex->Unlock();
    return r;
}

}
}
