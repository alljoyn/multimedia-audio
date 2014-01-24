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
#define __STDC_FORMAT_MACROS

#include <alljoyn/audio/StreamObject.h>

#include "AudioSinkObject.h"
#include "Clock.h"
#include "ImageSinkObject.h"
#include "MetadataSinkObject.h"
#include "Sink.h"
#include <alljoyn/BusAttachment.h>
#include <qcc/Debug.h>
#include <qcc/Mutex.h>
#include <inttypes.h>

#define QCC_MODULE "ALLJOYN_AUDIO"

using namespace ajn;
using namespace ajn::services;
using namespace qcc;
using namespace std;

namespace ajn {
namespace services {

StreamObject::StreamObject(BusAttachment* bus, const char* path, AudioDevice* audioDevice,
                           SessionPort sp, PropertyStore* props)
    : BusObject(path), mOwner(NULL), mAudioDevice(audioDevice), mAbout(NULL),
    mAudioSinkObjectPath(NULL), mImageSinkObjectPath(NULL), mMetadataSinkObjectPath(NULL),
    mPortsMutex(new qcc::Mutex()), mClockAdjustment(0) {
    mSessionPort = sp;
    mAbout = new AboutService(*bus, *props);

    mAudioSinkObjectPath = strdup((String(path) + String("/Audio")).c_str());
    mImageSinkObjectPath = strdup((String(path) + String("/Image")).c_str());
    mMetadataSinkObjectPath = strdup((String(path) + String("/Metadata")).c_str());

    QStatus status = bus->CreateInterfacesFromXml(INTERFACES_XML);
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to create interfaces from XML"));
    }

    const InterfaceDescription* streamIntf = bus->GetInterface(STREAM_INTERFACE);
    assert(streamIntf);
    AddInterface(*streamIntf);

    const InterfaceDescription* clockIntf = bus->GetInterface(CLOCK_INTERFACE);
    assert(clockIntf);
    AddInterface(*clockIntf);

    /** Register the method handlers with the object */
    const MethodEntry methodEntries[] = {
        { streamIntf->GetMember("Open"), static_cast<MessageReceiver::MethodHandler>(&StreamObject::Open) },
        { streamIntf->GetMember("Close"), static_cast<MessageReceiver::MethodHandler>(&StreamObject::Close) },
        { clockIntf->GetMember("SetTime"), static_cast<MessageReceiver::MethodHandler>(&StreamObject::SetTime) },
        { clockIntf->GetMember("AdjustTime"), static_cast<MessageReceiver::MethodHandler>(&StreamObject::AdjustTime) }
    };
    status = AddMethodHandlers(methodEntries, sizeof(methodEntries) / sizeof(methodEntries[0]));
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to register method handlers for StreamObject"));
    }
}

StreamObject::~StreamObject() {
    delete mAbout;
    if (mAudioSinkObjectPath != NULL) {
        free((void*)mAudioSinkObjectPath);
    }
    if (mImageSinkObjectPath != NULL) {
        free((void*)mImageSinkObjectPath);
    }
    if (mMetadataSinkObjectPath != NULL) {
        free((void*)mMetadataSinkObjectPath);
    }

    mPortsMutex->Lock();
    for (std::vector<PortObject*>::iterator it = mPorts.begin(); it != mPorts.end(); ++it) {
        PortObject* p = *it;
        bus->UnregisterBusObject(*p);
        p->Cleanup();
        delete p;
    }
    mPorts.clear();
    mPortsMutex->Unlock();
    delete mPortsMutex;
}

QStatus StreamObject::Register(BusAttachment* bus) {
    mAbout->Register(mSessionPort);
    bus->RegisterBusObject(*mAbout);

    bus->RegisterBusObject(*this);
    vector<String> intfNames;
    intfNames.push_back(STREAM_INTERFACE);
    intfNames.push_back(CLOCK_INTERFACE);
    mAbout->AddObjectDescription(this->GetPath(), intfNames);

    mPortsMutex->Lock();

    mPorts.push_back(new AudioSinkObject(bus, mAudioSinkObjectPath, this, mAudioDevice));
    bus->RegisterBusObject(*mPorts.back());
    intfNames.clear();
    intfNames.push_back(PORT_INTERFACE);
    intfNames.push_back(AUDIO_SINK_INTERFACE);
    intfNames.push_back(VOLUME_INTERFACE);
    mAbout->AddObjectDescription(mPorts.back()->GetPath(), intfNames);

    mPorts.push_back(new ImageSinkObject(bus, mImageSinkObjectPath, this));
    bus->RegisterBusObject(*mPorts.back());
    intfNames.clear();
    intfNames.push_back(PORT_INTERFACE);
    intfNames.push_back(IMAGE_SINK_INTERFACE);
    mAbout->AddObjectDescription(mPorts.back()->GetPath(), intfNames);

    mPorts.push_back(new MetadataSinkObject(bus, mMetadataSinkObjectPath, this));
    bus->RegisterBusObject(*mPorts.back());
    intfNames.clear();
    intfNames.push_back(PORT_INTERFACE);
    intfNames.push_back(METADATA_SINK_INTERFACE);
    mAbout->AddObjectDescription(mPorts.back()->GetPath(), intfNames);

    mPortsMutex->Unlock();

    QStatus status = mAbout->Announce();
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to announce"));
    }

    return status;
}

void StreamObject::Unregister() {
    mAbout->Unregister();

    mPortsMutex->Lock();
    for (std::vector<PortObject*>::iterator it = mPorts.begin(); it != mPorts.end(); ++it) {
        PortObject* p = *it;
        if (mOwner != NULL) {
            QCC_DbgHLPrintf(("Unregistering existing ports owned by \"%s\"", mOwner));
            p->EmitOwnershipLostSignal(mOwner);
        }
        bus->UnregisterBusObject(*p);
        p->Cleanup();
        delete p;
    }
    mPorts.clear();
    mPortsMutex->Unlock();

    free((void*)mOwner);
    mOwner = NULL;

    bus->UnregisterBusObject(*this);
}

QStatus StreamObject::Get(const char* ifcName, const char* propName, MsgArg& val) {
    QStatus status = ER_OK;

    QCC_DbgTrace(("GetProperty called for %s.%s", ifcName, propName));

    if (0 == strcmp(ifcName, STREAM_INTERFACE)) {
        if (0 == strcmp(propName, "Version")) {
            val.Set("q", INTERFACES_VERSION);
        } else {
            status = ER_BUS_NO_SUCH_PROPERTY;
        }
    } else if (0 == strcmp(ifcName, CLOCK_INTERFACE)) {
        if (0 == strcmp(propName, "Version")) {
            val.Set("q", INTERFACES_VERSION);
        } else {
            status = ER_BUS_NO_SUCH_PROPERTY;
        }
    } else {
        status = ER_BUS_NO_SUCH_INTERFACE;
    }

    if (status == ER_BUS_NO_SUCH_INTERFACE || status == ER_BUS_NO_SUCH_PROPERTY) {
        return BusObject::Get(ifcName, propName, val);
    }

    return status;
}

void StreamObject::Open(const InterfaceDescription::Member* member, Message& msg) {
    GET_ARGS(0);

    if (mOwner != NULL && strcmp(mOwner, msg->GetSender()) == 0) {
        QCC_DbgHLPrintf(("Open method called by existing owner"));
        REPLY(ER_OPEN_FAILED);
        return;
    }

    mPortsMutex->Lock();
    for (std::vector<PortObject*>::iterator it = mPorts.begin(); it != mPorts.end(); ++it) {
        PortObject* p = *it;
        if (mOwner != NULL) {
            QCC_DbgHLPrintf(("Unregistering existing ports owned by \"%s\"", mOwner));
            p->EmitOwnershipLostSignal(msg->GetSender());
        }
        p->Cleanup();
    }
    mPortsMutex->Unlock();

    uint32_t oldSessionId = 0;
    if (mOwner != NULL) {
        oldSessionId = mSessionId;
        free((void*)mOwner);
    }

    mOwner = strdup(msg->GetSender());
    mSessionId = msg->GetSessionId();

    QCC_DbgHLPrintf(("Opened ports for owner=\"%s\" sessionId=%u", mOwner, mSessionId));

    REPLY_OK();

    if (oldSessionId) {
        bus->EnableConcurrentCallbacks();
        bus->LeaveSession(oldSessionId);
    }
}

void StreamObject::Close(const InterfaceDescription::Member* member, Message& msg) {
    GET_ARGS(0);

    if (mOwner == NULL) {
        QCC_DbgHLPrintf(("Close method called but stream is not open"));
        REPLY(ER_FAIL);
        return;
    }

    if (0 != strcmp(mOwner, msg->GetSender())) {
        QCC_DbgHLPrintf(("Disallowing \"%s\" from closing stream owned by \"%s\"",
                         msg->GetSender(), mOwner));
        REPLY(ER_FAIL);
        return;
    }

    QCC_DbgHLPrintf(("Closing ports for owner=\"%s\" sessionId=%u", mOwner, mSessionId));

    mPortsMutex->Lock();
    for (std::vector<PortObject*>::iterator it = mPorts.begin(); it != mPorts.end(); ++it) {
        PortObject* p = *it;
        p->Cleanup(true);
    }
    mPortsMutex->Unlock();

    free((void*)mOwner);
    mOwner = NULL;

    REPLY_OK();
}

void StreamObject::Close() {
    if (mOwner == NULL) {
        QCC_DbgHLPrintf(("Close method called but stream is not open"));
        return;
    }

    mPortsMutex->Lock();
    for (std::vector<PortObject*>::iterator it = mPorts.begin(); it != mPorts.end(); ++it) {
        PortObject* p = *it;
        p->EmitOwnershipLostSignal("");
        p->Cleanup();
    }
    mPortsMutex->Unlock();

    uint32_t oldSessionId = mSessionId;
    mSessionId = 0;
    free((void*)mOwner);
    mOwner = NULL;

    if (oldSessionId) {
        bus->LeaveSession(oldSessionId);
    }
}

void StreamObject::SetTime(const InterfaceDescription::Member* member, Message& msg) {
    GET_ARGS(1);

    struct timespec time;
    const uint64_t t = args[0].v_uint64;
    time.tv_sec = t / 1000000000;
    time.tv_nsec = t % 1000000000;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    struct timespec adj;
    if ((time.tv_nsec - now.tv_nsec) < 0) {
        adj.tv_sec = time.tv_sec - now.tv_sec - 1;
        adj.tv_nsec = 1000000000 + time.tv_nsec - now.tv_nsec;
    } else {
        adj.tv_sec = time.tv_sec - now.tv_sec;
        adj.tv_nsec = time.tv_nsec - now.tv_nsec;
    }

    mClockAdjustment = ((int64_t)adj.tv_sec * 1000000000) + adj.tv_nsec;
    QCC_DbgHLPrintf(("Clock adjustment is %" PRId64, mClockAdjustment));
    REPLY_OK();
}

void StreamObject::AdjustTime(const InterfaceDescription::Member* member, Message& msg) {
    GET_ARGS(1);

    mClockAdjustment += args[0].v_int64;
    QCC_DbgHLPrintf(("Clock adjustment is %" PRId64, mClockAdjustment));
    REPLY_OK();
}

uint64_t StreamObject::GetCurrentTimeNanos() {
    return ajn::services::GetCurrentTimeNanos() + mClockAdjustment;
}

void StreamObject::SleepUntilTimeNanos(uint64_t timeNanos) {
    uint64_t now = GetCurrentTimeNanos();
    if (timeNanos > now) {
        SleepNanos(timeNanos - now);
    }
}

}
}
