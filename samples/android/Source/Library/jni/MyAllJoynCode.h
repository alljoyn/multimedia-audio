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
#include "Constants.h"
#include <alljoyn/audio/SinkPlayer.h>
#include <alljoyn/audio/SinkSearcher.h>
#include <alljoyn/audio/DataSource.h>

#ifndef _MY_ALLJOYN_CODE_
#define _MY_ALLJOYN_CODE_

class MyAllJoynCode : public ajn::services::SinkSearcher, public ajn::services::SinkListener  {
  public:
    MyAllJoynCode(JavaVM* vm, jobject jobj) : vm(vm), jobj(jobj),
        mBusAttachment(NULL), mSinkPlayer(NULL),
        mCurrentDataSource(NULL), isMuted(false), wasStopped(true)  { };

    ~MyAllJoynCode() {
        Release();
        if (mSinkPlayer) {
            delete mSinkPlayer;
        }
        mSinkPlayer = NULL;
    };

    void Prepare(const char* packageName);

    void SetDataSource(const char* dataSource);

    void AddSink(const char*name, const char*path, uint16_t port);

    void RemoveSink(const char*name);

    void Start();

    void Pause();

    void Stop();

    void Reset();

    void ChangeVolume(float value);

    void Mute();

    void Release();

    /* SinkSearcher */
    virtual void SinkFound(Service*sink);

    virtual void SinkLost(Service*sink);

    /* SinkListener */
    void SinkAdded(const char*name);

    void SinkAddFailed(const char*name);

    void SinkRemoved(const char*name, bool lost);

  private:
    JavaVM* vm;
    jobject jobj;
    /* Static data */
    qcc::String wellKnownName;
    ajn::BusAttachment* mBusAttachment;
    ajn::services::SinkPlayer* mSinkPlayer;
    ajn::services::DataSource* mCurrentDataSource;
    std::list<qcc::String> mSinkNames;
    bool isMuted;
    bool wasStopped;
};

#endif //_MY_ALLJOYN_CODE_
