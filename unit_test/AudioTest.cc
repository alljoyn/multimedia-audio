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

#include "Sink.h"
#include <qcc/Mutex.h>
#include <map>

using namespace ajn;
using namespace qcc;
using namespace std;
using namespace testing;

class TestClientListener : public BusListener, public MessageReceiver {
  public:
    TestClientListener(AudioTest* test) {
        audioTest = test;
    }

    virtual void ListenerRegistered(BusAttachment* bus) {
        mBus = bus;
    }

    void OnAnnounce(const InterfaceDescription::Member* member, const char* srcPath, ajn::Message& message) {

        Service service;
        bool serviceAnnounced = false;

        /*
         * Filter for the interfaces we're interested in.
         */
        const char* serviceName = message->GetSender();

        size_t numObjectDescriptions = 0;
        MsgArg* objectDescriptions = NULL;
        QStatus status = message->GetArg(2)->Get("a(oas)", &numObjectDescriptions, &objectDescriptions);
        if (status != ER_OK) {
            fprintf(stderr, "Failed to get objectDescriptions: %s\n", QCC_StatusText(status));
        }
        for (size_t i = 0; i < numObjectDescriptions; ++i) {
            char* objectPath = NULL;
            size_t numInterfaces = 0;
            MsgArg* interfaces = NULL;
            status = objectDescriptions[i].Get("(oas)", &objectPath, &numInterfaces, &interfaces);
            if (status != ER_OK) {
                fprintf(stderr, "Failed to get interfaces: %s\n", QCC_StatusText(status));
            }
            for (size_t j = 0; j < numInterfaces; ++j) {
                char* intf;
                status = interfaces[j].Get("s", &intf);
                if (status != ER_OK) {
                    fprintf(stderr, "Failed to get interface: %s\n", QCC_StatusText(status));
                }

                if (!strcmp(intf, AUDIO_SINK_INTERFACE)) {
                    serviceAnnounced = true;
                } else if (!strcmp(intf, STREAM_INTERFACE)) {
                    service.path = objectPath;
                }
            }
        }

        if (!serviceAnnounced || service.path.empty()) {
            return;
        }

        /*
         * Extract extra information from the service metadata.
         */
        status = message->GetArg(1)->Get("q", &service.port);
        if (status != ER_OK) {
            fprintf(stderr, "Failed to get SessionPort: %s\n", QCC_StatusText(status));
        }

        /*
         * Now we've got all the info we need to join a session and begin doing useful work so cache it.
         */
        mServices.erase(serviceName);
        mServices.insert(pair<String, Service>(serviceName, service));

        /*
         * There are a number of strategies for tracking device presence.
         *
         * One is to leave outstanding find requests for each device of interest.  This is what is
         * below.  However, this can lead to false negatives if multicast traffic does not make it
         * through, and may take up to 40 seconds to find the device again after a false negative.
         *
         * Another is to force a refresh by cancelling any finds and re-issuing them.  The found
         * response should occur relative quickly after re-issue if the device is present.  See
         * Refresh below.
         */
        mBus->EnableConcurrentCallbacks();
        mBus->FindAdvertisedName(serviceName);
    }

    /*
     * Using first advertisement as test sink
     */
    virtual void FoundAdvertisedName(const char* name, TransportMask transport, const char* namePrefix) {
        audioTestLock.Lock();
        if (!audioTest->sReady) {
            map<String, Service>::iterator it = mServices.find(name);
            if (it != mServices.end()) {
                it->second.found = true;
                printf("\t     Found %s objectPath=%s, sessionPort=%d\n", name, it->second.path.c_str(), it->second.port);
                audioTest->sServiceName = strdup(name);
                audioTest->sSessionPort = it->second.port;
                audioTest->sStreamObjectPath = strdup(it->second.path.c_str());
                audioTest->sReady = true;
            }
        }
        audioTestLock.Unlock();
    }

    virtual void LostAdvertisedName(const char* name, TransportMask transport, const char* namePrefix) {
        audioTestLock.Lock();
        map<String, Service>::iterator it = mServices.find(name);
        if (it != mServices.end()) {
            it->second.found = false;
            printf("Lost %s\n", name);
            if (strcmp(audioTest->sServiceName, name) == 0) {
                audioTest->sReady = false;
            }
        }
        audioTestLock.Unlock();
    }

  private:
    struct Service {
        String path;
        uint16_t port;
        bool found;
        Service() : port(0), found(false) { }
    };
    map<String, Service> mServices;
    BusAttachment* mBus;
    AudioTest* audioTest;
    qcc::Mutex audioTestLock;
};

char* AudioTest::sServiceName = NULL;
char* AudioTest::sStreamObjectPath = NULL;
uint16_t AudioTest::sSessionPort = 0;
bool AudioTest::sReady = false;
uint32_t AudioTest::sTimeout = 5000;

AudioTest::AudioTest() {

    mListener = new TestClientListener(this);
}

AudioTest::~AudioTest() {

}

void AudioTest::SetUp() {

    QStatus status = ER_OK;

    const char* connectArgs = getenv("BUS_ADDRESS");
    if (connectArgs == NULL) {
        connectArgs = "unix:abstract=alljoyn";
    }

    mMsgBus = new BusAttachment("AudioTest", true);
    status = mMsgBus->CreateInterfacesFromXml(INTERFACES_XML);
    ASSERT_EQ(status, ER_OK);

    /* Start the msg bus */
    status = mMsgBus->Start();
    ASSERT_EQ(status, ER_OK);

    /* Create the client-side endpoint */
    status = mMsgBus->Connect(connectArgs);
    ASSERT_EQ(status, ER_OK);

    /* Register a bus listener */
    mMsgBus->RegisterBusListener(*mListener);

    status = mMsgBus->CreateInterfacesFromXml(ABOUT_XML);
    ASSERT_EQ(status, ER_OK);

    /* Begin discovery on the well-known name of the service to be called */
    status = mMsgBus->RegisterSignalHandler(mListener, static_cast<MessageReceiver::SignalHandler>(&TestClientListener::OnAnnounce),
                                            mMsgBus->GetInterface(ABOUT_INTERFACE)->GetMember("Announce"), 0);
    ASSERT_EQ(status, ER_OK);

    status = mMsgBus->AddMatch(ANNOUNCE_MATCH_RULE);
    ASSERT_EQ(status, ER_OK);

    printf("\t     Waiting for sink service advertisement...\n");
    while (!sReady) {
        usleep(100 * 1000);
    }
}

void AudioTest::TearDown() {

    if (sServiceName) {
        mMsgBus->ReleaseName(sServiceName);
    }

    if (mMsgBus != NULL) {
        mMsgBus->UnregisterBusListener(*mListener);
        mMsgBus->UnregisterAllHandlers(mListener);
        BusAttachment* deleteMe = mMsgBus;
        mMsgBus = NULL;
        delete deleteMe;
    }

    delete mListener;
}

/** Main entry point */
int main(int argc, char** argv, char** envArg)
{
    int status = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("\nRunning AudioTest unit test\n");

    InitGoogleTest(&argc, argv);
    AddGlobalTestEnvironment(new AudioTest);

    status = RUN_ALL_TESTS();
    printf("%s exiting with status %d \n", argv[0], status);
    return (int) status;
}



