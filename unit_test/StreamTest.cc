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

#include "AudioTest.h"
#include "TestSignalHandler.h"

#include "Clock.h"
#include "Sink.h"

using namespace ajn::services;
using namespace ajn;
using namespace qcc;
using namespace std;

static const uint32_t DEFAULT_SAMPLERATE = 44100;
static const uint8_t DEFAULT_CHANNELS = 2;
static const uint64_t SECOND_NANOS = 1000000000;

enum { IDLE = 0, PLAYING = 1, PAUSED = 2 };

class TestFixture {

  public:
    class TestSessionListener : public SessionListener {
      private:
        TestFixture* testFixture;

      public:
        TestSessionListener(TestFixture* fixture) {
            testFixture = fixture;
        }

        void SessionLost(SessionId sessionId) {
            testFixture->ResetSession(sessionId);
        }
    };

    BusAttachment* mMsgBus;
    bool mJoinComplete;
    char* mServiceName;
    SessionId mSessionId;
    char* mStreamObjectPath;
    volatile sig_atomic_t mInterrupt;
    TestSessionListener* mSessionListener;
    const char* connectArgs;
    TestSignalHandler* signalHandler;

    TestFixture() {

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

    ~TestFixture() {

        if (mServiceName) {
            mMsgBus->ReleaseName(mServiceName);
        }

        if (mMsgBus != NULL) {
            BusAttachment* deleteMe = mMsgBus;
            mMsgBus = NULL;
            delete deleteMe;
        }

        delete mSessionListener;
        mSessionListener = NULL;

        if (signalHandler != NULL) {
            delete signalHandler;
            signalHandler = NULL;
        }
    }

    const qcc::String GetUniqueName() {
        return mMsgBus->GetUniqueName();
    }

    void InitStream() {

        QStatus status = ER_OK;

        connectArgs = getenv("BUS_ADDRESS");
        if (connectArgs == NULL) {
            connectArgs = "unix:abstract=alljoyn";
        }

        mMsgBus = new BusAttachment("StreamTest", true);
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

        /* Joining session with existing sink service */
        JoinSession(serviceName, sessionPort, objectPath);
        ASSERT_TRUE(mJoinComplete);
        ASSERT_TRUE(!mInterrupt);
    }

    void JoinSession(const char* name, SessionPort port, const char* path) {

        //printf("StreamTest::JoinSession() (name=%s)\n", name);
        if (!mJoinComplete) {
            mServiceName = strdup(name);
            SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
            QStatus status = mMsgBus->JoinSession(mServiceName, port, mSessionListener, mSessionId, opts);
            if (status != ER_OK) {
                //printf("StreamTest::JoinSession() failed (status=%s)\n", QCC_StatusText(status));
                mInterrupt = true;
            } else {
                mStreamObjectPath = strdup(path);
                //printf("StreamTest::JoinSession() succeeded (path=%s)\n", mStreamObjectPath);
                //mMsgBus->CancelFindAdvertisedName(mServiceName);
                mJoinComplete = true;
            }
        }
    }

    void ResetSession(SessionId sessionId) {

        printf("StreamTest::ResetSession() (sessionId=%d)\n", sessionId);

        if (mSessionId == sessionId) {
            mSessionId = 0;
        }
    }

    QStatus OpenStream(ProxyBusObject* stream) {

        /* Open the stream */
        Message openReply(*mMsgBus);
        QStatus status = stream->MethodCall(STREAM_INTERFACE, "Open", NULL, 0, openReply);
        //printf("StreamTest::OpenStream() status=%s\n", QCC_StatusText(status));
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

    ProxyBusObject* GetPort(ProxyBusObject* stream, const char* portInterface = AUDIO_SINK_INTERFACE) {
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

            if (child->ImplementsInterface(portInterface)) {
                port = child;
                break;
            }
        }

        delete [] children;

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

    QStatus ConfigurePort(ProxyBusObject* port, Capability* capability) {
        Message reply(*mMsgBus);
        MsgArg connectArgs[3];
        connectArgs[0].Set("s", ""); // host
        connectArgs[1].Set("o", "/"); // path
        CAPABILITY_TO_MSGARG((*capability), connectArgs[2]);
        return port->MethodCall(PORT_INTERFACE, "Connect", connectArgs, 3, reply);
    }

    QStatus SetTime(ProxyBusObject* stream) {
        int64_t diffTime = 0;
        for (int i = 0; i < 5; i++) {
            uint64_t time = GetCurrentTimeNanos();
            MsgArg setTimeArgs[1];
            setTimeArgs[0].Set("t", time);
            Message setTimeReply(*mMsgBus);
            QStatus status = stream->MethodCall(CLOCK_INTERFACE, "SetTime", setTimeArgs, 1, setTimeReply);
            if (ER_OK != status) {
                return status;
            }
            uint64_t newTime = GetCurrentTimeNanos();
            diffTime = (newTime - time) / 2;
            if (diffTime < 10000000) {
                break;
            }
            SleepNanos(1000000000);
        }
        MsgArg adjustTimeArgs[1];
        adjustTimeArgs[0].Set("x", diffTime);
        Message adjustTimeReply(*mMsgBus);
        return stream->MethodCall(CLOCK_INTERFACE, "AdjustTime", adjustTimeArgs, 1, adjustTimeReply);
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

    QStatus SendSilentAudio(uint32_t totalLength, uint8_t channels, uint32_t sampleRate) {

        uint32_t bytesPerSecond = sampleRate * channels * 2; // Sample rate * channels * 16 bit per sample
        uint32_t bufferSize = 16384;
        uint32_t bytesEmitted = 0;
        uint8_t* buffer = (uint8_t*)calloc(bufferSize, 1);
        uint64_t timestamp = GetCurrentTimeNanos() + SECOND_NANOS; // play in 1 second

        while (bytesEmitted < totalLength) {
            uint32_t remainingBytes = bufferSize;
            if (bytesEmitted + remainingBytes > totalLength) {
                remainingBytes = totalLength - bytesEmitted;
            }
            QStatus status = signalHandler->EmitAudioDataSignal(buffer, remainingBytes, timestamp);
            if (status != ER_OK) { return status; }
            bytesEmitted += remainingBytes;
            timestamp += (uint64_t)((remainingBytes / bytesPerSecond) * SECOND_NANOS);
        }
        return ER_OK;
    }

    QStatus Play(ProxyBusObject* port) {

        Message reply(*mMsgBus);
        return port->MethodCall(AUDIO_SINK_INTERFACE, "Play", NULL, 0, reply);
    }

    QStatus Pause(ProxyBusObject* port) {

        Message reply(*mMsgBus);
        MsgArg pauseArgs("t", 0);
        return port->MethodCall(AUDIO_SINK_INTERFACE, "Pause", &pauseArgs, 1, reply);
    }

    QStatus Flush(ProxyBusObject* port) {

        Message reply(*mMsgBus);
        MsgArg args("t", 0);
        return port->MethodCall(AUDIO_SINK_INTERFACE, "Flush", &args, 1, reply);
    }

    QStatus WaitForPlayStateChanged(uint32_t timeoutMs) {

        printf("\t     Waiting for PlayStateChanged event...\n");
        uint32_t interval = 500;
        for (uint32_t i = 0; i < (timeoutMs / interval) && mInterrupt == false; i++) {
            if (signalHandler->WaitForPlayStateChanged(interval) == ER_OK) {
                return ER_OK;
            }
        }
        return ER_TIMEOUT;
    }

    QStatus GetPlayState(uint8_t& playState) {

        if (signalHandler != NULL) {
            playState = signalHandler->GetPlayState();
            return ER_OK;
        }
        return ER_FAIL;
    }

    QStatus WaitForFifoPositionChanged(uint32_t timeoutMs) {

        printf("\t     Waiting for FifoPositionChanged event...\n");
        uint32_t interval = 500;
        for (uint32_t i = 0; mInterrupt == false && i < (timeoutMs / interval); i++) {
            if (signalHandler->WaitForFifoPositionChanged(interval) == ER_OK) {
                return ER_OK;
            }
        }
        return ER_TIMEOUT;
    }

    QStatus GetFifoSize(ProxyBusObject* port, uint32_t& size) {

        size = 0;
        MsgArg reply;
        QStatus status = port->GetProperty(AUDIO_SINK_INTERFACE, "FifoSize", reply);
        if (status != ER_OK) { return status; }
        return reply.Get("u", &size);
    }

    QStatus WaitForOwnershipLost(uint32_t timeoutMs) {

        printf("\t     Waiting for OwnershipLost event...\n");
        uint32_t interval = 500;
        for (uint32_t i = 0; mInterrupt == false && i < (timeoutMs / interval); i++) {
            if (signalHandler->WaitForOwnershipLost(interval) == ER_OK) {
                return ER_OK;
            }
        }
        return ER_TIMEOUT;
    }

    QStatus GetNewOwner(String& newOwner) {

        if (signalHandler != NULL) {
            newOwner = signalHandler->GetNewOwner();
            return ER_OK;
        }
        return ER_FAIL;
    }
};

class StreamTest : public testing::Test {

  protected:

    TestFixture* mFixture;

    virtual void SetUp() { mFixture = new TestFixture(); }
    virtual void TearDown() { delete mFixture; }

    QStatus OpenStream(ProxyBusObject* stream) { return mFixture->OpenStream(stream); }
    QStatus CloseStream(ProxyBusObject* stream) { return mFixture->CloseStream(stream); }
    ProxyBusObject* CreateStream() { return mFixture->CreateStream(); }
    ProxyBusObject* GetPort(ProxyBusObject* stream, const char* portInterface = AUDIO_SINK_INTERFACE) {
        return mFixture->GetPort(stream, portInterface);
    }
    void SetRawCapability(Capability& capability, uint8_t channels, uint32_t sampleRate, const char* format = "s16le") {
        return mFixture->SetRawCapability(capability, channels, sampleRate, format);
    }
    QStatus ConfigurePort(ProxyBusObject* port, Capability* capability) { return mFixture->ConfigurePort(port, capability); }
    QStatus SetTime(ProxyBusObject* stream) { return mFixture->SetTime(stream); }
    void RegisterSignalHandler(const char* path) { return mFixture->RegisterSignalHandler(path); }
    QStatus SendSilentAudio(uint32_t totalLength, uint8_t channels, uint32_t sampleRate) {
        return mFixture->SendSilentAudio(totalLength, channels, sampleRate);
    }
    QStatus Play(ProxyBusObject* port) { return mFixture->Play(port); }
    QStatus Pause(ProxyBusObject* port) { return mFixture->Pause(port); }
    QStatus Flush(ProxyBusObject* port) { return mFixture->Flush(port); }
    QStatus WaitForPlayStateChanged(uint32_t timeoutMs) { return mFixture->WaitForPlayStateChanged(timeoutMs); }
    QStatus GetPlayState(uint8_t& playState) { return mFixture->GetPlayState(playState); }
    QStatus WaitForFifoPositionChanged(uint32_t timeoutMs) { return mFixture->WaitForFifoPositionChanged(timeoutMs); }
    QStatus GetFifoSize(ProxyBusObject* port, uint32_t& size) { return mFixture->GetFifoSize(port, size); }
    QStatus WaitForOwnershipLost(uint32_t timeoutMs) { return mFixture->WaitForOwnershipLost(timeoutMs); }
    QStatus GetNewOwner(String& newOwner) { return mFixture->GetNewOwner(newOwner); }
    QStatus EmitImageDataSignal(uint8_t* data, int32_t dataSize) {
        return mFixture->signalHandler->EmitImageDataSignal(data, dataSize);
    }
    QStatus EmitMetaDataSignal(MsgArg* metadata) {
        return mFixture->signalHandler->EmitMetaDataSignal(metadata);
    }

    QStatus GetInterfaceVersion(ProxyBusObject* object, const char* iface, uint16_t& version) {

        MsgArg value;
        QStatus status = object->GetProperty(iface, "Version", value);
        if (status == ER_OK) {
            status = value.Get("q", &version);
        }
        return status;
    }
};

TEST_F(StreamTest, OpenStream) {

    ProxyBusObject* stream = CreateStream();
    QStatus status = OpenStream(stream);
    EXPECT_EQ(status, ER_OK);

    delete stream;
}

TEST_F(StreamTest, CloseStream) {

    ProxyBusObject* stream = CreateStream();
    QStatus status = OpenStream(stream);
    EXPECT_EQ(status, ER_OK);

    status = CloseStream(stream);
    EXPECT_EQ(status, ER_OK);

    delete stream;
}

TEST_F(StreamTest, CloseUnopenedStream) {

    ProxyBusObject* stream = CreateStream();
    QStatus status = OpenStream(stream);
    EXPECT_EQ(status, ER_OK);

    status = CloseStream(stream);
    EXPECT_EQ(status, ER_OK);

    status = CloseStream(stream);
    EXPECT_EQ(status, ER_BUS_REPLY_IS_ERROR_MESSAGE);

    delete stream;
}

TEST_F(StreamTest, GetPort) {

    ProxyBusObject* stream = CreateStream();
    QStatus status = OpenStream(stream);
    EXPECT_EQ(status, ER_OK);

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    delete stream;
}

TEST_F(StreamTest, GetPortCapabilities) {

    ProxyBusObject* stream = CreateStream();
    QStatus status = OpenStream(stream);
    EXPECT_EQ(status, ER_OK);

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    MsgArg capabilitiesReply;
    status = port->GetProperty(PORT_INTERFACE, "Capabilities", capabilitiesReply);
    EXPECT_EQ(status, ER_OK);

    delete stream;
}

TEST_F(StreamTest, ConfigurePortWithSupportedRawCapability) {

    ProxyBusObject* stream = CreateStream();
    QStatus status = OpenStream(stream);
    EXPECT_EQ(status, ER_OK);

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    MsgArg capabilitiesReply;
    status = port->GetProperty(PORT_INTERFACE, "Capabilities", capabilitiesReply);
    EXPECT_EQ(status, ER_OK);

    if (status != ER_OK) {
        return;
    }

    size_t count;
    Capability* capabilities;
    Capability* rawCapability = NULL;
    MSGARG_TO_CAPABILITIES(capabilitiesReply, capabilities, count);
    for (size_t i = 0; i < count; i++) {
        if (capabilities[i].type == MIMETYPE_AUDIO_RAW) {
            rawCapability = &capabilities[i];
            break;
        }
    }
    EXPECT_TRUE(rawCapability != NULL);

    for (size_t i = 0; i < rawCapability->numParameters; i++) {
        MsgArg* p1 = rawCapability->parameters[i].v_dictEntry.val->v_variant.val;
        if (p1->typeId == ALLJOYN_BYTE_ARRAY && p1->v_scalarArray.numElements > 0) {
            *p1 = MsgArg("y", p1->v_scalarArray.v_byte[0]);
        } else if (p1->typeId == ALLJOYN_UINT16_ARRAY && p1->v_scalarArray.numElements > 0) {
            *p1 = MsgArg("q", p1->v_scalarArray.v_uint16[0]);
        } else if (p1->typeId == ALLJOYN_ARRAY && strcmp(p1->v_array.GetElemSig(), "s") == 0 && p1->v_array.GetNumElements() > 0) {
            const MsgArg* elements = p1->v_array.GetElements();
            *p1 = MsgArg("s", strdup(elements[0].v_string.str));
        }
    }

    status = ConfigurePort(port, rawCapability);
    EXPECT_EQ(status, ER_OK);

    delete stream;
}

TEST_F(StreamTest, ConfigurePortWithInvalidCapability) {

    ProxyBusObject* stream = CreateStream();
    QStatus status = OpenStream(stream);
    EXPECT_EQ(status, ER_OK);

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    Capability mp3Capability;
    mp3Capability.type = "audio/mp3";
    mp3Capability.numParameters = 0;

    status = ConfigurePort(port, &mp3Capability);
    EXPECT_NE(status, ER_OK);

    delete stream;
}

TEST_F(StreamTest, ConfigurePortWithInvalidRawCapability) {

    ProxyBusObject* stream = CreateStream();
    QStatus status = OpenStream(stream);
    EXPECT_EQ(status, ER_OK);

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    Capability invalidCapability;
    SetRawCapability(invalidCapability, 0, 0, "s32le");

    status = ConfigurePort(port, &invalidCapability);
    EXPECT_NE(status, ER_OK);

    delete stream;
}

TEST_F(StreamTest, ConfigurePortMoreThanOnce) {

    ProxyBusObject* stream = CreateStream();
    QStatus status = OpenStream(stream);
    EXPECT_EQ(status, ER_OK);

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    Capability capability;
    SetRawCapability(capability, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE);

    status = ConfigurePort(port, &capability);
    EXPECT_EQ(status, ER_OK);

    status = ConfigurePort(port, &capability);
    EXPECT_NE(status, ER_OK);

    delete stream;
}

TEST_F(StreamTest, ConfigurePortWithRequiredRawCapabilities) {

    int count = 4;
    for (int i = 1; i <= count; i++) {
        Capability cap;
        uint32_t sampleRate = (i % 2 == 0) ? 48000 : 44100;
        uint32_t channels = (i / ((count / 2) + 1)) + 1;
        printf("\t     PCM with Channels = %d, SampleRate = %d\n", channels, sampleRate);
        SetRawCapability(cap, channels, sampleRate);

        ProxyBusObject* stream = CreateStream();
        QStatus status = OpenStream(stream);
        EXPECT_EQ(status, ER_OK);
        ProxyBusObject* port = GetPort(stream);
        ASSERT_TRUE(port);
        status = ConfigurePort(port, &cap);
        EXPECT_EQ(status, ER_OK);
        status = CloseStream(stream);
        EXPECT_EQ(status, ER_OK);

        delete stream;
    }
}

TEST_F(StreamTest, TakeOwnership) {

    TestFixture* A = new TestFixture();
    ProxyBusObject* streamA = A->CreateStream();
    QStatus status = A->OpenStream(streamA);
    EXPECT_EQ(status, ER_OK);
    ProxyBusObject* portA = GetPort(streamA);
    ASSERT_TRUE(portA);
    A->RegisterSignalHandler(portA->GetPath().c_str());

    TestFixture* B = new TestFixture();
    ProxyBusObject* streamB = B->CreateStream();
    status = B->OpenStream(streamB);
    EXPECT_EQ(status, ER_OK);

    String newOwner;
    EXPECT_EQ(A->WaitForOwnershipLost(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(A->GetNewOwner(newOwner), ER_OK);
    EXPECT_STREQ(newOwner.c_str(), B->GetUniqueName().c_str());

    delete streamB;
    delete B;
    delete streamA;
    delete A;
}

TEST_F(StreamTest, PlaybackStarts) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(OpenStream(stream), ER_OK);

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    Capability capability;
    SetRawCapability(capability, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE);
    EXPECT_EQ(ConfigurePort(port, &capability), ER_OK);

    EXPECT_EQ(SetTime(stream), ER_OK);

    RegisterSignalHandler(port->GetPath().c_str());

    uint32_t fifoSize;
    GetFifoSize(port, fifoSize);

    uint8_t playState;
    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, IDLE);
    EXPECT_EQ(SendSilentAudio(fifoSize, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE), ER_OK);
    EXPECT_EQ(WaitForPlayStateChanged(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, PLAYING);

    delete stream;
}

TEST_F(StreamTest, FifoDrain) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(ER_OK, OpenStream(stream));

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    Capability capability;
    SetRawCapability(capability, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE);
    EXPECT_EQ(ER_OK, ConfigurePort(port, &capability));
    uint32_t fifoSize;
    EXPECT_EQ(ER_OK, GetFifoSize(port, fifoSize));
    RegisterSignalHandler(port->GetPath().c_str());

    uint8_t playState;
    EXPECT_EQ(ER_OK, GetPlayState(playState));
    EXPECT_EQ(IDLE, playState);
    EXPECT_EQ(ER_OK, SendSilentAudio(fifoSize, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE));

    EXPECT_EQ(ER_OK, WaitForPlayStateChanged(AudioTest::sTimeout));
    EXPECT_EQ(ER_OK, GetPlayState(playState));
    EXPECT_EQ(PLAYING, playState);

    EXPECT_EQ(ER_OK, WaitForFifoPositionChanged(AudioTest::sTimeout));

    EXPECT_EQ(ER_OK, WaitForPlayStateChanged(AudioTest::sTimeout));
    EXPECT_EQ(ER_OK, GetPlayState(playState));
    EXPECT_EQ(IDLE, playState);

    delete stream;
}

TEST_F(StreamTest, PlayPause) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(OpenStream(stream), ER_OK);

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    Capability capability;
    SetRawCapability(capability, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE);
    EXPECT_EQ(ConfigurePort(port, &capability), ER_OK);
    uint32_t fifoSize;
    EXPECT_EQ(GetFifoSize(port, fifoSize), ER_OK);
    RegisterSignalHandler(port->GetPath().c_str());

    uint8_t playState;
    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, IDLE);

    EXPECT_EQ(SendSilentAudio(fifoSize, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE), ER_OK);
    EXPECT_EQ(WaitForPlayStateChanged(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, PLAYING);

    EXPECT_EQ(Pause(port), ER_OK);
    EXPECT_EQ(WaitForPlayStateChanged(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, PAUSED);

    delete stream;
}

TEST_F(StreamTest, PlayPauseFlush) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(OpenStream(stream), ER_OK);

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    Capability capability;
    SetRawCapability(capability, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE);
    EXPECT_EQ(ConfigurePort(port, &capability), ER_OK);
    uint32_t fifoSize;
    EXPECT_EQ(GetFifoSize(port, fifoSize), ER_OK);
    RegisterSignalHandler(port->GetPath().c_str());

    uint8_t playState;
    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, IDLE);

    EXPECT_EQ(SendSilentAudio(fifoSize, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE), ER_OK);
    EXPECT_EQ(WaitForPlayStateChanged(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, PLAYING);

    EXPECT_EQ(Pause(port), ER_OK);
    EXPECT_EQ(WaitForPlayStateChanged(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, PAUSED);

    EXPECT_EQ(Flush(port), ER_OK);
    EXPECT_EQ(WaitForFifoPositionChanged(AudioTest::sTimeout), ER_OK);

    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, IDLE);

    delete stream;
}

TEST_F(StreamTest, PlayFlush) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(OpenStream(stream), ER_OK);

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    Capability capability;
    SetRawCapability(capability, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE);
    EXPECT_EQ(ConfigurePort(port, &capability), ER_OK);
    uint32_t fifoSize;
    EXPECT_EQ(GetFifoSize(port, fifoSize), ER_OK);
    RegisterSignalHandler(port->GetPath().c_str());

    uint8_t playState;
    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, IDLE);

    EXPECT_EQ(SendSilentAudio(fifoSize, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE), ER_OK);
    EXPECT_EQ(WaitForPlayStateChanged(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, PLAYING);

    EXPECT_EQ(Flush(port), ER_OK);
    EXPECT_EQ(WaitForFifoPositionChanged(AudioTest::sTimeout), ER_OK);

    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, IDLE);

    delete stream;
}

TEST_F(StreamTest, IdlePauseFill) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(OpenStream(stream), ER_OK);

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    Capability capability;
    SetRawCapability(capability, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE);
    EXPECT_EQ(ConfigurePort(port, &capability), ER_OK);
    uint32_t fifoSize;
    EXPECT_EQ(GetFifoSize(port, fifoSize), ER_OK);
    RegisterSignalHandler(port->GetPath().c_str());

    uint8_t playState;
    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, IDLE);

    EXPECT_EQ(Pause(port), ER_OK);
    EXPECT_EQ(WaitForPlayStateChanged(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, PAUSED);

    EXPECT_EQ(SendSilentAudio(fifoSize, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE), ER_OK);
    EXPECT_EQ(WaitForPlayStateChanged(AudioTest::sTimeout), ER_TIMEOUT);
    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, PAUSED);

    delete stream;
}

TEST_F(StreamTest, IdlePlay) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(OpenStream(stream), ER_OK);

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    Capability capability;
    SetRawCapability(capability, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE);
    EXPECT_EQ(ConfigurePort(port, &capability), ER_OK);
    uint32_t fifoSize;
    EXPECT_EQ(GetFifoSize(port, fifoSize), ER_OK);
    RegisterSignalHandler(port->GetPath().c_str());

    uint8_t playState;
    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, IDLE);

    EXPECT_EQ(Play(port), ER_OK);
    EXPECT_EQ(WaitForPlayStateChanged(AudioTest::sTimeout), ER_TIMEOUT);

    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, IDLE);

    delete stream;
}

TEST_F(StreamTest, IdleFlush) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(OpenStream(stream), ER_OK);

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    Capability capability;
    SetRawCapability(capability, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE);
    EXPECT_EQ(ConfigurePort(port, &capability), ER_OK);
    uint32_t fifoSize;
    EXPECT_EQ(GetFifoSize(port, fifoSize), ER_OK);
    RegisterSignalHandler(port->GetPath().c_str());

    uint8_t playState;
    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, IDLE);

    EXPECT_EQ(Flush(port), ER_OK);
    EXPECT_EQ(WaitForFifoPositionChanged(AudioTest::sTimeout), ER_OK);

    EXPECT_EQ(GetPlayState(playState), ER_OK);
    EXPECT_EQ(playState, IDLE);

    delete stream;
}

TEST_F(StreamTest, ImageTest) {
    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(ER_OK, OpenStream(stream));

    ProxyBusObject* port = GetPort(stream, IMAGE_SINK_INTERFACE);
    ASSERT_TRUE(port);

    Capability capability;
    capability.type = MIMETYPE_IMAGE_JPEG;
    EXPECT_EQ(ER_OK, ConfigurePort(port, &capability));

    RegisterSignalHandler(port->GetPath().c_str());

    uint8_t data[] = { };
    QStatus status = EmitImageDataSignal(data, sizeof(data));
    EXPECT_EQ(ER_OK, status);
}

TEST_F(StreamTest, MetadataTest) {
    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(ER_OK, OpenStream(stream));

    ProxyBusObject* port = GetPort(stream, METADATA_SINK_INTERFACE);
    ASSERT_TRUE(port);

    Capability capability;
    capability.type = MIMETYPE_METADATA;
    EXPECT_EQ(ER_OK, ConfigurePort(port, &capability));

    RegisterSignalHandler(port->GetPath().c_str());

    MsgArg name("s", "Beat and the Pulse");
    MsgArg artist("s", "Austra");
    MsgArg metadataEntries[2];
    metadataEntries[0].Set("{sv}", "Name", &name);
    metadataEntries[1].Set("{sv}", "Artist", &artist);
    MsgArg metadata("a{sv}", 2, &metadataEntries);

    QStatus status = EmitMetaDataSignal(&metadata);
    EXPECT_EQ(ER_OK, status);
}

TEST_F(StreamTest, GetInterfaceVersions) {
    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(ER_OK, OpenStream(stream));
    uint16_t version;
    EXPECT_EQ(ER_OK, GetInterfaceVersion(stream, STREAM_INTERFACE, version));
    EXPECT_GE(version, 1);
    EXPECT_EQ(ER_OK, GetInterfaceVersion(stream, CLOCK_INTERFACE, version));
    EXPECT_GE(version, 1);

    ProxyBusObject* port = GetPort(stream, AUDIO_SINK_INTERFACE);
    ASSERT_TRUE(port);
    EXPECT_EQ(ER_OK, GetInterfaceVersion(port, PORT_INTERFACE, version));
    EXPECT_GE(version, 1);
    EXPECT_EQ(ER_OK, GetInterfaceVersion(port, AUDIO_SINK_INTERFACE, version));
    EXPECT_GE(version, 1);
    EXPECT_EQ(ER_OK, GetInterfaceVersion(port, VOLUME_INTERFACE, version));
    EXPECT_GE(version, 1);

    port = GetPort(stream, IMAGE_SINK_INTERFACE);
    ASSERT_TRUE(port);
    EXPECT_EQ(ER_OK, GetInterfaceVersion(port, PORT_INTERFACE, version));
    EXPECT_GE(version, 1);
    EXPECT_EQ(ER_OK, GetInterfaceVersion(port, IMAGE_SINK_INTERFACE, version));
    EXPECT_GE(version, 1);

    port = GetPort(stream, METADATA_SINK_INTERFACE);
    ASSERT_TRUE(port);
    EXPECT_EQ(ER_OK, GetInterfaceVersion(port, PORT_INTERFACE, version));
    EXPECT_GE(version, 1);
    EXPECT_EQ(ER_OK, GetInterfaceVersion(port, METADATA_SINK_INTERFACE, version));
    EXPECT_GE(version, 1);
}

/*
 * There's currently no way to tell from the sender whether the sink
 * drops or drains its buffered data.  The Drop* and Drain* tests
 * below need to be manually verified by turning on debug tracing on
 * the sink and seeing that it drops or drains correctly.
 */
TEST_F(StreamTest, DropWhenIdle) {

    TestFixture* A = new TestFixture();
    ProxyBusObject* streamA = A->CreateStream();
    QStatus status = A->OpenStream(streamA);
    EXPECT_EQ(status, ER_OK);
    ProxyBusObject* portA = GetPort(streamA);
    ASSERT_TRUE(portA);

    Capability capability;
    A->SetRawCapability(capability, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE);
    EXPECT_EQ(ER_OK, A->ConfigurePort(portA, &capability));
    uint32_t fifoSize;
    EXPECT_EQ(ER_OK, A->GetFifoSize(portA, fifoSize));
    A->RegisterSignalHandler(portA->GetPath().c_str());

    uint8_t playState;
    EXPECT_EQ(ER_OK, A->GetPlayState(playState));
    EXPECT_EQ(IDLE, playState);

    TestFixture* B = new TestFixture();
    ProxyBusObject* streamB = B->CreateStream();
    status = B->OpenStream(streamB);
    EXPECT_EQ(status, ER_OK);

    String newOwner;
    EXPECT_EQ(A->WaitForOwnershipLost(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(A->GetNewOwner(newOwner), ER_OK);
    EXPECT_STREQ(newOwner.c_str(), B->GetUniqueName().c_str());

    delete streamB;
    delete B;
    delete streamA;
    delete A;
}

TEST_F(StreamTest, DrainWhenIdle) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(ER_OK, OpenStream(stream));

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    Capability capability;
    SetRawCapability(capability, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE);
    EXPECT_EQ(ER_OK, ConfigurePort(port, &capability));
    uint32_t fifoSize;
    EXPECT_EQ(ER_OK, GetFifoSize(port, fifoSize));
    RegisterSignalHandler(port->GetPath().c_str());

    uint8_t playState;
    EXPECT_EQ(ER_OK, GetPlayState(playState));
    EXPECT_EQ(IDLE, playState);

    QStatus status = CloseStream(stream);
    EXPECT_EQ(status, ER_OK);

    delete stream;
}

TEST_F(StreamTest, DropWhenPlaying) {

    TestFixture* A = new TestFixture();
    ProxyBusObject* streamA = A->CreateStream();
    QStatus status = A->OpenStream(streamA);
    EXPECT_EQ(status, ER_OK);
    ProxyBusObject* portA = GetPort(streamA);
    ASSERT_TRUE(portA);

    Capability capability;
    A->SetRawCapability(capability, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE);
    EXPECT_EQ(ER_OK, A->ConfigurePort(portA, &capability));
    uint32_t fifoSize;
    EXPECT_EQ(ER_OK, A->GetFifoSize(portA, fifoSize));
    A->RegisterSignalHandler(portA->GetPath().c_str());

    uint8_t playState;
    EXPECT_EQ(A->SendSilentAudio(fifoSize, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE), ER_OK);
    EXPECT_EQ(A->WaitForPlayStateChanged(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(ER_OK, A->GetPlayState(playState));
    EXPECT_EQ(PLAYING, playState);

    TestFixture* B = new TestFixture();
    ProxyBusObject* streamB = B->CreateStream();
    status = B->OpenStream(streamB);
    EXPECT_EQ(status, ER_OK);

    String newOwner;
    EXPECT_EQ(A->WaitForOwnershipLost(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(A->GetNewOwner(newOwner), ER_OK);
    EXPECT_STREQ(newOwner.c_str(), B->GetUniqueName().c_str());

    delete streamB;
    delete B;
    delete streamA;
    delete A;
}

TEST_F(StreamTest, DrainWhenPlaying) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(ER_OK, OpenStream(stream));

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    Capability capability;
    SetRawCapability(capability, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE);
    EXPECT_EQ(ER_OK, ConfigurePort(port, &capability));
    uint32_t fifoSize;
    EXPECT_EQ(ER_OK, GetFifoSize(port, fifoSize));
    RegisterSignalHandler(port->GetPath().c_str());

    uint8_t playState;
    EXPECT_EQ(SendSilentAudio(fifoSize, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE), ER_OK);
    EXPECT_EQ(WaitForPlayStateChanged(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(ER_OK, GetPlayState(playState));
    EXPECT_EQ(PLAYING, playState);

    QStatus status = CloseStream(stream);
    EXPECT_EQ(status, ER_OK);

    delete stream;
}

TEST_F(StreamTest, DropWhenPaused) {

    TestFixture* A = new TestFixture();
    ProxyBusObject* streamA = A->CreateStream();
    QStatus status = A->OpenStream(streamA);
    EXPECT_EQ(status, ER_OK);
    ProxyBusObject* portA = GetPort(streamA);
    ASSERT_TRUE(portA);

    Capability capability;
    A->SetRawCapability(capability, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE);
    EXPECT_EQ(ER_OK, A->ConfigurePort(portA, &capability));
    uint32_t fifoSize;
    EXPECT_EQ(ER_OK, A->GetFifoSize(portA, fifoSize));
    A->RegisterSignalHandler(portA->GetPath().c_str());

    uint8_t playState;
    EXPECT_EQ(A->SendSilentAudio(fifoSize, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE), ER_OK);
    EXPECT_EQ(A->WaitForPlayStateChanged(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(ER_OK, A->GetPlayState(playState));
    EXPECT_EQ(PLAYING, playState);

    EXPECT_EQ(A->Pause(portA), ER_OK);
    EXPECT_EQ(A->WaitForPlayStateChanged(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(ER_OK, A->GetPlayState(playState));
    EXPECT_EQ(PAUSED, playState);

    TestFixture* B = new TestFixture();
    ProxyBusObject* streamB = B->CreateStream();
    status = B->OpenStream(streamB);
    EXPECT_EQ(status, ER_OK);

    String newOwner;
    EXPECT_EQ(A->WaitForOwnershipLost(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(A->GetNewOwner(newOwner), ER_OK);
    EXPECT_STREQ(newOwner.c_str(), B->GetUniqueName().c_str());

    delete streamB;
    delete B;
    delete streamA;
    delete A;
}

TEST_F(StreamTest, DrainWhenPaused) {

    ProxyBusObject* stream = CreateStream();
    EXPECT_EQ(ER_OK, OpenStream(stream));

    ProxyBusObject* port = GetPort(stream);
    ASSERT_TRUE(port);

    Capability capability;
    SetRawCapability(capability, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE);
    EXPECT_EQ(ER_OK, ConfigurePort(port, &capability));
    uint32_t fifoSize;
    EXPECT_EQ(ER_OK, GetFifoSize(port, fifoSize));
    RegisterSignalHandler(port->GetPath().c_str());

    uint8_t playState;
    EXPECT_EQ(SendSilentAudio(fifoSize, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE), ER_OK);
    EXPECT_EQ(WaitForPlayStateChanged(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(ER_OK, GetPlayState(playState));
    EXPECT_EQ(PLAYING, playState);

    EXPECT_EQ(Pause(port), ER_OK);
    EXPECT_EQ(WaitForPlayStateChanged(AudioTest::sTimeout), ER_OK);
    EXPECT_EQ(ER_OK, GetPlayState(playState));
    EXPECT_EQ(PAUSED, playState);

    QStatus status = CloseStream(stream);
    EXPECT_EQ(status, ER_OK);

    delete stream;
}
