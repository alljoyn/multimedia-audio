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
#define __STDC_LIMIT_MACROS

#include "AudioSinkObject.h"

#include "Clock.h"
#include <alljoyn/audio/StreamObject.h>
#include <qcc/Debug.h>
#include <qcc/time.h>
#include <algorithm>
#include <inttypes.h>
#include <stdint.h>
#include <math.h>

#define QCC_MODULE "ALLJOYN_AUDIO"

using namespace ajn;
using namespace qcc;
using namespace std;

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define FIFO_SIZE_IN_SECONDS    5
#define FIFO_LOW_THRESHOLD      (FIFO_SIZE_IN_SECONDS - 1) /* The low-water mark at which FifoPositionChanged signal will be emitted */

namespace ajn {
namespace services {

AudioSinkObject::AudioSinkObject(BusAttachment* bus, const char* path, StreamObject* stream, AudioDevice* audioDevice) :
    PortObject(bus, path, stream),
    mPlayState(PlayState::IDLE), mLateChunkCount(0),
    mDecodeThread(NULL), mDecoder(NULL),
    mAudioOutputEvent(new Event()), mAudioOutputThread(NULL),
    mAudioDevice(audioDevice), mAudioDeviceBufferSize(0) {
    mAudioDevice->AddListener(this);
    mDirection = DIRECTION_SINK;

    AudioDecoder::GetCapabilities(&mCapabilities, &mNumCapabilities);

    /* Add Port.AudioSink interface */
    const InterfaceDescription* audioSinkIntf = bus->GetInterface(AUDIO_SINK_INTERFACE);
    assert(audioSinkIntf);
    AddInterface(*audioSinkIntf);

    /* Add VolumeControl interface */
    const InterfaceDescription* volumeIntf = bus->GetInterface(VOLUME_INTERFACE);
    assert(volumeIntf);
    AddInterface(*volumeIntf);

    /* Register the method handlers with the object */
    const MethodEntry methodEntries[] = {
        { audioSinkIntf->GetMember("Play"), static_cast<MessageReceiver::MethodHandler>(&AudioSinkObject::Play) },
        { audioSinkIntf->GetMember("Pause"), static_cast<MessageReceiver::MethodHandler>(&AudioSinkObject::Pause) },
        { audioSinkIntf->GetMember("Flush"), static_cast<MessageReceiver::MethodHandler>(&AudioSinkObject::Flush) },
        { volumeIntf->GetMember("AdjustVolume"), static_cast<MessageReceiver::MethodHandler>(&AudioSinkObject::AdjustVolume) },
        { volumeIntf->GetMember("AdjustVolumePercent"), static_cast<MessageReceiver::MethodHandler>(&AudioSinkObject::AdjustVolumePercent) },
    };
    QStatus status = AddMethodHandlers(methodEntries, sizeof(methodEntries) / sizeof(methodEntries[0]));
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to register method handlers for AudioSinkObject"));
    }

    const InterfaceDescription* audioSourceIntf = bus->GetInterface(AUDIO_SOURCE_INTERFACE);
    assert(audioSourceIntf);
    const InterfaceDescription::Member* audioDataMember = audioSourceIntf->GetMember("Data");
    assert(audioDataMember);
    status = bus->RegisterSignalHandler(this,
                                        static_cast<MessageReceiver::SignalHandler>(&AudioSinkObject::AudioDataSignalHandler),
                                        audioDataMember, NULL);
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to register audio signal handler"));
    }

    mPlayStateChangedMember = audioSinkIntf->GetMember("PlayStateChanged");
    assert(mPlayStateChangedMember);

    mFifoPositionChangedMember = audioSinkIntf->GetMember("FifoPositionChanged");
    assert(mFifoPositionChangedMember);

    mVolumeChangedMember = volumeIntf->GetMember("VolumeChanged");
    assert(mVolumeChangedMember);

    mMuteChangedMember = volumeIntf->GetMember("MuteChanged");
    assert(mMuteChangedMember);

    mEnabledChangedMember = volumeIntf->GetMember("EnabledChanged");
    assert(mEnabledChangedMember);
}

AudioSinkObject::~AudioSinkObject() {
    delete mAudioOutputEvent;
    mAudioOutputEvent = NULL;

    bus->UnregisterAllHandlers(this);
}

void AudioSinkObject::Cleanup(bool drain) {
    QCC_DbgTrace(("%s(drain=%d)", __FUNCTION__, drain));

    bool drainAudioDevice = drain && (mPlayState == PlayState::PLAYING);
    if (drain) {
        while (mPlayState == PlayState::PLAYING)
            SleepNanos(10000000);
    }

    StopAudioOutputThread();
    StopDecodeThread();
    mAudioDevice->Close(drainAudioDevice);

    mAudioOutputEvent->ResetEvent();

    if (mDecoder != NULL) {
        delete mDecoder;
        mDecoder = NULL;
    }

    ClearBuffer();
    SetPlayState(PlayState::IDLE);

    PortObject::Cleanup(drain);
}

QStatus AudioSinkObject::Get(const char* ifcName, const char* propName, MsgArg& val) {
    QStatus status = ER_OK;

    QCC_DbgTrace(("GetProperty called for %s.%s", ifcName, propName));

    if (0 == strcmp(ifcName, AUDIO_SINK_INTERFACE)) {
        if (0 == strcmp(propName, "Version")) {
            val.Set("q", INTERFACES_VERSION);
        } else if (0 == strcmp(propName, "FifoSize")) {
            val.Set("u", mMaxBufferSize);
        } else if (0 == strcmp(propName, "FifoPosition")) {
            mBufferMutex.Lock();
            uint32_t fifoPosition = GetCombinedBufferSize();
            mBufferMutex.Unlock();
            val.Set("u", fifoPosition);
        } else if (0 == strcmp(propName, "Delay")) {
            mBufferMutex.Lock();
            uint32_t fifoPosition = GetCombinedBufferSize();
            mBufferMutex.Unlock();
            val.Set("(uu)", fifoPosition, mAudioDeviceBufferSize);
        } else {
            status = ER_BUS_NO_SUCH_PROPERTY;
        }
    } else if (0 == strcmp(ifcName, VOLUME_INTERFACE)) {
        if (0 == strcmp(propName, "Version")) {
            val.Set("q", INTERFACES_VERSION);
        } else if (0 == strcmp(propName, "Volume")) {
            int16_t volume;
            bool success = mAudioDevice->GetVolume(volume);
            if (success) {
                val.Set("n", volume);
            } else {
                status = ER_FAIL;
            }
        } else if (0 == strcmp(propName, "VolumeRange")) {
            int16_t low, high, step;
            bool success = mAudioDevice->GetVolumeRange(low, high, step);
            if (success) {
                val.Set("(nnn)", low, high, step);
            } else {
                status = ER_FAIL;
            }
        } else if (0 == strcmp(propName, "Mute")) {
            bool mute;
            bool success = mAudioDevice->GetMute(mute);
            if (success) {
                val.Set("b", mute);
            } else {
                status = ER_FAIL;
            }
        } else if (0 == strcmp(propName, "Enabled")) {
            val.Set("b", mAudioDevice->GetEnabled());
        } else {
            status = ER_BUS_NO_SUCH_PROPERTY;
        }
    } else {
        status = ER_BUS_NO_SUCH_INTERFACE;
    }

    if (status == ER_BUS_NO_SUCH_INTERFACE || status == ER_BUS_NO_SUCH_PROPERTY) {
        return PortObject::Get(ifcName, propName, val);
    }

    return status;
}

void AudioSinkObject::SetProp(const InterfaceDescription::Member* member, Message& msg) {
    const char* ifcName = msg->GetArg(0)->v_string.str;
    const char* propName = msg->GetArg(1)->v_string.str;
    const MsgArg* val = msg->GetArg(2);

    QStatus status = ER_OK;
    if (0 == strcmp(ifcName, AUDIO_SINK_INTERFACE)) {
        /* No writable properties on AudioSink interface. */
        status = ER_BUS_PROPERTY_ACCESS_DENIED;
    } else if (0 == strcmp(ifcName, VOLUME_INTERFACE)) {
        if (!mAudioDevice->GetEnabled()) {
            status = ER_FAIL;
        } else if (0 == strcmp(propName, "Volume")) {
            int16_t newVolume = INT16_MIN;
            status = val->Get("n", &newVolume);
            if (status == ER_OK) {
                int16_t low, high, step;
                mAudioDevice->GetVolumeRange(low, high, step);
                if (newVolume < low || high < newVolume) {
                    MethodReply(msg, "org.alljoyn.Error.OutOfRange");
                    return;
                }
                if (!mAudioDevice->SetVolume(newVolume)) {
                    status = ER_FAIL;
                }
            }
        } else if (0 == strcmp(propName, "VolumeRange")) {
            status = ER_BUS_PROPERTY_ACCESS_DENIED;
        } else if (0 == strcmp(propName, "Mute")) {
            bool newMute = false;
            status = val->Get("b", &newMute);
            if (status == ER_OK) {
                if (!mAudioDevice->SetMute(newMute)) {
                    status = ER_FAIL;
                }
            }
        } else if (0 == strcmp(propName, "Enabled")) {
            status = ER_BUS_PROPERTY_ACCESS_DENIED;
        } else {
            status = ER_BUS_NO_SUCH_PROPERTY;
        }
    } else {
        status = ER_BUS_NO_SUCH_INTERFACE;
    }

    switch (status) {
    case ER_BUS_NO_SUCH_INTERFACE:
        QCC_LogError(status, ("SetProperty called for unhandled interface: %s", ifcName));
        break;

    case ER_BUS_NO_SUCH_PROPERTY:
        QCC_LogError(status, ("SetProperty called for unhandled property: %s", propName));
        break;

    default: break;
    }

    MethodReply(msg, status);
}

void AudioSinkObject::DoConnect(const InterfaceDescription::Member* member, Message& msg) {
    MsgArg* rateArg = GetParameterValue(mConfiguration->parameters, mConfiguration->numParameters, "Rate");
    MsgArg* formatArg = GetParameterValue(mConfiguration->parameters, mConfiguration->numParameters, "Format");
    MsgArg* channelsArg = GetParameterValue(mConfiguration->parameters, mConfiguration->numParameters, "Channels");
    if (rateArg == NULL || formatArg == NULL || channelsArg == NULL) {
        QCC_LogError(ER_FAIL, ("Configure missing Rate, Format or Channels"));
        REPLY(ER_FAIL);
        return;
    }

    mSampleRate = rateArg->v_uint16;
    mChannelsPerFrame = channelsArg->v_byte;
    const char* format = formatArg->v_string.str;

    uint32_t bitsPerChannel;
    if (strcmp(format, "s16le") == 0) {
        bitsPerChannel = 16;
    } else {
        QCC_LogError(ER_FAIL, ("Unsupported audio format: %s", format));
        REPLY(ER_FAIL);
        return;
    }

    mBytesPerFrame = (bitsPerChannel >> 3) * mChannelsPerFrame;
    mBytesPerSecond = mSampleRate * mBytesPerFrame;
    mMaxBufferSize = mBytesPerSecond * FIFO_SIZE_IN_SECONDS;
    mFifoLowThreshold = mBytesPerSecond * FIFO_LOW_THRESHOLD;

    delete mDecoder;
    mDecoder = AudioDecoder::Create(mConfiguration->type.c_str());
    QStatus status = mDecoder->Configure(mConfiguration);
    if (status != ER_OK) {
        delete mDecoder;
        mDecoder = NULL;
        REPLY(status);
        return;
    }

    if (!mAudioDevice->Open(format, mSampleRate, mChannelsPerFrame, mAudioDeviceBufferSize)) {
        QCC_LogError(ER_FAIL, ("Failed to open audio device"));
        REPLY(ER_FAIL);
        return;
    }

    StartAudioOutputThread();

    StartDecodeThread();

    QCC_DbgHLPrintf(("Configured type=[%s] numParameters=[%zu]", mConfiguration->type.c_str(), mConfiguration->numParameters));

    REPLY_OK();
}

void AudioSinkObject::Play(const InterfaceDescription::Member* member, Message& msg) {
    if (mAudioOutputThread == NULL) {
        StartAudioOutputThread();
        mAudioDevice->Play();
    }

    REPLY_OK();
}

struct TimedCmd {
    AudioSinkObject* apo;
    uint64_t timeNanos;
};

void AudioSinkObject::Pause(const InterfaceDescription::Member* member, Message& msg) {
    GET_ARGS(1);

    uint64_t now = mStream->GetCurrentTimeNanos();
    const uint64_t time = args[0].v_uint64;
    if (time == 0 || time <= now) {
        DoPause();
    } else {
        TimedCmd* tc = new TimedCmd;
        tc->apo = this;
        tc->timeNanos = time;
        Thread* t = new Thread("Pause", &PauseThread);
        t->Start(tc);
    }

    REPLY_OK();
}

void AudioSinkObject::DoPause() {
    if (mAudioOutputThread != NULL) {
        StopAudioOutputThread();
        mAudioDevice->Pause();
        SetPlayState(PlayState::PAUSED);
    }
}

ThreadReturn AudioSinkObject::PauseThread(void* arg) {
    TimedCmd* tc = reinterpret_cast<TimedCmd*>(arg);
    AudioSinkObject* apo = tc->apo;

    apo->mStream->SleepUntilTimeNanos(tc->timeNanos);
    apo->DoPause();

    return NULL;
}

void AudioSinkObject::Flush(const InterfaceDescription::Member* member, Message& msg) {
    GET_ARGS(1);

    const uint64_t flushTime = args[0].v_uint64;
    if (flushTime != 0) {
        mStream->SleepUntilTimeNanos(flushTime);
    }

    mBufferMutex.Lock();
    uint32_t size = GetCombinedBufferSize();
    ClearBuffer();
    mBufferMutex.Unlock();

    EmitFifoPositionChangedSignal();
    SetPlayState(PlayState::IDLE);

    MsgArg outArg("u", size);
    QStatus status = MethodReply(msg, &outArg, 1);
    if (status != ER_OK) {
        QCC_LogError(status, ("Flush reply failed"));
    }
}

void AudioSinkObject::AdjustVolume(const InterfaceDescription::Member* member, Message& msg) {
    if (!mAudioDevice->GetEnabled()) {
        MethodReply(msg, ER_FAIL);
        return;
    }

    GET_ARGS(1);

    int16_t delta = args[0].v_int16;

    int16_t low, high, step, volume;
    if (!mAudioDevice->GetVolumeRange(low, high, step) || !mAudioDevice->GetVolume(volume)) {
        MethodReply(msg, ER_FAIL);
        return;
    }

    int16_t newVolume = volume + delta;
    newVolume = (newVolume < low) ? low : ((newVolume > high) ? high : newVolume);

    if (!mAudioDevice->SetVolume(newVolume)) {
        MethodReply(msg, ER_FAIL);
        return;
    }

    MethodReply(msg, ER_OK);
}

void AudioSinkObject::AdjustVolumePercent(const InterfaceDescription::Member* member, Message& msg) {
    if (!mAudioDevice->GetEnabled()) {
        MethodReply(msg, ER_FAIL);
        return;
    }

    GET_ARGS(1);

    double volChange = args[0].v_double;

    if (0 == volChange) {
        MethodReply(msg, ER_OK);
        return;
    }

    int16_t low, high, step, volume;
    if (!mAudioDevice->GetVolumeRange(low, high, step) || !mAudioDevice->GetVolume(volume)) {
        MethodReply(msg, ER_FAIL);
        return;
    }

    int16_t newVolume = 0;
    double halfStep = step / 2;
    if (volChange <= -1.0) {
        newVolume = low;
    } else if (volChange >= 1.0) {
        newVolume = high;
    } else {
        if (volChange > 0) {
            newVolume = volume + floor(((high - volume) * volChange) + halfStep);
        } else {
            newVolume = volume + floor(((volume - low) * volChange) + halfStep);
        }
        newVolume -= newVolume % step;
    }

    if (!mAudioDevice->SetVolume(newVolume)) {
        MethodReply(msg, ER_FAIL);
        return;
    }

    MethodReply(msg, ER_OK);
}

void AudioSinkObject::MuteChanged(bool mute) {
    EmitMuteChangedSignal(mute);
}

void AudioSinkObject::VolumeChanged(int16_t volume) {
    EmitVolumeChangedSignal(volume);
}

QStatus AudioSinkObject::EmitPlayStateChangedSignal(uint8_t oldState, uint8_t newState) {
    MsgArg args[2];
    args[0].Set("y", oldState);
    args[1].Set("y", newState);

    uint8_t flags = 0;
    QStatus status = Signal(NULL, mStream->GetSessionId(), *mPlayStateChangedMember, args, 2, 0, flags);
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to emit PlayStateChanged signal"));
    }

    return status;
}

QStatus AudioSinkObject::EmitVolumeChangedSignal(int16_t volume) {
    MsgArg arg("n", volume);

    uint8_t flags = 0;
    QStatus status = Signal(NULL, mStream->GetSessionId(), *mVolumeChangedMember, &arg, 1, 0, flags);
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to emit VolumeChanged signal"));
    }

    return status;
}

QStatus AudioSinkObject::EmitMuteChangedSignal(bool mute) {
    MsgArg arg("b", mute);

    uint8_t flags = 0;
    QStatus status = Signal(NULL, mStream->GetSessionId(), *mMuteChangedMember, &arg, 1, 0, flags);
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to emit MuteChanged signal"));
    }

    return status;
}

QStatus AudioSinkObject::EmitFifoPositionChangedSignal() {
    uint8_t flags = 0;
    QStatus status = Signal(NULL, mStream->GetSessionId(), *mFifoPositionChangedMember, NULL, 0, 0, flags);
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to emit FifoPositionChanged signal"));
    }

    return status;
}

QStatus AudioSinkObject::EmitVolumeControlEnabledChangedSignal()
{
    MsgArg arg("b", mAudioDevice->GetEnabled());

    uint8_t flags = 0;
    QStatus status = Signal(NULL, mStream->GetSessionId(), *mEnabledChangedMember, &arg, 1, 0, flags);
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to emit Enabled signal"));
    }

    return status;
}

void AudioSinkObject::AudioDataSignalHandler(const InterfaceDescription::Member* member,
                                             const char* sourcePath, Message& msg)
{
    if (mConfiguration == NULL) {
        QCC_LogError(ER_WARNING, ("Not configured, ignoring Audio Data signal"));
        return;
    }

    size_t numArgs = 0;
    const MsgArg* args = NULL;
    msg->GetArgs(numArgs, args);

    if (numArgs != 2) {
        QCC_LogError(ER_BAD_ARG_COUNT, ("Received Audio Data signal with invalid number of arguments"));
        return;
    }

    const uint64_t timestamp = args[0].v_uint64;
    uint64_t now = mStream->GetCurrentTimeNanos();
    if (timestamp < now) {
        QCC_LogError(ER_WARNING, ("Dropping received Audio Data as it's out of date by %" PRIu64 " nanos", now - timestamp));
        EmitFifoPositionChangedSignal();
        mLateChunkCount++;
        return;
    }

    const uint8_t* data = args[1].v_scalarArray.v_byte;
    size_t dataSize = args[1].v_scalarArray.numElements;

    QCC_DbgTrace(("Received Audio Data: %zu bytes timestamped %" PRIu64, dataSize, timestamp));

    TimedSamples ts;
    memset(&ts, 0, sizeof(ts));
    ts.timestamp = timestamp;
    ts.dataSize = dataSize;
    ts.data = (uint8_t*)malloc(ts.dataSize);
    memcpy(ts.data, data, dataSize);

    if (mLateChunkCount > 0) {
        mLateChunkCount = 0;
        ts.resync = true;
    }

    mDecodeBufferMutex.Lock();
    mDecodeBuffers.push_back(ts);
    if (!mDecodeEvent.IsSet()) {
        mDecodeEvent.SetEvent();
    }
    mDecodeBufferMutex.Unlock();
}

void AudioSinkObject::StartAudioOutputThread() {
    if (mAudioOutputThread == NULL) {
        mAudioOutputThread = new Thread("AudioOutput", &AudioOutputThread);
        mAudioOutputThread->Start(this);
    }
}

void AudioSinkObject::StopAudioOutputThread() {
    if (mAudioOutputThread != NULL) {
        mAudioOutputThread->Stop();
        mAudioOutputThread->Join();
        delete mAudioOutputThread;
        mAudioOutputThread = NULL;
        mAudioOutputEvent->ResetEvent();
    }
}

ThreadReturn AudioSinkObject::AudioOutputThread(void* arg) {
    AudioSinkObject* apo = reinterpret_cast<AudioSinkObject*>(arg);
    Thread* selfThread = Thread::GetThread();

    Event& stopEvent = selfThread->GetStopEvent();
    vector<Event*> waitEvents, signaledEvents;

    waitEvents.push_back(apo->mAudioOutputEvent);
    waitEvents.push_back(&stopEvent);

    QStatus status = Event::Wait(waitEvents, signaledEvents);
    if (status != ER_OK) {
        QCC_LogError(status, ("Event wait failed"));
        return NULL;
    }

    // Has thread been instructed to stop?
    if (find(signaledEvents.begin(), signaledEvents.end(), &stopEvent) != signaledEvents.end()) {
        return NULL;
    }

    size_t bufferSize = apo->mAudioDeviceBufferSize * 4;
    uint8_t* buffer = (uint8_t*)calloc(bufferSize, 1);
    if (buffer == NULL) {
        QCC_LogError(ER_OUT_OF_MEMORY, ("Out of memory"));
        return NULL;
    }

    apo->mBufferMutex.Lock();
    TimedSamples ts = apo->mBuffers.front();
    uint64_t startTime = ts.timestamp;
    apo->mBufferMutex.Unlock();

    apo->mStream->SleepUntilTimeNanos(startTime);
    QCC_DbgHLPrintf(("Playback started at: %" PRIu64, apo->mStream->GetCurrentTimeNanos()));

    bool didUnderrun = false;
    while (!selfThread->IsStopping()) {
        apo->mBufferMutex.Lock();

        if (apo->mBuffers.empty()) {
            didUnderrun = true;
            QCC_LogError(ER_WARNING, ("Buffer underrun at %" PRIu64, apo->mStream->GetCurrentTimeNanos()));

            apo->mAudioOutputEvent->ResetEvent();
            apo->mBufferMutex.Unlock();

            apo->SetPlayState(PlayState::IDLE);

            status = Event::Wait(waitEvents, signaledEvents);
            if (status != ER_OK) {
                QCC_LogError(status, ("Event wait failed"));
                break;
            }

            if (find(signaledEvents.begin(), signaledEvents.end(), &stopEvent) != signaledEvents.end()) {
                break;
            }

            continue;
        }

        while (didUnderrun) {
            // On underrun we require 50% fifo fill before resuming playback
            if (apo->GetBufferSize() < (apo->mMaxBufferSize * 0.5)) {
                apo->mAudioOutputEvent->ResetEvent();
                apo->mBufferMutex.Unlock();

                status = Event::Wait(waitEvents, signaledEvents);
                if (status != ER_OK) {
                    QCC_LogError(status, ("Event wait failed"));
                    break;
                }

                if (find(signaledEvents.begin(), signaledEvents.end(), &stopEvent) != signaledEvents.end()) {
                    status = ER_STOPPING_THREAD;
                    break;
                }

                apo->mBufferMutex.Lock();
            } else {
                TimedSamples ts = apo->mBuffers.front();
                apo->mBuffers.pop_front();
                // Chunks may have become outdated while we were waiting for fifo fill
                if (ts.timestamp < (apo->mStream->GetCurrentTimeNanos() + 10000)) {
                    QCC_LogError(ER_WARNING, ("Dropping outdated chunk during underrun recovery: %" PRIu64, ts.timestamp));
                    apo->EmitFifoPositionChangedSignal();
                    continue;
                } else {
                    ts.resync = true;
                    apo->mBuffers.push_front(ts);
                    apo->mAudioDevice->Recover();
                    didUnderrun = false;
                    break;
                }
            }
        }

        if (status != ER_OK) {
            break;
        }

        TimedSamples ts = apo->mBuffers.front();
        if (ts.resync) {
            apo->mBufferMutex.Unlock();
            uint64_t now = apo->mStream->GetCurrentTimeNanos();
            if (ts.timestamp > now) {
                uint64_t diff = ts.timestamp - now;
                QCC_LogError(ER_WARNING, ("Resync, sleeping for %" PRIu64 " nanos until %" PRIu64, diff, ts.timestamp));
                SleepNanos(diff);
            } else {
                QCC_LogError(ER_WARNING, ("Encountered outdated chunk for resync"));
            }
            apo->mBufferMutex.Lock();
            // Flush may have occurred during sleep
            if (apo->mBuffers.empty()) {
                apo->mBufferMutex.Unlock();
                continue;
            }
            QCC_DbgHLPrintf(("Resync finished\n"));
            ts.resync = false;
        }

        size_t size = apo->GetBufferSize();
        apo->mBuffers.pop_front();

        size_t sizeToRead = apo->mAudioDeviceBufferSize * apo->mBytesPerFrame;
        uint32_t framesWanted = apo->mAudioDevice->GetFramesWanted();
        if (framesWanted > 0) {
            size_t bytesWanted = framesWanted * apo->mBytesPerFrame;
            sizeToRead = MAX(bytesWanted, sizeToRead);
        }

        sizeToRead = MIN(size, sizeToRead);

        if (bufferSize < sizeToRead) {
            free((void*)buffer);
            bufferSize = sizeToRead;
            buffer = (uint8_t*)calloc(bufferSize, 1);
        }

#ifndef NDEBUG
        uint64_t delay = 0;
        uint32_t delayInFrames = apo->mAudioDevice->GetDelay();
        if (delayInFrames > 0) {
            size_t delayInBytes = delayInFrames * apo->mBytesPerFrame;
            delay = (uint64_t)(((double)delayInBytes / apo->mBytesPerSecond) * 1000000000);
        }

        QCC_DbgHLPrintf(("Difference between requested and expected chunk time: %" PRId64 " nanos",
                         (apo->mStream->GetCurrentTimeNanos() + delay) - ts.timestamp));
#endif

        if (ts.dataSize < sizeToRead) {
            sizeToRead = ts.dataSize;
        }

        size_t sizeRead = 0;
        if (ts.dataSize == sizeToRead) {
            memcpy(buffer, ts.data + ts.offset, ts.dataSize);
            sizeRead = ts.dataSize;
            free((void*)ts.data);
        } else if (ts.dataSize > sizeToRead) {
            memcpy(buffer, ts.data + ts.offset, sizeToRead);
            ts.offset += sizeToRead;
            ts.dataSize -= sizeToRead;
            ts.timestamp += (uint64_t)(((double)sizeToRead / apo->mBytesPerSecond) * 1000000000);
            apo->mBuffers.push_front(ts);
            sizeRead = sizeToRead;
        }

        apo->mBufferMutex.Unlock();

        size_t newSize = size - sizeRead;
        if (newSize <= apo->mFifoLowThreshold) {
            apo->EmitFifoPositionChangedSignal();
        }

        apo->SetPlayState(PlayState::PLAYING);

        uint32_t bufferSizeInFrames = sizeRead / apo->mBytesPerFrame;
        apo->mAudioDevice->Write(buffer, bufferSizeInFrames);
    }

    free((void*)buffer);

    return NULL;
}

void AudioSinkObject::StartDecodeThread() {
    if (mDecodeThread == NULL) {
        mDecodeEvent.ResetEvent();
        mDecodeThread = new Thread("Decode", &DecodeThread);
        mDecodeThread->Start(this);
    }
}

void AudioSinkObject::StopDecodeThread() {
    if (mDecodeThread != NULL) {
        mDecodeThread->Stop();
        mDecodeThread->Join();
        delete mDecodeThread;
        mDecodeThread = NULL;
    }
}

ThreadReturn AudioSinkObject::DecodeThread(void* arg) {
    AudioSinkObject* apo = reinterpret_cast<AudioSinkObject*>(arg);
    Thread* selfThread = Thread::GetThread();

    Event& stopEvent = selfThread->GetStopEvent();
    vector<Event*> waitEvents, signaledEvents;

    waitEvents.push_back(&apo->mDecodeEvent);
    waitEvents.push_back(&stopEvent);

    QStatus status = Event::Wait(waitEvents, signaledEvents);
    if (status != ER_OK) {
        QCC_LogError(status, ("Event wait failed"));
        return NULL;
    }

    // Has thread been instructed to stop?
    if (find(signaledEvents.begin(), signaledEvents.end(), &stopEvent) != signaledEvents.end()) {
        return NULL;
    }

    while (!selfThread->IsStopping()) {
        apo->mDecodeBufferMutex.Lock();

        if (apo->mDecodeBuffers.empty()) {
            apo->mDecodeEvent.ResetEvent();
            apo->mDecodeBufferMutex.Unlock();

            status = Event::Wait(waitEvents, signaledEvents);
            if (status != ER_OK) {
                QCC_LogError(status, ("Event wait failed"));
                break;
            }

            if (find(signaledEvents.begin(), signaledEvents.end(), &stopEvent) != signaledEvents.end()) {
                break;
            }

            continue;
        }

        TimedSamples ts = apo->mDecodeBuffers.front();
        apo->mDecodeBufferMutex.Unlock();

        apo->mDecoder->Decode(&ts.data, &ts.dataSize);

        apo->mBufferMutex.Lock();
        apo->mDecodeBuffers.pop_front();
        size_t size = apo->GetCombinedBufferSize();
        if ((apo->mMaxBufferSize - size) < ts.dataSize) {
            QCC_LogError(ER_WARNING, ("Buffer is full, discarding received data, Capacity=%zu Size=%zu DataSize=%u",
                                      apo->mMaxBufferSize, size, ts.dataSize));
            free((void*)ts.data);
            ts.data = NULL;
        } else {
            apo->mBuffers.push_back(ts);
            if (!apo->mAudioOutputEvent->IsSet()) {
                apo->mAudioOutputEvent->SetEvent();
            }
        }
        apo->mBufferMutex.Unlock();
    }

    return NULL;
}

void AudioSinkObject::SetPlayState(uint8_t newState) {
    uint8_t oldState = mPlayState;
    mPlayState = newState;
    if (oldState != newState) {
        EmitPlayStateChangedSignal(oldState, newState);
    }
}

size_t AudioSinkObject::GetCombinedBufferSize() {
    size_t s = 0;

    mDecodeBufferMutex.Lock();
    s += GetDecodeBufferSize();
    mDecodeBufferMutex.Unlock();

    s += GetBufferSize();

    return s;
}

size_t AudioSinkObject::GetDecodeBufferSize() {
    return mDecodeBuffers.size() * (mDecoder ? mDecoder->GetFrameSize() * mBytesPerFrame : 0);
}

size_t AudioSinkObject::GetBufferSize() {
    size_t size = 0;
    for (TimedSamplesList::iterator it = mBuffers.begin(); it != mBuffers.end(); ++it)
        size += (*it).dataSize;
    return size;
}

void AudioSinkObject::ClearBuffer() {
    mDecodeBufferMutex.Lock();
    for (TimedSamplesList::iterator it = mDecodeBuffers.begin(); it != mDecodeBuffers.end(); ++it) {
        free((void*)(*it).data);
        (*it).data = NULL;
    }
    mDecodeBuffers.clear();
    mDecodeBufferMutex.Unlock();

    for (TimedSamplesList::iterator it = mBuffers.begin(); it != mBuffers.end(); ++it) {
        free((void*)(*it).data);
        (*it).data = NULL;
    }
    mBuffers.clear();
}

}
}
