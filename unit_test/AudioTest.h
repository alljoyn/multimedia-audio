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

#include "gtest/gtest.h"
#include <alljoyn/BusAttachment.h>

using namespace qcc;
using namespace ajn;

class TestClientListener;

class AudioTest : public testing::Environment {
  public:

    AudioTest();
    virtual ~AudioTest();
    virtual void SetUp();
    virtual void TearDown();

    static char* sServiceName;
    static char* sStreamObjectPath;
    static uint16_t sSessionPort;
    static bool sReady;
    static uint32_t sTimeout;

  private:
    TestClientListener* mListener;
    BusAttachment* mMsgBus;
};
