/**
 * @file
 * Audio encoders and decoders.
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

#ifndef _AUDIOCODEC_H
#define _AUDIOCODEC_H

#ifndef __cplusplus
#error Only include AudioCodec.h in C++ code.
#endif

#include <alljoyn/audio/Audio.h>
#include <alljoyn/audio/DataSource.h>

namespace ajn {
namespace services {

/**
 * The maximum number of frames to return in one call of Decode() or
 * Encode().
 */
const uint32_t FRAMES_PER_PACKET = 16384;

/**
 * The base class of audio decoders used by AudioSinkObject.
 */
class AudioDecoder {
  public:
    /**
     * Gets the capabilities of all enabled audio decoders.  This is suitable as
     * the Capabilities property of a Port object.
     *
     * @param[out] capabilities these (and each parameters member) should be
     *                          delete[]'d by the caller.
     * @param[out] numCapabilities
     */
    static void GetCapabilities(Capability** capabilities, size_t* numCapabilities);

    /**
     * Creates an instance of an AudioDecoder for the media type.
     *
     * @param[in] type the media type.
     *
     * @return an AudioDecoder.  This should be delete'd by the caller.
     */
    static AudioDecoder* Create(const char* type);

    virtual ~AudioDecoder() { }

    /**
     * Configures the decoder with the selected parameters.
     *
     * @param[in] configuration the selected parameters.
     *
     * @return ER_OK if configured.
     */
    virtual QStatus Configure(Capability* configuration) = 0;

    /**
     * Gets the maximum number of frames returned in one call of Decode().
     *
     * @return the maximum number of frames returned in one call of Decode().
     */
    virtual uint32_t GetFrameSize() const = 0;

    /**
     * Decodes audio data.
     *
     * @param[in,out] buffer on input, the encoded data.  On output, the decoded
     *                       data.  The returned buffer will be free()'d by the
     *                       caller.
     * @param[in,out] numBytes on input, the number of bytes of encoded data.  On
     *                         output, the number of bytes of decoded data.
     */
    virtual void Decode(uint8_t** buffer, uint32_t* numBytes) = 0;
};

/**
 * The base class of audio encoders used by SinkPlayer.
 */
class AudioEncoder {
  public:
    /**
     * Returns true if encoding the media type is enabled.
     *
     * @param[in] type the media type.
     *
     * @return true if encoding the media type is enabled, false otherwise.
     */
    static bool CanCreate(const char* type);

    /**
     * Creates an instance of an AudioEncoder for the media type.
     *
     * @param[in] type the media type.
     *
     * @return an AudioEncoder.  This should be delete'd by the caller.
     */
    static AudioEncoder* Create(const char* type);

    virtual ~AudioEncoder() { }

    /**
     * Configures this encoder for the data source.
     *
     * @param[in] dataSource the data source.
     *
     * @return ER_OK if successful, an error otherwise.
     */
    virtual QStatus Configure(DataSource* dataSource) = 0;

    /**
     * Gets the current configuration.  This is suitable as the configuration
     * parameter of Connect.
     *
     * @param[out] configuration the configuration as a Capability.  The caller
     *                           will delete[] the parameters member.
     */
    virtual void GetConfiguration(Capability* configuration) = 0;

    /**
     * Gets the maximum number of frames that can be passed to one call of
     * Encode().
     *
     * @return the maximum number of frames that can be passed.
     */
    virtual uint32_t GetFrameSize() const = 0;

    /**
     * Encodes audio data.
     *
     * @param[in,out] buffer on input, the unencoded data.  On output the encoded
     *                       data.  This returned buffer will not be free()'d by
     *                       the caller.
     * @param[in,out] numBytes on input, the number of bytes of unencoded data.
     *                         On output, the number of bytes of encoded data.
     */
    virtual void Encode(uint8_t** buffer, uint32_t* numBytes) = 0;
};

}
}

#endif /* _AUDIOCODEC_H */
