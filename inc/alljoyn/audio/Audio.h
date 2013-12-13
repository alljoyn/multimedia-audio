/**
 * @file
 * Definitions common to both service and client.
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

#ifndef _AUDIO_H
#define _AUDIO_H

#ifndef __cplusplus
#error Only include Audio.h in C++ code.
#endif

#include <alljoyn/MsgArg.h>

namespace ajn {
namespace services {

namespace audio {
const char* GetVersion();               /**< Gives the version of AllJoyn audio */
const char* GetBuildInfo();             /**< Gives build information of AllJoyn audio */
uint32_t GetNumericVersion();           /**< Gives the version of AllJoyn audio as a single number */
}

extern const char* MIMETYPE_AUDIO_RAW; /**< The media type of a raw (PCM) audio port capability. */
extern const char* MIMETYPE_AUDIO_ALAC; /**< The media type of an ALAC encoded audio port capability. */

extern const char* MIMETYPE_IMAGE_JPEG; /**< The media type of a JPEG image port capability. */
extern const char* MIMETYPE_IMAGE_PNG; /**< The media type of a PNG image port capability. */

extern const char* MIMETYPE_METADATA; /**< The media type of a metadata port capability. */

/**
 * A capability of a port.
 */
struct Capability {
    qcc::String type; /**< The media type of this capability. */
    ajn::MsgArg* parameters;  /**< An array of parameters of the media type. */
    size_t numParameters; /**< The number of parameters in the array. */
    Capability() : parameters(NULL), numParameters(0) { }
};

}
}

#endif /* _AUDIO_H */
