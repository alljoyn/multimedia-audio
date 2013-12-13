/**
 * @file
 * ALAC encoders and decoders.
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

#ifndef _ALACCODEC_H
#define _ALACCODEC_H

#ifndef __cplusplus
#error Only include AlacCodec.h in C++ code.
#endif

#include "ALACAudioTypes.h"
#include "ALACDecoder.h"
#include "ALACEncoder.h"
#include <alljoyn/audio/AudioCodec.h>

namespace ajn {
namespace services {

/**
 * An ALAC decoder used by AudioSinkObject.
 */
class AlacDecoder : public AudioDecoder {
  public:
    AlacDecoder();
    ~AlacDecoder();

    /**
     * Gets the capability of this ALAC decoder.  This is suitable for
     * use in the Capabilities property of a Port object.
     *
     * @param[out] capability this (and each parameters member) should be
     *                        delete[]'d by the caller.
     */
    static void GetCapability(Capability* capability);

    QStatus Configure(Capability* configuration);
    uint32_t GetFrameSize() const { return mFramesPerPacket; }
    void Decode(uint8_t** buffer, uint32_t* numBytes);

  private:
    ALACDecoder* mDecoder;
    uint8_t* mDecodeBuffer;
    uint32_t mChannelsPerFrame;
    uint32_t mBytesPerFrame;
    uint32_t mFramesPerPacket;
};

/**
 * An ALAC encoder used by SinkPlayer.
 */
class AlacEncoder : public AudioEncoder {
  public:
    AlacEncoder();
    ~AlacEncoder();

    QStatus Configure(DataSource* dataSource);
    uint32_t GetFrameSize() const { return mOutputFormat.mFramesPerPacket; }
    void GetConfiguration(Capability* configuration);
    void Encode(uint8_t** buffer, uint32_t* numBytes);

  private:
    ALACEncoder* mEncoder;
    AudioFormatDescription mInputFormat;
    AudioFormatDescription mOutputFormat;
    uint8_t* mEncodeBuffer;
};

}
}

#endif /* _ALACCODEC_H */
