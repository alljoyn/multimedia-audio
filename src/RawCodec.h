/**
 * @file
 * PCM encoders and decoders.
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

#ifndef _RAWCODEC_H
#define _RAWCODEC_H

#ifndef __cplusplus
#error Only include RawCodec.h in C++ code.
#endif

#include <alljoyn/audio/AudioCodec.h>

namespace ajn {
namespace services {

/**
 * A PCM decoder used by AudioSinkObject.
 *
 * The decode is just a pass-through.
 */
class RawDecoder : public AudioDecoder {
  public:
    RawDecoder();
    ~RawDecoder();

    /**
     * Gets the capability of this raw decoder.  This is suitable for
     * use in the Capabilities property of a Port object.
     *
     * @param[out] capability this (and each parameters member) should be
     *                        delete[]'d by the caller.
     */
    static void GetCapability(Capability* capability);

    QStatus Configure(Capability* capability);
    uint32_t GetFrameSize() const { return FRAMES_PER_PACKET; }
    void Decode(uint8_t** buffer, uint32_t* numBytes);
};

/**
 * A PCM encoder used by SinkPlayer.
 *
 * The encode is just a pass-through.
 */
class RawEncoder : public AudioEncoder {
  public:
    RawEncoder();
    ~RawEncoder();

    QStatus Configure(DataSource* dataSource);
    uint32_t GetFrameSize() const { return FRAMES_PER_PACKET; }
    void GetConfiguration(Capability* configuration);
    void Encode(uint8_t** buffer, uint32_t* numBytes);

  private:
    uint32_t mChannelsPerFrame;
    double mSampleRate;
};

}
}

#endif /* _RAW_H */
