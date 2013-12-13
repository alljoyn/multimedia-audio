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

#include <alljoyn/BusAttachment.h>
#include <alljoyn/Status.h>
#include <qcc/Event.h>
#include <vector>

using namespace std;
using namespace qcc;
using namespace ajn;

class TestSignalHandler : public BusObject {

  private:
    BusAttachment* mBus;
    vector<const char*> mMatches;
    Event* mReadyToEmitEvent;
    Event* mPlayStateChangedEvent;
    Event* mFifoPositionChangedEvent;
    Event* mMuteChangedEvent;
    Event* mVolumeChangedEvent;
    Event* mOwnershipLostEvent;
    const InterfaceDescription::Member* mAudioDataMember;
    const InterfaceDescription::Member* mImageDataMember;
    const InterfaceDescription::Member* mMetaDataMember;
    uint8_t mPlayState;
    bool mMute;
    int16_t mVolume;
    String mNewOwner;
    SessionId mSessionId;
    const char* mPortPath;

  public:
    TestSignalHandler(BusAttachment* bus, const char* path, SessionId sessionId);
    ~TestSignalHandler();

    void RegisterHandlers();
    void ObjectRegistered();
    void ObjectUnregistered();
    void FifoPositionChangedSignalHandler(const InterfaceDescription::Member* member, const char* sourcePath, Message& msg);
    void PlayStateChangedSignalHandler(const InterfaceDescription::Member* member, const char* sourcePath, Message& msg);
    void MuteChangedSignalHandler(const InterfaceDescription::Member* member, const char* sourcePath, Message& msg);
    void VolumeChangedSignalHandler(const InterfaceDescription::Member* member, const char* sourcePath, Message& msg);
    void OwnershipLostSignalHandler(const InterfaceDescription::Member* member, const char* sourcePath, Message& msg);
    uint8_t GetPlayState();
    bool GetMute();
    int16_t GetVolume();
    const char* GetNewOwner();
    QStatus WaitUntilReadyToEmit(uint32_t waitMs);
    QStatus WaitForPlayStateChanged(uint32_t waitMs);
    QStatus WaitForFifoPositionChanged(uint32_t waitMs);
    QStatus WaitForMuteChanged(uint32_t waitMs);
    QStatus WaitForVolumeChanged(uint32_t waitMs);
    QStatus WaitForOwnershipLost(uint32_t waitMs);
    QStatus EmitAudioDataSignal(uint8_t* data, int32_t dataSize, uint64_t timestamp);
    QStatus EmitImageDataSignal(uint8_t* data, int32_t dataSize);
    QStatus EmitMetaDataSignal(MsgArg* metadata);
};
