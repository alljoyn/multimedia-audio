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

#include <alljoyn/audio/Audio.h>
#include <alljoyn/audio/StreamObject.h>
#if defined(QCC_OS_ANDROID)
#include <alljoyn/audio/android/AndroidDevice.h>
#elif defined(QCC_OS_GROUP_POSIX)
#include <alljoyn/audio/posix/ALSADevice.h>
#endif
#include <alljoyn/BusAttachment.h>
#include <alljoyn/Status.h>
#include <alljoyn/version.h>
#include <qcc/String.h>
#include <signal.h>
#include <stdio.h>
#include <sys/utsname.h>
#include <time.h>

using namespace ajn::services;
using namespace ajn;
using namespace qcc;

class AboutStore : public PropertyStore {
  public:
    AboutStore(const char* friendlyName) {
        struct utsname utsname;
        uname(&utsname);
        mDeviceId = strdup(utsname.nodename);
        mFriendlyName = strdup(friendlyName);
    }
    ~AboutStore() {
        if (mDeviceId != NULL) {
            free((void*)mDeviceId);
        }
        if (mFriendlyName != NULL) {
            free((void*)mFriendlyName);
        }
    }
    QStatus ReadAll(const char* languageTag, PropertyStore::Filter filter, MsgArg& all) {
        if (languageTag && strcmp(languageTag, "en") != 0) {
            return ER_LANGUAGE_NOT_SUPPORTED;
        }
        if (PropertyStore::WRITE == filter) {
            return ER_NOT_IMPLEMENTED;
        }

        /*
         * The properties of these values should be customized for the application.
         */
        size_t numProps = (PropertyStore::READ == filter) ? 11 : 7;
        MsgArg* props = new MsgArg[numProps];
        static const uint8_t appId[] = { 0x5a, 0x1e, 0xff, 0xf1, 0xf7, 0x99, 0x4d, 0x22, 0x82, 0xc0, 0x93, 0x4d, 0x3c, 0x86, 0x16, 0xa6 };
        props[0].Set("{sv}", "AppId", new MsgArg("ay", 16, appId));
        props[1].Set("{sv}", "DefaultLanguage", new MsgArg("s", "en"));
        props[2].Set("{sv}", "DeviceName", new MsgArg("s", mFriendlyName));
        props[3].Set("{sv}", "DeviceId", new MsgArg("s", mDeviceId));
        props[4].Set("{sv}", "AppName", new MsgArg("s", "SinkService"));
        props[5].Set("{sv}", "Manufacturer", new MsgArg("s", "AllJoyn"));
        props[6].Set("{sv}", "ModelNumber", new MsgArg("s", "1"));
        if (PropertyStore::READ == filter) {
            static const char* supportedLanguages[] = { "en" };
            props[7].Set("{sv}", "SupportedLanguages", new MsgArg("as", 1, supportedLanguages));
            props[8].Set("{sv}", "Description", new MsgArg("s", "AllJoyn Audio Sink"));
            props[9].Set("{sv}", "SoftwareVersion", new MsgArg("s", "v0.0.1"));
            props[10].Set("{sv}", "AJSoftwareVersion", new MsgArg("s", ajn::GetVersion()));
        }

        all.Set("a{sv}", numProps, props);
        all.SetOwnershipFlags(MsgArg::OwnsArgs, true);
        return ER_OK;
    }
  private:
    const char* mDeviceId;
    const char* mFriendlyName;
};

class MyAllJoynListener : public SessionPortListener {
  private:
    BusAttachment* mMsgBus;

  public:
    MyAllJoynListener(BusAttachment* msgBus) {
        mMsgBus = msgBus;
    }

    bool AcceptSessionJoiner(SessionPort sessionPort, const char* joiner, const SessionOpts& opts) {
        printf("Accepting join session request from %s (opts.proximity=%x, opts.traffic=%x, opts.transports=%x)\n",
               joiner, opts.proximity, opts.traffic, opts.transports);
        return true;
    }

    void SessionJoined(SessionPort sessionPort, SessionId id, const char* joiner) {
        printf("SessionJoined with %s (id=%d)\n", joiner, id);
        mMsgBus->EnableConcurrentCallbacks();
        uint32_t timeout = 20;
        QStatus status = mMsgBus->SetLinkTimeout(id, timeout);
        if (status == ER_OK) {
            printf("Link timeout has been set to %ds\n", timeout);
        } else {
            printf("SetLinkTimeout(%d) failed\n", timeout);
        }
    }
};

static volatile sig_atomic_t g_interrupt = false;

static void SigIntHandler(int sig) {
    g_interrupt = true;
}

static int usage(const char* name) {
    printf("Usage: %s [-Ddevice] [-Mmixer] [friendlyname]\n", name);
    return 1;
}

/* Main entry point */
int main(int argc, char** argv, char** envArg) {
    if (argc > 4) {
        return usage(argv[0]);
    }
    const char* deviceName = "default";
    const char* mixerName = "default";
    const char* friendlyName = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strncmp(argv[i], "-h", 2) || !strncmp(argv[i], "--h", 3)) {
            return usage(argv[0]);
        } else if (!strncmp(argv[i], "-D", 2)) {
            deviceName = &argv[i][2];
        } else if (!strncmp(argv[i], "-M", 2)) {
            mixerName = &argv[i][2];
        } else {
            friendlyName = argv[i];
        }
    }

    QStatus status = ER_OK;

    signal(SIGINT, SigIntHandler);

    srand(time(NULL)); // initialize random number generator

    printf("AllJoyn Library version: %s\n", ajn::GetVersion());
    printf("AllJoyn Library build info: %s\n", ajn::GetBuildInfo());
    printf("AllJoyn Audio version: %s\n", ajn::services::audio::GetVersion());
    printf("AllJoyn Audio build info: %s\n", ajn::services::audio::GetBuildInfo());

    const char* connectArgs = getenv("BUS_ADDRESS");
    if (connectArgs == NULL) {
        connectArgs = "unix:abstract=alljoyn";
    }

    BusAttachment* msgBus = new BusAttachment("SinkService", true);
    MyAllJoynListener* listener = new MyAllJoynListener(msgBus);

    AudioDevice* audioDevice = NULL;
    AboutStore* aboutProps = NULL;
    StreamObject* streamObj = NULL;

    /* Start the msg bus */
    status = msgBus->Start();
    if (status == ER_OK) {
        //printf("BusAttachement started.\n");

        /* Create the client-side endpoint */
        status = msgBus->Connect(connectArgs);
        if (status != ER_OK) {
            fprintf(stderr, "Failed to connect to \"%s\"\n", connectArgs);
        } else {
            //printf("Connected to '%s'\n", connectArgs);
        }
    } else {
        fprintf(stderr, "BusAttachment::Start failed\n");
    }

    /*
     * Advertise this service on the bus
     * There are two steps to advertising this service on the bus
     * 1) Create a session
     * 2) Advertise the unique name that will be used by the client to discover
     *    this service
     */
    String name = msgBus->GetUniqueName();
    if (!friendlyName) {
        friendlyName = name.c_str();
    }

    /* Create session */
    SessionPort sessionPort = SESSION_PORT_ANY;
    SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
    if (status == ER_OK) {
        status = msgBus->BindSessionPort(sessionPort, opts, *listener);
        if (status != ER_OK) {
            fprintf(stderr, "BindSessionPort failed (%s)\n", QCC_StatusText(status));
        }
    }

    /* Advertise name */
    if (status == ER_OK) {
        status = msgBus->AdvertiseName(name.c_str(), opts.transports);
        if (status != ER_OK) {
            printf("Failed to advertise name %s (%s)\n", name.c_str(), QCC_StatusText(status));
        }
    }

    if (status == ER_OK) {
#if defined(QCC_OS_ANDROID)
        deviceName = deviceName; mixerName = mixerName; /* Fix compiler warning */
        audioDevice = new AndroidDevice();
#elif defined(QCC_OS_GROUP_POSIX)
        audioDevice = new ALSADevice(deviceName, mixerName);
#endif
        aboutProps = new AboutStore(friendlyName);
        streamObj = new StreamObject(msgBus, "/Speaker/In", audioDevice, sessionPort, aboutProps);
        status = streamObj->Register(msgBus);
        if (status != ER_OK) {
            printf("Failed to register stream object (%s)\n", QCC_StatusText(status));
        }
    }

    if (status == ER_OK) {
        while (g_interrupt == false)
            usleep(100 * 1000);
    }

    if (streamObj != NULL) {
        streamObj->Unregister();
        delete streamObj;
        streamObj = NULL;
    }

    delete audioDevice;
    audioDevice = NULL;
    delete aboutProps;
    aboutProps = NULL;
    delete msgBus;
    msgBus = NULL;
    delete listener;
    listener = NULL;

    return (int)status;
}
