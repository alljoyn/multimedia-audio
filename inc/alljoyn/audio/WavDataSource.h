/**
 * @file
 * Handle feeding input into the SinkPlayer
 */

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

#ifndef _WAVDATASOURCE_H_
#define _WAVDATASOURCE_H_

#ifndef __cplusplus
#error Only include WavDataSource.h in C++ code.
#endif

#include <alljoyn/audio/DataSource.h>
#include <stdio.h>

namespace qcc { class Mutex; }

namespace ajn {
namespace services {

/**
 * A WAV file data input source.
 */
class WavDataSource : public DataSource {
  public:
    WavDataSource();
    virtual ~WavDataSource();

    /**
     * Opens the file used to read data from.
     *
     * @param[in] inputFile the file pointer.
     *
     * @return true if open.
     */
    bool Open(FILE* inputFile);
    /**
     * Opens the file used to read data from.
     *
     * @param[in] filePath the file path.
     *
     * @return true if open.
     */
    bool Open(const char* filePath);
    /**
     * Closes the file used to read data from.
     */
    void Close();

    double GetSampleRate() { return mSampleRate; }
    uint32_t GetBytesPerFrame() { return mBytesPerFrame; }
    uint32_t GetChannelsPerFrame() { return mChannelsPerFrame; }
    uint32_t GetBitsPerChannel() { return mBitsPerChannel; }
    uint32_t GetInputSize() { return mInputSize; }

    size_t ReadData(uint8_t* buffer, size_t offset, size_t length);

    /**
     * Since we read ondemand from a file always return true that data is ready
     */
    bool IsDataReady() { return true; }

  private:
    bool ReadHeader();

    double mSampleRate;
    uint32_t mBytesPerFrame;
    uint32_t mChannelsPerFrame;
    uint32_t mBitsPerChannel;
    uint32_t mInputSize;
    uint32_t mInputDataStart;
    qcc::Mutex* mInputFileMutex;
    FILE* mInputFile;
};

}
}

#endif //_WAVDATASOURCE_H_
