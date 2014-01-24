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
#include <alljoyn/audio/SinkPlayer.h>
#include <alljoyn/audio/SinkSearcher.h>
#include <alljoyn/audio/WavDataSource.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/version.h>
#include <qcc/String.h>
#include <list>
#include <signal.h>

using namespace ajn::services;
using namespace ajn;
using namespace qcc;
using namespace std;

static SinkPlayer* g_sinkPlayer = NULL;

static volatile sig_atomic_t g_interrupt = false;

static void SigIntHandler(int sig) {
    g_interrupt = true;
}

class MySinkSearcher : public SinkSearcher {
    virtual void SinkFound(Service* sink) {
        const char* name = sink->name.c_str();
        const char* path = sink->path.c_str();
        printf("Found \"%s\" %s objectPath=%s, sessionPort=%d\n", sink->friendlyName.c_str(), name, path, sink->port);
        if (!g_sinkPlayer->HasSink(name)) {
            g_sinkPlayer->AddSink(name, sink->port, path);
        }
    }

    virtual void SinkLost(Service* sink) {
        const char* name = sink->name.c_str();
        printf("Lost %s\n", name);
    }
};

class MySinkListener : public SinkListener {
    void SinkAdded(const char* name) {
        printf("SinkAdded: %s\n", name);
        sinks.push_back(name);
        g_sinkPlayer->OpenSink(name);
        /* Start playing when we have at least one sink. */
        if (g_sinkPlayer->GetSinkCount() == 1) {
            g_sinkPlayer->Play();
        }
    }

    void SinkAddFailed(const char* name) {
        printf("SinkAddFailed: %s\n", name);
    }

    void SinkRemoved(const char* name, bool lost) {
        printf("SinkRemoved: %s lost=%d\n", name, lost);
        sinks.remove(name);
    }

    void MuteChanged(const char* name, bool mute) {
        printf("MuteChanged: %s mute=%s\n", name, mute ? "on" : "off");
    }

    void VolumeChanged(const char* name, int16_t volume) {
        printf("VolumeChanged: %s volume=%d\n", name, volume);
    }

  public:
    list<String> sinks;

    void SetVolume(uint8_t volume) {
        for (list<String>::iterator it = sinks.begin(); it != sinks.end(); ++it) {
            int16_t low, high, step;
            g_sinkPlayer->GetVolumeRange(it->c_str(), low, high, step);
            int16_t value = (high - low) * (volume / 100.0) + low;
            g_sinkPlayer->SetVolume(it->c_str(), value);
        }
    }

    uint8_t GetVolume() {
        double volume = .0;
        for (list<String>::iterator it = sinks.begin(); it != sinks.end(); ++it) {
            int16_t low, high, step, value;
            g_sinkPlayer->GetVolumeRange(it->c_str(), low, high, step);
            g_sinkPlayer->GetVolume(it->c_str(), value);
            double v = ((double)value - low) / (high - low) * 100.0;
            if (v > volume) {
                volume = v;
            }
        }
        return volume;
    }
};

/*
 * get a line of input from the the file pointer (most likely stdin).
 * This will capture the the num-1 characters or till a newline character is
 * entered.
 *
 * @param[out] str a pointer to a character array that will hold the user input
 * @param[in]  num the size of the character array 'str'
 * @param[in]  fp  the file pointer the sting will be read from. (most likely stdin)
 *
 * @return returns the same string as 'str' if there has been a read error a null
 *                 pointer will be returned and 'str' will remain unchanged.
 */
char* get_line(char* str, size_t num, FILE* fp)
{
    char* p = fgets(str, num, fp);

    // fgets will capture the '\n' character if the string entered is shorter than
    // num. Remove the '\n' from the end of the line and replace it with nul '\0'.
    if (p != NULL) {
        size_t last = strlen(str) - 1;
        if (str[last] == '\n') {
            str[last] = '\0';
        }
    }

    return g_interrupt ? NULL : p;
}

/* Main entry point */
int main(int argc, char** argv, char** envArg) {
    if (argc < 2 || argc > 3) {
        printf("Usage: %s file.wav [raw|alac]\n", argv[0]);
        return 1;
    }

    const char* selectedType = MIMETYPE_AUDIO_RAW;
    if (argc >= 3) {
        if (0 == strcmp(argv[2], "raw")) {
            selectedType = MIMETYPE_AUDIO_RAW;
        } else if (0 == strcmp(argv[2], "alac")) {
            selectedType = MIMETYPE_AUDIO_ALAC;
        } else {
            printf("invalid type: %s\n", argv[2]);
            return 1;
        }
    }

    QStatus status = ER_OK;

    printf("AllJoyn Library version: %s\n", ajn::GetVersion());
    printf("AllJoyn Library build info: %s\n", ajn::GetBuildInfo());
    printf("AllJoyn Audio version: %s\n", ajn::services::audio::GetVersion());
    printf("AllJoyn Audio build info: %s\n", ajn::services::audio::GetBuildInfo());

    /* Install SIGINT handler */
    signal(SIGINT, SigIntHandler);

    const char* connectArgs = getenv("BUS_ADDRESS");
    if (connectArgs == NULL) {
        connectArgs = "unix:abstract=alljoyn";
    }

    /* Create message bus */
    BusAttachment* msgBus = new BusAttachment("SinkClient", true, 128);

    /* Start the msg bus */
    if (status == ER_OK) {
        status = msgBus->Start();
        if (status != ER_OK) {
            fprintf(stderr, "BusAttachment::Start failed\n");
        }
    }

    /* Connect to the bus */
    if (status == ER_OK) {
        status = msgBus->Connect(connectArgs);
        if (status != ER_OK) {
            fprintf(stderr, "Failed to connect to \"%s\"\n", connectArgs);
        }
    }

    /* Create SinkPlayer */
    MySinkListener listener;
    g_sinkPlayer = new SinkPlayer(msgBus);
    g_sinkPlayer->AddListener(&listener);
    g_sinkPlayer->SetPreferredFormat(selectedType);

    /* Register our searcher */
    MySinkSearcher searcher;
    if (status == ER_OK) {
        status = searcher.Register(msgBus);
        if (status != ER_OK) {
            fprintf(stderr, "Failed to register searcher (%s)\n", QCC_StatusText(status));
        }
    }

    /* Set data source to WAV file */
    WavDataSource dataSource;
    if (!dataSource.Open(argv[1])) {
        fprintf(stderr, "Failed to set data source (%s)\n", argv[1]);
        return 1;
    }
    if (!g_sinkPlayer->SetDataSource(&dataSource)) {
        fprintf(stderr, "Failed to set data source (%s)\n", argv[1]);
        return 1;
    }

    if (status == ER_OK) {
        char buf[1024];
        char name[128];
        while (!g_interrupt && get_line(buf, sizeof(buf) / sizeof(buf[0]), stdin) != NULL) {
            int i;
            if (strcmp(buf, "play") == 0) {
                if (!g_sinkPlayer->IsPlaying()) {
                    g_sinkPlayer->Play();
                }

            } else if (strcmp(buf, "pause") == 0) {
                if (g_sinkPlayer->IsPlaying()) {
                    g_sinkPlayer->Pause();
                }

            } else if (sscanf(buf, "volume %128s %d%%", name, &i) == 2) {
                g_sinkPlayer->SetVolume(name, i);
            } else if (sscanf(buf, "volume %d%%", &i) == 1) {
                listener.SetVolume(i);
            } else if (sscanf(buf, "volume %128s", name) == 1) {
                int16_t low, high, step, volume;
                if (g_sinkPlayer->GetVolumeRange(name, low, high, step) &&
                    g_sinkPlayer->GetVolume(name, volume)) {
                    printf("Volume is %d [%d,%d..%d]\n", volume, low, low + step, high);
                }
            } else if (strcmp(buf, "volume") == 0) {
                printf("Volume is %d%%\n", listener.GetVolume());

            } else if (sscanf(buf, "mute %d", &i) == 1) {
                g_sinkPlayer->SetMute(NULL, i);
            } else if (sscanf(buf, "mute %128s %d", name, &i) == 2) {
                g_sinkPlayer->SetMute(name, i);
            } else if (strcmp(buf, "mute") == 0) {
                bool mute;
                if (g_sinkPlayer->GetMute(NULL, mute)) {
                    printf("Mute is %s\n", mute ? "off" : "on");
                }

            } else if (sscanf(buf, "open %128s", name) == 1) {
                dataSource.Close();
                if (!dataSource.Open(name)) {
                    fprintf(stderr, "Failed to set data source (%s)\n", name);
                    continue;
                }
                if (!g_sinkPlayer->SetDataSource(&dataSource)) {
                    fprintf(stderr, "Failed to set data source (%s)\n", name);
                    continue;
                }
                g_sinkPlayer->OpenAllSinks();
            } else if (strcmp(buf, "open") == 0) {
                g_sinkPlayer->OpenAllSinks();

            } else if (strcmp(buf, "close") == 0) {
                g_sinkPlayer->CloseAllSinks();

            } else if (strcmp(buf, "quit") == 0 || strcmp(buf, "exit") == 0) {
                break;
            } else {
                printf("available commands: open, close, play, pause, volume, mute, quit\n");
            }
        }
    }

    searcher.Unregister();
    g_sinkPlayer->RemoveAllSinks();
    /* Want for sinks to be removed */
    while (g_sinkPlayer->GetSinkCount() > 0)
        usleep(100 * 1000);

    delete g_sinkPlayer;
    g_sinkPlayer = NULL;

    delete msgBus;
    msgBus = NULL;

    printf("SinkClient exiting with status %d (%s)\n", status, QCC_StatusText(status));

    return (int)status;
}
