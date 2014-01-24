/******************************************************************************
 * Copyright (c) 2013-2014, AllSeen Alliance. All rights reserved.
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

#include <alljoyn/BusAttachment.h>
#include <alljoyn/ProxyBusObject.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/AllJoynStd.h>
#include <qcc/String.h>
#include "Constants.h"
#include <alljoyn/audio/StreamObject.h>
#include <alljoyn/audio/AudioDevice.h>
#include "MyAllJoynListeners.h"

#ifndef _MY_ALLJOYN_CODE_
#define _MY_ALLJOYN_CODE_

class MyAllJoynCode {
  public:
    MyAllJoynCode()
        : listener(NULL), mBusAttachment(NULL), mAboutProps(NULL), mStreamObj(NULL) { };

    ~MyAllJoynCode() {
        shutdown();
        if (listener) {
            delete listener;
        }
        listener = NULL;
    };

    void initialize(const char* packageName);

    void shutdown();

  private:
    MyAllJoynListeners* listener;
    BusAttachment* mBusAttachment;
    qcc::String mAdvertisedName;
    services::PropertyStore* mAboutProps;
    services::StreamObject* mStreamObj;
    services::AudioDevice* mAudioDevice;
};

#endif //_MY_ALLJOYN_CODE_
