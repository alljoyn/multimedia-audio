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

#include "AudioTest.h"
#include "TestSignalHandler.h"

#include "Sink.h"
#include "gtest/gtest.h"
#include <math.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

using namespace ajn::services;
using namespace ajn;
using namespace qcc;
using namespace std;

class VolumeControlTest : public testing::Test {

  private:
    class TestSessionListener : public SessionListener {
      private:
        VolumeControlTest* volumeTest;

      public:
        TestSessionListener(VolumeControlTest* test) {
            volumeTest = test;
        }

        void SessionLost(SessionId sessionId) {
            volumeTest->ResetSession(sessionId);
        }
    };

  protected:

    BusAttachment* mMsgBus;
    bool mJoinComplete;
    char* mServiceName;
    SessionId mSessionId;
    char* mStreamObjectPath;
    volatile sig_atomic_t mInterrupt;
    TestSessionListener* mSessionListener;
    const char* connectArgs;
    TestSignalHandler* signalHandler;

    virtual void SetUp() {

        mMsgBus = NULL;
        mJoinComplete = false;
        mServiceName = NULL;
        mSessionId = 0;
        mStreamObjectPath = NULL;
        mInterrupt = false;
        connectArgs = NULL;
        mSessionListener = new TestSessionListener(this);
        signalHandler = NULL;

        InitStream();
    }

    virtual void TearDown() {

        if (mServiceName) {
            mMsgBus->ReleaseName(mServiceName);
        }

        if (mMsgBus->IsConnected()) {
            mMsgBus->Disconnect(connectArgs);
        }

        delete mMsgBus;
        mMsgBus = NULL;

        delete signalHandler;
        signalHandler = NULL;
    }

    void InitStream() {

        QStatus status = ER_OK;

        connectArgs = getenv("BUS_ADDRESS");
        if (connectArgs == NULL) {
            connectArgs = "unix:abstract=alljoyn";
        }

        mMsgBus = new BusAttachment("VolumeControlTest", true);
        status = mMsgBus->CreateInterfacesFromXml(INTERFACES_XML);
        ASSERT_EQ(status, ER_OK);

        /* Start the msg bus */
        status = mMsgBus->Start();
        ASSERT_EQ(status, ER_OK);

        /* Create the client-side endpoint */
        status = mMsgBus->Connect(connectArgs);
        ASSERT_EQ(status, ER_OK);

        const char* serviceName = AudioTest::sServiceName;
        ASSERT_TRUE(serviceName);

        const char* objectPath = AudioTest::sStreamObjectPath;
        ASSERT_TRUE(objectPath);

        SessionPort sessionPort = AudioTest::sSessionPort;
        ASSERT_NE(sessionPort, 0);

        JoinSession(serviceName, sessionPort, objectPath);
        ASSERT_TRUE(mJoinComplete);
        ASSERT_TRUE(!mInterrupt);
    }

    void JoinSession(const char* name, SessionPort port, const char* path) {

        if (!mJoinComplete) {

            SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
            QStatus status = mMsgBus->JoinSession(name, port, mSessionListener, mSessionId, opts);
            if (status != ER_OK) {
                printf("JoinSession failed (status=%s)\n", QCC_StatusText(status));
                mInterrupt = true;
            } else {
                mServiceName = strdup(name);
                mStreamObjectPath = strdup(path);
                mJoinComplete = true;
            }
        }
    }

    void ResetSession(SessionId sessionId) {

        if (mSessionId == sessionId) {
            mSessionId = 0;
            mInterrupt = true;
        }
    }

    QStatus OpenStream(ProxyBusObject* stream) {

        /* Open the stream */
        Message openReply(*mMsgBus);
        QStatus status = stream->MethodCall(STREAM_INTERFACE, "Open", NULL, 0, openReply);
        //printf("OpenStream (status=%s)\n", QCC_StatusText(status));
        return status;
    }

    QStatus CloseStream(ProxyBusObject* stream) {

        Message closeReply(*mMsgBus);
        QStatus status = stream->MethodCall(STREAM_INTERFACE, "Close", NULL, 0, closeReply);
        //printf("CloseStream (status=%s, reply=%s)\n", QCC_StatusText(status), closeReply->ToString().data());
        return status;
    }

    ProxyBusObject* CreateStream() {

        ProxyBusObject* stream = new ProxyBusObject(*mMsgBus, mServiceName, mStreamObjectPath, mSessionId);
        const InterfaceDescription* streamIntf = mMsgBus->GetInterface(STREAM_INTERFACE);
        stream->AddInterface(*streamIntf);
        return stream;
    }

    ProxyBusObject* GetPort(ProxyBusObject* stream) {
        QStatus status = stream->IntrospectRemoteObject();
        if (status != ER_OK) {
            //fprintf(stderr, "IntrospectRemoteObject(stream) failed: %s\n", QCC_StatusText(status));
            return NULL;
        }

        size_t nChildren = stream->GetChildren(NULL);
        if (nChildren == 0) {
            //fprintf(stderr, "Stream does not have any child objects\n");
            return NULL;
        }

        ProxyBusObject** children = new ProxyBusObject *[nChildren];
        if (stream->GetChildren(children, nChildren) != nChildren) {
            //fprintf(stderr, "Stream returned bad number of children\n");
            delete [] children;
            return NULL;
        }

        ProxyBusObject* port = NULL;

        for (size_t i = 0; i < nChildren; i++) {
            ProxyBusObject* child = children[i];

            status = child->IntrospectRemoteObject();
            if (status != ER_OK) {
                //fprintf(stderr, "IntrospectRemoteObject(child) failed: %s\n", QCC_StatusText(status));
                continue;
            }

            if (child->ImplementsInterface(AUDIO_SINK_INTERFACE)) {
                port = child;
                break;
            }
        }

        delete [] children;

        return port;
    }

    ProxyBusObject* GetRawPort(ProxyBusObject* stream) {
        ProxyBusObject* port = GetPort(stream);
        if (port == NULL) {
            return NULL;
        }

        Capability capability;
        SetRawCapability(capability, 2, 44100);
        MsgArg connectArgs[3];
        connectArgs[0].Set("s", ""); // host
        connectArgs[1].Set("o", "/"); // path
        CAPABILITY_TO_MSGARG(capability, connectArgs[2]);
        Message reply(*mMsgBus);
        QStatus status = port->MethodCall(PORT_INTERFACE, "Connect", connectArgs, 3, reply);
        EXPECT_EQ(status, ER_OK);
        RegisterSignalHandler(port->GetPath().c_str());
        return port;
    }

    void SetRawCapability(Capability& capability, uint8_t channels, uint32_t sampleRate, const char* format = "s16le") {

        MsgArg* channelsArg = new MsgArg("y", channels);
        MsgArg* rateArg = new MsgArg("q", sampleRate);
        MsgArg* formatArg = new MsgArg("s", format);
        capability.type = MIMETYPE_AUDIO_RAW;
        capability.numParameters = 3;
        capability.parameters = new MsgArg[capability.numParameters];
        capability.parameters[0].Set("{sv}", "Channels", channelsArg);
        capability.parameters[1].Set("{sv}", "Rate", rateArg);
        capability.parameters[2].Set("{sv}", "Format", formatArg);
    }

    void RegisterSignalHandler(const char* path) {

        signalHandler = new TestSignalHandler(mMsgBus, path, mSessionId);
        signalHandler->RegisterHandlers();
        mMsgBus->RegisterBusObject(*signalHandler);
        while (mInterrupt == false) {
            if (signalHandler->WaitUntilReadyToEmit(100) == ER_OK) {
                break;
            }
        }
    }

    QStatus GetMute(ProxyBusObject* port, bool& mute) {

        MsgArg reply;
        QStatus status = port->GetProperty(VOLUME_INTERFACE, "Mute", reply);
        if (status != ER_OK) { return status; }
        return reply.Get("b", &mute);
    }

    bool GetMuteChange() {
        return signalHandler->GetMute();
    }

    QStatus SetMute(ProxyBusObject* port, bool mute) {

        MsgArg arg;
        arg.Set("b", mute);
        return port->SetProperty(VOLUME_INTERFACE, "Mute", arg);
    }

    QStatus WaitForMuteChanged(uint32_t timeoutMs) {

        printf("\t     Waiting for MuteChanged event...\n");
        return signalHandler->WaitForMuteChanged(timeoutMs);
    }

    QStatus GetVolumeRange(ProxyBusObject* port, int16_t& low, int16_t& high, int16_t& step) {
        MsgArg reply;
        QStatus status = port->GetProperty(VOLUME_INTERFACE, "VolumeRange", reply);
        if (status != ER_OK) { return status; }
        return reply.Get("(nnn)", &low, &high, &step);
    }

    QStatus GetVolume(ProxyBusObject* port, int16_t& volume) {

        MsgArg reply;
        QStatus status = port->GetProperty(VOLUME_INTERFACE, "Volume", reply);
        if (status != ER_OK) { return status; }
        return reply.Get("n", &volume);
    }

    int16_t GetVolumeChange() {
        return signalHandler->GetVolume();
    }

    QStatus SetVolume(ProxyBusObject* port, int16_t volume) {

        MsgArg arg;
        arg.Set("n", volume);
        return port->SetProperty(VOLUME_INTERFACE, "Volume", arg);
    }

    QStatus AdjustVolumePercent(ProxyBusObject* port, double change) {
        Message msg(*mMsgBus);
        MsgArg arg;
        arg.Set("d", change);
        return port->MethodCall(VOLUME_INTERFACE, "AdjustVolumePercent", &arg, 1, msg);
    }

    QStatus WaitForVolumeChanged(uint32_t timeoutMs) {

        printf("\t     Waiting for VolumeChanged event...\n");
        return signalHandler->WaitForVolumeChanged(timeoutMs);
    }

    QStatus AdjustVolume(ProxyBusObject* port, int16_t delta) {

        Message reply(*mMsgBus);
        MsgArg args("n", delta);
        return port->MethodCall(VOLUME_INTERFACE, "AdjustVolume", &args, 1, reply);
    }
};

TEST_F(VolumeControlTest, MuteUnmute) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(OpenStream(stream), ER_OK);
    ProxyBusObject* port = GetRawPort(stream);
    ASSERT_TRUE(port);

    bool mute = false;
    EXPECT_EQ(GetMute(port, mute), ER_OK);

    EXPECT_EQ(SetMute(port, !mute), ER_OK);
    EXPECT_EQ(WaitForMuteChanged(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(GetMuteChange(), !mute);

    EXPECT_EQ(SetMute(port, mute), ER_OK);
    EXPECT_EQ(WaitForMuteChanged(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(GetMuteChange(), mute);

    delete stream;
}

TEST_F(VolumeControlTest, SetValidVolume) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(ER_OK, OpenStream(stream));
    ProxyBusObject* port = GetRawPort(stream);
    ASSERT_TRUE(port);
    int16_t low, high, step;
    EXPECT_EQ(ER_OK, GetVolumeRange(port, low, high, step));
    int16_t currentVol;
    EXPECT_EQ(ER_OK, GetVolume(port, currentVol));

    if (currentVol < high) {
        int16_t newVol1 = currentVol + 1;
        EXPECT_EQ(ER_OK, SetVolume(port, newVol1));
        EXPECT_EQ(ER_OK, WaitForVolumeChanged(AudioTest::sTimeout));
        EXPECT_NEAR(newVol1, GetVolumeChange(), 2);
    }
    if (currentVol > low) {
        int16_t newVol2 = currentVol - 1;
        EXPECT_EQ(ER_OK, SetVolume(port, newVol2));
        EXPECT_EQ(ER_OK, WaitForVolumeChanged(AudioTest::sTimeout));
        EXPECT_NEAR(newVol2, GetVolumeChange(), 2);
    }
    delete stream;
}

TEST_F(VolumeControlTest, SetInvalidVolume) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(ER_OK, OpenStream(stream));
    ProxyBusObject* port = GetRawPort(stream);
    ASSERT_TRUE(port);
    int16_t low, high, step;
    EXPECT_EQ(GetVolumeRange(port, low, high, step), ER_OK);

    if (high != INT16_MAX) {
        EXPECT_NE(ER_OK, SetVolume(port, INT16_MAX));
    } else if (low != INT16_MIN) {
        EXPECT_NE(ER_OK, SetVolume(port, INT16_MIN));
    }

    delete stream;
}

TEST_F(VolumeControlTest, IndependenceOfMuteAndVolume) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(ER_OK, OpenStream(stream));
    ProxyBusObject* port = GetRawPort(stream);
    ASSERT_TRUE(port);
    int16_t low, high, step;
    EXPECT_EQ(GetVolumeRange(port, low, high, step), ER_OK);

    int16_t volume = ((high - low) / 2) + low, currentVol = low, newVol = low + 1;
    EXPECT_EQ(ER_OK, SetVolume(port, volume));
    EXPECT_EQ(ER_OK, GetVolume(port, currentVol));
    EXPECT_EQ(volume, currentVol);

    bool mute = false;
    EXPECT_EQ(ER_OK, SetMute(port, true));
    EXPECT_EQ(ER_OK, GetMute(port, mute));
    EXPECT_EQ(true, mute);

    currentVol = low;
    EXPECT_EQ(ER_OK, GetVolume(port, currentVol));
    EXPECT_EQ(volume, currentVol);

    EXPECT_EQ(ER_OK, SetMute(port, false));
    EXPECT_EQ(ER_OK, GetMute(port, mute));
    EXPECT_FALSE(mute);

    currentVol = low;
    EXPECT_EQ(ER_OK, GetVolume(port, currentVol));
    EXPECT_EQ(volume, currentVol);

    EXPECT_EQ(ER_OK, SetVolume(port, newVol));
    EXPECT_EQ(ER_OK, GetVolume(port, currentVol));
    EXPECT_EQ(newVol, currentVol);

    EXPECT_EQ(ER_OK, SetMute(port, true));
    EXPECT_EQ(ER_OK, GetMute(port, mute));
    EXPECT_TRUE(mute);

    currentVol = low;
    EXPECT_EQ(ER_OK, GetVolume(port, currentVol));
    EXPECT_EQ(newVol, currentVol);

    delete stream;
}

TEST_F(VolumeControlTest, AdjustVolume) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(ER_OK, OpenStream(stream));
    ProxyBusObject* port = GetRawPort(stream);
    ASSERT_TRUE(port);
    int16_t low, high, step;
    EXPECT_EQ(GetVolumeRange(port, low, high, step), ER_OK);

    int16_t volume;
    int16_t delta = 2;

    EXPECT_EQ(ER_OK, GetVolume(port, volume));
    int16_t adjustment = (volume + delta < high) ? delta : -delta;
    EXPECT_EQ(ER_OK, AdjustVolume(port, adjustment));
    EXPECT_EQ(ER_OK, WaitForVolumeChanged(AudioTest::sTimeout));
    EXPECT_NEAR(volume + adjustment, GetVolumeChange(), 2 * delta);

    EXPECT_EQ(ER_OK, AdjustVolume(port, delta * -1));
    EXPECT_EQ(ER_OK, WaitForVolumeChanged(AudioTest::sTimeout));
    EXPECT_NEAR(volume, GetVolumeChange(), 2 * delta);

    delete stream;
}

TEST_F(VolumeControlTest, AdjustInvalidVolume) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(ER_OK, OpenStream(stream));
    ProxyBusObject* port = GetRawPort(stream);
    ASSERT_TRUE(port);
    int16_t low, high, step;
    EXPECT_EQ(GetVolumeRange(port, low, high, step), ER_OK);

    if (low != INT16_MIN && high != INT16_MAX) {
        int16_t delta = high - low + 1;
        EXPECT_NE(ER_OK, AdjustVolume(port, delta));
        EXPECT_NE(ER_OK, AdjustVolume(port, delta * -1));
    }
    delete stream;
}

TEST_F(VolumeControlTest, AdjustVolumePercent) {
    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(ER_OK, OpenStream(stream));
    ProxyBusObject* port = GetRawPort(stream);
    ASSERT_TRUE(port);
    int16_t low, high, step;
    EXPECT_EQ(GetVolumeRange(port, low, high, step), ER_OK);

    EXPECT_EQ(AdjustVolumePercent(port, 1.0), ER_OK);
    EXPECT_EQ(ER_OK, WaitForVolumeChanged(AudioTest::sTimeout));
    EXPECT_EQ(GetVolumeChange(), high);

    EXPECT_EQ(AdjustVolumePercent(port, -1.0), ER_OK);
    EXPECT_EQ(ER_OK, WaitForVolumeChanged(AudioTest::sTimeout));
    EXPECT_EQ(GetVolumeChange(), low);

    EXPECT_EQ(AdjustVolumePercent(port, 0.5), ER_OK);
    EXPECT_EQ(ER_OK, WaitForVolumeChanged(AudioTest::sTimeout));
    int16_t v50 = GetVolumeChange();
    EXPECT_NEAR(v50, low + floor((high - low) * 0.5), step);

    EXPECT_EQ(AdjustVolumePercent(port, 0.25), ER_OK);
    EXPECT_EQ(ER_OK, WaitForVolumeChanged(AudioTest::sTimeout));
    int16_t v75 = GetVolumeChange();
    EXPECT_NEAR(v75, v50 + floor((high - v50) * 0.25), step);

    EXPECT_EQ(AdjustVolumePercent(port, -0.13), ER_OK);
    EXPECT_EQ(ER_OK, WaitForVolumeChanged(AudioTest::sTimeout));
    EXPECT_NEAR(GetVolumeChange(), v75 + floor((v75 - low) * -0.13), step);
}

TEST_F(VolumeControlTest, VolumeEnabled) {
    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(ER_OK, OpenStream(stream));
    ProxyBusObject* port = GetRawPort(stream);
    ASSERT_TRUE(port);

    MsgArg reply;
    EXPECT_EQ(ER_OK, port->GetProperty(VOLUME_INTERFACE, "Enabled", reply));

    MsgArg arg;
    arg.Set("b", false);
    EXPECT_NE(ER_OK, port->SetProperty(VOLUME_INTERFACE, "Enabled", arg));
}
