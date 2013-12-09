/******************************************************************************
 * Copyright (c) 2013, AllSeen Alliance. All rights reserved.
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

#include <alljoyn/AllJoynStd.h>
#include <alljoyn/BusListener.h>
#include <alljoyn/SessionPortListener.h>
#include <alljoyn/SessionListener.h>
#include <alljoyn/BusAttachment.h>
#include <cstdio>

#ifndef _MY_ALLJOYN_LISTENERS_
#define _MY_ALLJOYN_LISTENERS_

using namespace ajn;

class MyAllJoynListeners : public SessionPortListener {
  public:
    MyAllJoynListeners(BusAttachment* bus) : mBusAttachment(bus) { };

    virtual ~MyAllJoynListeners() {
        mBusAttachment = NULL;
    };

    virtual bool AcceptSessionJoiner(SessionPort sessionPort, const char* joiner, const SessionOpts& opts);

    virtual void SessionJoined(SessionPort sessionPort, SessionId id, const char* joiner);

  private:
    BusAttachment* mBusAttachment;
};

#endif //_MY_ALLJOYN_LISTENERS_
