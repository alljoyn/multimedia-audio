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

#include <alljoyn/audio/AudioCodec.h>

#include "RawCodec.h"
#ifdef WITH_ALAC
#include "alac/AlacCodec.h"
#endif
#include <qcc/Debug.h>

#define QCC_MODULE "ALLJOYN_AUDIO"

namespace ajn {
namespace services {

const char* MIMETYPE_AUDIO_RAW = "audio/x-raw";
const char* MIMETYPE_AUDIO_ALAC = "audio/x-alac";

const char* MIMETYPE_IMAGE_JPEG = "image/jpeg";
const char* MIMETYPE_IMAGE_PNG = "image/png";

const char* MIMETYPE_METADATA = "application/x-metadata";

void AudioDecoder::GetCapabilities(Capability** capabilities, size_t* numCapabilities) {
    *numCapabilities = 1;
#ifdef WITH_ALAC
    bool alacEnabled = true;
    char* e = getenv("DISABLE_ALAC");
    if (e != NULL && strcmp(e, "1") == 0) {
        alacEnabled = false;
    }
    if (alacEnabled) {
        ++*numCapabilities;
    }
#endif

    int i = 0;
    *capabilities = new Capability[*numCapabilities];
    RawDecoder::GetCapability(&(*capabilities)[i++]);
#ifdef WITH_ALAC
    if (alacEnabled) {
        AlacDecoder::GetCapability(&(*capabilities)[i++]);
    }
#endif
}

AudioDecoder* AudioDecoder::Create(const char* type) {
    if (strcmp(type, MIMETYPE_AUDIO_RAW) == 0) {
        return new RawDecoder();
    }
#ifdef WITH_ALAC
    else if (strcmp(type, MIMETYPE_AUDIO_ALAC) == 0) {
        return new AlacDecoder();
    }
#endif
    return NULL;
}

bool AudioEncoder::CanCreate(const char* type) {
    if (strcmp(type, MIMETYPE_AUDIO_RAW) == 0) {
        return true;
    }
#ifdef WITH_ALAC
    else if (strcmp(type, MIMETYPE_AUDIO_ALAC) == 0) {
        return true;
    }
#endif
    return false;
}

AudioEncoder* AudioEncoder::Create(const char* type) {
    if (strcmp(type, MIMETYPE_AUDIO_RAW) == 0) {
        return new RawEncoder();
    }
#ifdef WITH_ALAC
    else if (strcmp(type, MIMETYPE_AUDIO_ALAC) == 0) {
        return new AlacEncoder();
    }
#endif
    return NULL;
}

}
}
