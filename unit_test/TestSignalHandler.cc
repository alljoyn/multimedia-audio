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
#define __STDC_LIMIT_MACROS

#include "TestSignalHandler.h"

#include "Sink.h"
#include "gtest/gtest.h"

using namespace ajn;
using namespace qcc;
using namespace std;

class CriticalSection {
  private:
    static qcc::Mutex* sMutex;
  public:
    CriticalSection() { sMutex->Lock(); }
    ~CriticalSection() { sMutex->Unlock(); }
};

qcc::Mutex* CriticalSection::sMutex = new qcc::Mutex();

TestSignalHandler::TestSignalHandler(BusAttachment* bus, const char* path, SessionId sessionId) : BusObject(path) {

    mBus = bus;
    mReadyToEmitEvent = new Event();
    mPlayStateChangedEvent = new Event();
    mFifoPositionChangedEvent = new Event();
    mMuteChangedEvent = new Event();
    mVolumeChangedEvent = new Event();
    mOwnershipLostEvent = new Event();
    mPlayState = 0;
    mMute = false;
    mVolume = INT16_MIN;
    mSessionId = sessionId;
    mPortPath = path;
}

TestSignalHandler::~TestSignalHandler() {

    delete mReadyToEmitEvent;
    delete mPlayStateChangedEvent;
    delete mFifoPositionChangedEvent;
    delete mMuteChangedEvent;
    delete mVolumeChangedEvent;
    delete mOwnershipLostEvent;
}

void TestSignalHandler::RegisterHandlers() {

    const InterfaceDescription* portIntf = mBus->GetInterface(PORT_INTERFACE);
    ASSERT_TRUE(portIntf);

    const InterfaceDescription::Member* ownershipLostMember = portIntf->GetMember("OwnershipLost");
    ASSERT_TRUE(ownershipLostMember);
    QStatus status = mBus->RegisterSignalHandler(this,
                                                 static_cast<MessageReceiver::SignalHandler>(&TestSignalHandler::OwnershipLostSignalHandler),
                                                 ownershipLostMember, mPortPath);
    ASSERT_EQ(status, ER_OK);
    mMatches.push_back("type='signal',interface='org.alljoyn.Stream.Port',member='OwnershipLost'");

    const InterfaceDescription* sourceIntf = mBus->GetInterface(AUDIO_SOURCE_INTERFACE);
    ASSERT_TRUE(sourceIntf);
    mAudioDataMember = sourceIntf->GetMember("Data");
    ASSERT_TRUE(mAudioDataMember);

    const InterfaceDescription* imageSourceIntf = mBus->GetInterface(IMAGE_SOURCE_INTERFACE);
    ASSERT_TRUE(imageSourceIntf);
    mImageDataMember = imageSourceIntf->GetMember("Data");
    ASSERT_TRUE(mImageDataMember);

    const InterfaceDescription* metadataSourceIntf = mBus->GetInterface(METADATA_SOURCE_INTERFACE);
    ASSERT_TRUE(metadataSourceIntf);
    mMetaDataMember = metadataSourceIntf->GetMember("Data");
    ASSERT_TRUE(mMetaDataMember);

    const InterfaceDescription* sinkIntf = mBus->GetInterface(AUDIO_SINK_INTERFACE);
    ASSERT_TRUE(sinkIntf);
    AddInterface(*sinkIntf);

    const InterfaceDescription::Member* fifoPositionChangedMember = sinkIntf->GetMember("FifoPositionChanged");
    ASSERT_TRUE(fifoPositionChangedMember);
    status = mBus->RegisterSignalHandler(this,
                                         static_cast<MessageReceiver::SignalHandler>(&TestSignalHandler::FifoPositionChangedSignalHandler),
                                         fifoPositionChangedMember, mPortPath);
    ASSERT_EQ(status, ER_OK);
    mMatches.push_back("type='signal',interface='org.alljoyn.Stream.Port.AudioSink',member='FifoPositionChanged'");

    const InterfaceDescription::Member* playStateChangedMember = sinkIntf->GetMember("PlayStateChanged");
    ASSERT_TRUE(playStateChangedMember);
    status = mBus->RegisterSignalHandler(this,
                                         static_cast<MessageReceiver::SignalHandler>(&TestSignalHandler::PlayStateChangedSignalHandler),
                                         playStateChangedMember, mPortPath);
    ASSERT_EQ(status, ER_OK);
    mMatches.push_back("type='signal',interface='org.alljoyn.Stream.Port.AudioSink',member='PlayStateChanged'");

    const InterfaceDescription* volumeIntf = mBus->GetInterface(VOLUME_INTERFACE);
    ASSERT_TRUE(volumeIntf);
    AddInterface(*volumeIntf);

    const InterfaceDescription::Member* muteChangedMember = volumeIntf->GetMember("MuteChanged");
    ASSERT_TRUE(muteChangedMember);
    status = mBus->RegisterSignalHandler(this,
                                         static_cast<MessageReceiver::SignalHandler>(&TestSignalHandler::MuteChangedSignalHandler),
                                         muteChangedMember, NULL);
    ASSERT_EQ(status, ER_OK);
    mMatches.push_back("type='signal',interface='org.alljoyn.VolumeControl',member='MuteChanged'");

    const InterfaceDescription::Member* volumeChangedMember = volumeIntf->GetMember("VolumeChanged");
    ASSERT_TRUE(volumeChangedMember);
    status = mBus->RegisterSignalHandler(this,
                                         static_cast<MessageReceiver::SignalHandler>(&TestSignalHandler::VolumeChangedSignalHandler),
                                         volumeChangedMember, NULL);
    ASSERT_EQ(status, ER_OK);
    mMatches.push_back("type='signal',interface='org.alljoyn.VolumeControl',member='VolumeChanged'");
}

void TestSignalHandler::ObjectRegistered() {

    BusObject::ObjectRegistered();
    for (size_t i = 0; i < mMatches.size(); i++) {
        const char* match = mMatches.at(i);
        QStatus status = bus->AddMatch(match);
        ASSERT_EQ(status, ER_OK);
    }

    mReadyToEmitEvent->SetEvent();
}

void TestSignalHandler::ObjectUnregistered() {

    BusObject::ObjectUnregistered();
    bus->EnableConcurrentCallbacks();
    for (size_t i = 0; i < mMatches.size(); i++)
        bus->RemoveMatch(mMatches.at(i));
}

void TestSignalHandler::OwnershipLostSignalHandler(const InterfaceDescription::Member* member, const char* sourcePath, Message& msg) {
    size_t numArgs = 0;
    const MsgArg* args = NULL;
    msg->GetArgs(numArgs, args);
    ASSERT_TRUE(numArgs == 1);

    char* newOwner;
    args[0].Get("s", &newOwner);
    mNewOwner = newOwner;

    mOwnershipLostEvent->SetEvent();
}

void TestSignalHandler::FifoPositionChangedSignalHandler(const InterfaceDescription::Member* member, const char* sourcePath, Message& msg) {
    size_t numArgs = 0;
    const MsgArg* args = NULL;
    msg->GetArgs(numArgs, args);
    ASSERT_TRUE(numArgs == 0);

    mFifoPositionChangedEvent->SetEvent();
}

void TestSignalHandler::PlayStateChangedSignalHandler(const InterfaceDescription::Member* member, const char* sourcePath, Message& msg) {

    CriticalSection lock;
    size_t numArgs = 0;
    const MsgArg* args = NULL;
    msg->GetArgs(numArgs, args);
    ASSERT_TRUE(numArgs == 2);

    args[1].Get("y", &mPlayState);

    mPlayStateChangedEvent->SetEvent();
}

void TestSignalHandler::MuteChangedSignalHandler(const InterfaceDescription::Member* member, const char* sourcePath, Message& msg) {
    CriticalSection lock;
    size_t numArgs = 0;
    const MsgArg* args = NULL;
    msg->GetArgs(numArgs, args);
    ASSERT_TRUE(numArgs == 1);

    args[0].Get("b", &mMute);

    mMuteChangedEvent->SetEvent();
}

void TestSignalHandler::VolumeChangedSignalHandler(const InterfaceDescription::Member* member, const char* sourcePath, Message& msg) {
    CriticalSection lock;
    size_t numArgs = 0;
    const MsgArg* args = NULL;
    msg->GetArgs(numArgs, args);
    ASSERT_TRUE(numArgs == 1);

    args[0].Get("n", &mVolume);

    mVolumeChangedEvent->SetEvent();
}

uint8_t TestSignalHandler::GetPlayState() {

    CriticalSection lock;
    return mPlayState;
}

bool TestSignalHandler::GetMute() {
    CriticalSection lock;
    return mMute;
}

int16_t TestSignalHandler::GetVolume() {
    CriticalSection lock;
    return mVolume;
}

const char* TestSignalHandler::GetNewOwner() {
    CriticalSection lock;
    return mNewOwner.c_str();
}

QStatus TestSignalHandler::WaitUntilReadyToEmit(uint32_t waitMs) {

    QStatus status = ER_OK;
    if (!mReadyToEmitEvent->IsSet()) {
        status = Event::Wait(*mReadyToEmitEvent, waitMs);
    }
    mReadyToEmitEvent->ResetEvent();
    return status;
}

QStatus TestSignalHandler::WaitForPlayStateChanged(uint32_t waitMs) {

    QStatus status = ER_OK;
    if (!mPlayStateChangedEvent->IsSet()) {
        status = Event::Wait(*mPlayStateChangedEvent, waitMs);
    }
    mPlayStateChangedEvent->ResetEvent();
    return status;
}

QStatus TestSignalHandler::WaitForFifoPositionChanged(uint32_t waitMs) {

    QStatus status = ER_OK;
    if (!mFifoPositionChangedEvent->IsSet()) {
        status = Event::Wait(*mFifoPositionChangedEvent, waitMs);
    }
    mFifoPositionChangedEvent->ResetEvent();
    return status;
}

QStatus TestSignalHandler::WaitForOwnershipLost(uint32_t waitMs) {

    QStatus status = ER_OK;
    if (!mOwnershipLostEvent->IsSet()) {
        status = Event::Wait(*mOwnershipLostEvent, waitMs);
    }
    mOwnershipLostEvent->ResetEvent();
    return status;
}

QStatus TestSignalHandler::WaitForMuteChanged(uint32_t waitMs) {

    QStatus status = ER_OK;
    if (!mMuteChangedEvent->IsSet()) {
        status = Event::Wait(*mMuteChangedEvent, waitMs);
    }
    mMuteChangedEvent->ResetEvent();
    return status;
}

QStatus TestSignalHandler::WaitForVolumeChanged(uint32_t waitMs) {

    QStatus status = ER_OK;
    if (!mVolumeChangedEvent->IsSet()) {
        status = Event::Wait(*mVolumeChangedEvent, waitMs);
    }
    mVolumeChangedEvent->ResetEvent();
    return status;
}

QStatus TestSignalHandler::EmitAudioDataSignal(uint8_t* data, int32_t dataSize, uint64_t timestamp) {

    MsgArg args[2];
    args[0].Set("t", timestamp);
    args[1].Set("ay", dataSize, data);
    uint8_t flags = 0;
    return Signal(NULL, mSessionId, *mAudioDataMember, args, 2, 0, flags);
}

QStatus TestSignalHandler::EmitImageDataSignal(uint8_t* data, int32_t dataSize) {
    MsgArg arg("ay", dataSize, data);
    uint8_t flags = 0;
    return Signal(NULL, mSessionId, *mImageDataMember, &arg, 1, 0, flags);
}

QStatus TestSignalHandler::EmitMetaDataSignal(MsgArg* metadata) {
    uint8_t flags = 0;
    return Signal(NULL, mSessionId, *mMetaDataMember, metadata, 1, 0, flags);
}
