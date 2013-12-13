/**
 * @file
 * Data input sources.
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

#ifndef _DATASOURCE_H_
#define _DATASOURCE_H_

#ifndef __cplusplus
#error Only include DataSource.h in C++ code.
#endif

#include <stddef.h>
#include <stdint.h>

namespace ajn {
namespace services {

/**
 * The base class of data input sources.
 */
class DataSource {
  public:
    virtual ~DataSource() { }

    /**
     * @return the sample rate of the data source.
     */
    virtual double GetSampleRate() = 0;
    /**
     * @return the bytes per frame of the data source.
     */
    virtual uint32_t GetBytesPerFrame() = 0;
    /**
     * @return the channels per frame of the data source.
     */
    virtual uint32_t GetChannelsPerFrame() = 0;
    /**
     * @return the bits per channel of the data source.
     */
    virtual uint32_t GetBitsPerChannel() = 0;
    /**
     * @return the size of the data source in bytes.
     */
    virtual uint32_t GetInputSize() = 0;

    /**
     * Reads data from the source.
     *
     * @param[in] buffer the buffer to read data into.
     * @param[in] offset the byte offset from the beginning of the
     *                   data source to read from.
     * @param[in] length the length of buffer.
     *
     * @return the number of bytes read.
     */
    virtual size_t ReadData(uint8_t* buffer, size_t offset, size_t length) = 0;

    /**
     * Used by thread that calls ReadData to ensure a data is ready for reading
     * @return true if data is ready to read
     */
    virtual bool IsDataReady() = 0;
};

}
}

#endif //_DATASOURCE_H_
