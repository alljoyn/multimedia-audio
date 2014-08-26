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

#include <alljoyn/audio/posix/ALSADevice.h>

#include <qcc/Debug.h>
#include <qcc/Mutex.h>
#include <qcc/Thread.h>
#include <algorithm>
#include <limits.h>

#define QCC_MODULE "ALLJOYN_AUDIO"

/*
 * Some ALSA drivers will trigger an event when we set volume or mute.
 * Others will not.  The intent of this define is to capture that so
 * that we always generate an event.
 */
#ifdef __UCLIBC__
#define ALERT_EVENT_THREAD_ON_SET 0
#else
#define ALERT_EVENT_THREAD_ON_SET 1
#endif

using namespace qcc;
using namespace std;

namespace ajn {
namespace services {

ALSADevice::ALSADevice(const char* deviceName, const char* mixerName)
    : mAudioDeviceName(deviceName), mAudioMixerName(mixerName),
    mMutex(new qcc::Mutex()), mMute(false), mVolume(LONG_MAX), mVolumeScale(1.0), mVolumeOffset(0),
    mAudioDeviceHandle(NULL), mAudioMixerHandle(NULL),
    mAudioMixerElementMaster(NULL), mAudioMixerElementPCM(NULL), mAudioMixerThread(NULL),
    mListenersMutex(new qcc::Mutex()) {
}

ALSADevice::~ALSADevice() {
    delete mListenersMutex;
    delete mMutex;
}

bool ALSADevice::Open(const char* format, uint32_t sampleRate, uint32_t numChannels, uint32_t& bufferSize) {
    int err;

    if (mAudioDeviceHandle != NULL) {
        QCC_LogError(ER_FAIL, ("Open: already open"));
        return false;
    }

    uint32_t bitsPerChannel;
    snd_pcm_format_t pcmFormat;

    if (strcmp(format, "s16le") == 0) {
        pcmFormat = SND_PCM_FORMAT_S16_LE;
        bitsPerChannel = 16;
    } else {
        QCC_LogError(ER_FAIL, ("Unsupported audio format: %s", format));
        return false;
    }

    if ((err = snd_pcm_open(&mAudioDeviceHandle, mAudioDeviceName, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("cannot open audio device \"%s\" (%s)", mAudioDeviceName, snd_strerror(err)));
        return false;
    }

    mMutex->Lock();
    snd_pcm_hw_params_t* hw_params = NULL;
#define AUDIO_CLEANUP() \
    if (mAudioDeviceHandle != NULL) { \
        snd_pcm_close(mAudioDeviceHandle); \
        mAudioDeviceHandle = NULL; \
    } \
    if (hw_params != NULL) { \
        snd_pcm_hw_params_free(hw_params); \
        hw_params = NULL; \
    } \
    mMutex->Unlock();

    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("cannot allocate hardware parameter structure (%s)", snd_strerror(err)));
        AUDIO_CLEANUP();
        return false;
    }

    if ((err = snd_pcm_hw_params_any(mAudioDeviceHandle, hw_params)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("cannot initialize hardware parameter structure (%s)", snd_strerror(err)));
        AUDIO_CLEANUP();
        return false;
    }

    if ((err = snd_pcm_hw_params_set_access(mAudioDeviceHandle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("cannot set access type (%s)", snd_strerror(err)));
        AUDIO_CLEANUP();
        return false;
    }

    if ((err = snd_pcm_hw_params_set_format(mAudioDeviceHandle, hw_params, pcmFormat)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("cannot set sample format (%s)", snd_strerror(err)));
        AUDIO_CLEANUP();
        return false;
    }

    if ((err = snd_pcm_hw_params_set_rate(mAudioDeviceHandle, hw_params, sampleRate, 0)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("cannot set sample rate (%s)", snd_strerror(err)));
        AUDIO_CLEANUP();
        return false;
    }

    if ((err = snd_pcm_hw_params_set_channels(mAudioDeviceHandle, hw_params, numChannels)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("cannot set channel count (%s)", snd_strerror(err)));
        AUDIO_CLEANUP();
        return false;
    }

    uint32_t bytesPerFrame = (bitsPerChannel >> 3) * numChannels;
    snd_pcm_uframes_t bs = 4096 * bytesPerFrame;
    if ((err = snd_pcm_hw_params_set_buffer_size(mAudioDeviceHandle, hw_params, bs)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("snd_pcm_hw_params_set_buffer_size failed: %s", snd_strerror(err)));
    }

    if ((err = snd_pcm_hw_params(mAudioDeviceHandle, hw_params)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("cannot set parameters (%s)", snd_strerror(err)));
        AUDIO_CLEANUP();
        return false;
    }

    snd_pcm_hw_params_get_buffer_size(hw_params, &bs);
    bufferSize = (uint32_t)bs;

    mHardwareCanPause = snd_pcm_hw_params_can_pause(hw_params) == 1;

    snd_pcm_hw_params_free(hw_params);

#define MIXER_CLEANUP() \
    if (mAudioMixerHandle != NULL) { \
        snd_mixer_close(mAudioMixerHandle); \
        mAudioMixerHandle = NULL; \
    }

    if ((err = snd_mixer_open(&mAudioMixerHandle, 0)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("mixer open failed: %s", snd_strerror(err)));
    }

    if (mAudioMixerHandle != NULL && (err = snd_mixer_attach(mAudioMixerHandle, mAudioMixerName)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("mixer attach \"%s\" failed: %s", mAudioMixerName, snd_strerror(err)));
        MIXER_CLEANUP();
    }

    if (mAudioMixerHandle != NULL && (err = snd_mixer_selem_register(mAudioMixerHandle, NULL, NULL)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("mixer selem register failed: %s", snd_strerror(err)));
        MIXER_CLEANUP();
    }

    if (mAudioMixerHandle != NULL && (err = snd_mixer_load(mAudioMixerHandle)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("mixer load failed: %s", snd_strerror(err)));
        MIXER_CLEANUP();
    }

    if (mAudioMixerHandle != NULL) {
        snd_mixer_elem_t* elem = NULL;
        snd_mixer_selem_id_t* sid = NULL;
        snd_mixer_selem_id_alloca(&sid);
        for (elem = snd_mixer_first_elem(mAudioMixerHandle); elem != NULL; elem = snd_mixer_elem_next(elem)) {
            snd_mixer_selem_get_id(elem, sid);
            const char* controlName = snd_mixer_selem_id_get_name(sid);
            if (0 == strcmp(controlName, "PCM")) {
                mAudioMixerElementPCM = elem;
            } else if (0 == strcmp(controlName, "Master")) {
                mAudioMixerElementMaster = elem;
            } else if (0 == strcmp(controlName, "Left Mixer") || 0 == strcmp(controlName, "Right Mixer")) {
                if (snd_mixer_selem_has_playback_switch(elem)) {
                    if ((err = snd_mixer_selem_set_playback_switch_all(elem, 1)) < 0) {
                        QCC_LogError(ER_OS_ERROR, ("set playback switch failed: %s", snd_strerror(err)));
                    }
                }
            }
        }

        mMinVolume = 0;
        mMaxVolume = 0;
        if (mAudioMixerElementMaster != NULL) {
            if ((err = snd_mixer_selem_get_playback_volume_range(mAudioMixerElementMaster, &mMinVolume, &mMaxVolume)) < 0) {
                QCC_LogError(ER_OS_ERROR, ("get playback Master volume range failed: %s", snd_strerror(err)));
                mAudioMixerElementMaster = NULL;
                mAudioMixerElementPCM = NULL;
            }
        } else if (mAudioMixerElementPCM != NULL) {
            if ((err = snd_mixer_selem_get_playback_volume_range(mAudioMixerElementPCM, &mMinVolume, &mMaxVolume)) < 0) {
                QCC_LogError(ER_OS_ERROR, ("get playback PCM volume range failed: %s", snd_strerror(err)));
                mAudioMixerElementPCM = NULL;
            }
        }
        if ((mMaxVolume - mMinVolume) > (INT16_MAX - INT16_MIN)) {
            mVolumeScale = (double)(mMaxVolume - mMinVolume) / (INT16_MAX - INT16_MIN);
            mVolumeOffset = INT16_MIN - mMinVolume;
        }

        GetMute(mMute);
        GetVolume(mVolume);
        StartAudioMixerThread();
    }

    mMutex->Unlock();
    return true;
}

void ALSADevice::Close(bool drain) {
    QCC_DbgTrace(("%s(drain=%d)", __FUNCTION__, drain));

    if (mAudioDeviceHandle != NULL) {
        mMutex->Lock();
        if (mAudioMixerHandle != NULL) {
            StopAudioMixerThread();
            snd_mixer_close(mAudioMixerHandle);
            mAudioMixerElementMaster = NULL;
            mAudioMixerElementPCM = NULL;
            mAudioMixerHandle = NULL;
        }
        if (drain) {
            snd_pcm_drain(mAudioDeviceHandle);
        }
        snd_pcm_close(mAudioDeviceHandle);
        mAudioDeviceHandle = NULL;
        mMutex->Unlock();
    }
}

bool ALSADevice::Pause() {
    if (!mAudioDeviceHandle) {
        return false;
    }

    bool hasPaused = false;
    mMutex->Lock();

    if (mHardwareCanPause) {
        int err = snd_pcm_pause(mAudioDeviceHandle, 1);
        hasPaused = err == 0;
        if (!hasPaused) {
            QCC_LogError(ER_OS_ERROR, ("pause failed (%s)", snd_strerror(err)));
        }
    }

    if (!hasPaused) {
        int err = snd_pcm_drop(mAudioDeviceHandle);
        if (err < 0) {
            QCC_LogError(ER_OS_ERROR, ("drop failed (%s)", snd_strerror(err)));
        }
        err = snd_pcm_prepare(mAudioDeviceHandle);
        if (err < 0) {
            QCC_LogError(ER_OS_ERROR, ("prepare failed (%s)", snd_strerror(err)));
        }
        hasPaused = err == 0;
    }

    mMutex->Unlock();
    return hasPaused;
}

bool ALSADevice::Play() {
    if (!mAudioDeviceHandle) {
        return false;
    }

    bool success = true;
    mMutex->Lock();

    // If not paused then no need to do anything, ALSA will start playing on write
    if (snd_pcm_state(mAudioDeviceHandle) == SND_PCM_STATE_PAUSED) {
        int err = snd_pcm_pause(mAudioDeviceHandle, 0);
        if (err < 0) {
            QCC_LogError(ER_OS_ERROR, ("resume failed (%s)", snd_strerror(err)));
            success = false;
        }
    }

    mMutex->Unlock();
    return success;
}

bool ALSADevice::Recover() {
    if (!mAudioDeviceHandle) {
        return false;
    }

    bool success = false;
    mMutex->Lock();

    if (snd_pcm_state(mAudioDeviceHandle) == SND_PCM_STATE_XRUN) {
        int err = snd_pcm_drop(mAudioDeviceHandle);
        if (err < 0) {
            QCC_LogError(ER_OS_ERROR, ("drop failed (%s)", snd_strerror(err)));
        }

        err = snd_pcm_prepare(mAudioDeviceHandle);
        if (err < 0) {
            QCC_LogError(ER_OS_ERROR, ("prepare after underrun failed (%s)", snd_strerror(err)));
        }

        success = true;
    }

    mMutex->Unlock();
    return success;
}

uint32_t ALSADevice::GetDelay() {
    if (!mAudioDeviceHandle) {
        return 0;
    }

    uint32_t delay = 0;
    mMutex->Lock();

    snd_pcm_sframes_t delayInFrames = 0;
    if (snd_pcm_delay(mAudioDeviceHandle, &delayInFrames) == 0) {
        delay = delayInFrames > 0 ? (uint32_t)delayInFrames : 0;
    }

    mMutex->Unlock();
    return delay;
}

uint32_t ALSADevice::GetFramesWanted() {
    if (!mAudioDeviceHandle) {
        return 0;
    }

    mMutex->Lock();
    snd_pcm_sframes_t framesWanted = snd_pcm_avail_update(mAudioDeviceHandle);
    mMutex->Unlock();
    return framesWanted > 0 ? (uint32_t)framesWanted : 0;
}

bool ALSADevice::Write(const uint8_t* buffer, uint32_t bufferSizeInFrames) {
    if (!mAudioDeviceHandle) {
        return false;
    }

    mMutex->Lock();
    snd_pcm_sframes_t err = snd_pcm_writei(mAudioDeviceHandle, buffer, bufferSizeInFrames);
    if (err < 0) {
        err = snd_pcm_recover(mAudioDeviceHandle, err, 0);
    }
    if (err < 0) {
        QCC_LogError(ER_OS_ERROR, ("write to audio interface failed (%s)", snd_strerror(err)));
    }
    if (err > 0 && err != (snd_pcm_sframes_t)bufferSizeInFrames) {
        QCC_LogError(ER_OS_ERROR, ("short write (expected %u, wrote %li)", bufferSizeInFrames, err));
    }
    mMutex->Unlock();
    return err > 0;
}

bool ALSADevice::GetMute(bool& mute) {
    snd_mixer_elem_t* elem = mAudioMixerElementMaster ? mAudioMixerElementMaster : mAudioMixerElementPCM;
    if (!elem) {
        return false;
    }

    bool success = true;
    mMutex->Lock();

    int on;
    int err;
    if ((err = snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &on)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("cannot get playback switch (%s)", snd_strerror(err)));
        success = false;
    }
    mute = !on;

    mMutex->Unlock();
    return success;
}

bool ALSADevice::SetMute(bool mute) {
    snd_mixer_elem_t* elem = mAudioMixerElementMaster ? mAudioMixerElementMaster : mAudioMixerElementPCM;
    if (!elem) {
        return false;
    }

    bool success = true;
    mMutex->Lock();

    int on = !mute;
    int err;
    if ((err = snd_mixer_selem_set_playback_switch_all(elem, on)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("cannot set playback switch (%s)", snd_strerror(err)));
        success = false;
    }
#if ALERT_EVENT_THREAD_ON_SET
    if (success && mAudioMixerThread != NULL) {
        mAudioMixerThread->Alert();
    }
#endif

    mMutex->Unlock();
    return success;
}

int16_t ALSADevice::ALSAToAllJoyn(long volume) {
    return (1.0 / mVolumeScale) * volume + mVolumeOffset;
}

long ALSADevice::AllJoynToALSA(int16_t volume) {
    return mVolumeScale * (volume - mVolumeOffset);
}

bool ALSADevice::GetVolume(long& volume) {
    snd_mixer_elem_t* elem = mAudioMixerElementMaster ? mAudioMixerElementMaster : mAudioMixerElementPCM;
    if (!elem) {
        return false;
    }

    bool success = true;
    mMutex->Lock();

    int err;
    if ((err = snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &volume)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("get playback volume failed: %s", snd_strerror(err)));
        success = false;
    }

    mMutex->Unlock();
    return success;
}

bool ALSADevice::GetVolumeRange(int16_t& low, int16_t& high, int16_t& step) {
    low = ALSAToAllJoyn(mMinVolume);
    high = ALSAToAllJoyn(mMaxVolume);
    step = 1;
    return true;
}

bool ALSADevice::GetVolume(int16_t& volume) {
    long value;
    if (GetVolume(value)) {
        volume = ALSAToAllJoyn(value);
        return true;
    }
    return false;
}

bool ALSADevice::SetVolume(int16_t volume) {
    snd_mixer_elem_t* elem = mAudioMixerElementMaster ? mAudioMixerElementMaster : mAudioMixerElementPCM;
    if (!elem) {
        return false;
    }

    bool success = true;
    mMutex->Lock();

    long value = AllJoynToALSA(volume);

    int err;
    if ((err = snd_mixer_selem_set_playback_volume_all(elem, value)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("set playback volume failed: %s", snd_strerror(err)));
        success = false;
    }
#if ALERT_EVENT_THREAD_ON_SET
    if (success && mAudioMixerThread != NULL) {
        mAudioMixerThread->Alert();
    }
#endif

    mMutex->Unlock();
    return success;
}

void ALSADevice::StartAudioMixerThread() {
    if (mAudioMixerThread == NULL) {
        mAudioMixerThread = new Thread("AudioMixer", &AudioMixerThread);
        mAudioMixerThread->Start(this);
    }
}

void ALSADevice::StopAudioMixerThread() {
    if (mAudioMixerThread != NULL) {
        mAudioMixerThread->Stop();
        mAudioMixerThread->Join();
        delete mAudioMixerThread;
        mAudioMixerThread = NULL;
    }
}

ThreadReturn ALSADevice::AudioMixerThread(void* arg) {
    ALSADevice* ad = reinterpret_cast<ALSADevice*>(arg);
    Thread* selfThread = Thread::GetThread();
    int err;

    ad->mMutex->Lock();
    if (ad->mAudioMixerElementMaster != NULL) {
        snd_mixer_elem_set_callback_private(ad->mAudioMixerElementMaster, ad);
        snd_mixer_elem_set_callback(ad->mAudioMixerElementMaster, &AudioMixerEvent);
    } else if (ad->mAudioMixerElementPCM != NULL) {
        snd_mixer_elem_set_callback_private(ad->mAudioMixerElementPCM, ad);
        snd_mixer_elem_set_callback(ad->mAudioMixerElementPCM, &AudioMixerEvent);
    }

    Event& stopEvent = selfThread->GetStopEvent();
    vector<Event*> waitEvents, signaledEvents;

    int count = snd_mixer_poll_descriptors_count(ad->mAudioMixerHandle);
    struct pollfd* pfds = new struct pollfd[count];
    if ((err = snd_mixer_poll_descriptors(ad->mAudioMixerHandle, pfds, count)) < 0) {
        QCC_LogError(ER_OS_ERROR, ("cannot open audio device (%s)", snd_strerror(err)));
        delete[] pfds;
        ad->mMutex->Unlock();
        return NULL;
    }
    for (int i = 0; i < count; ++i) {
        waitEvents.push_back(new Event(pfds[i].fd, Event::IO_READ, false));
    }
    waitEvents.push_back(&stopEvent);
    delete[] pfds;
    ad->mMutex->Unlock();

    while (!selfThread->IsStopping()) {
        QStatus status = Event::Wait(waitEvents, signaledEvents);
        if (status != ER_OK) {
            QCC_LogError(status, ("Event wait failed"));
            break;
        }

        if (find(signaledEvents.begin(), signaledEvents.end(), &stopEvent) != signaledEvents.end()) {
            if (selfThread->IsStopping()) {
                // Thread has been instructed to stop.
                break;
            } else {
                // Thread has been instructed to explicitly poll the state.
                selfThread->GetStopEvent().ResetEvent();
                ad->mMutex->Lock();
                AudioMixerEvent(ad->mAudioMixerElementMaster ? ad->mAudioMixerElementMaster : ad->mAudioMixerElementPCM,
                                SND_CTL_EVENT_MASK_VALUE);
                ad->mMutex->Unlock();
            }
        } else {
            ad->mMutex->Lock();
            snd_mixer_handle_events(ad->mAudioMixerHandle);
            ad->mMutex->Unlock();
        }

        signaledEvents.clear();
    }

    for (int i = 0; i < count; ++i) {
        Event* eventFd = waitEvents[i];
        delete eventFd;
    }
    return NULL;
}

/*
 * Called with the lock.
 */
int ALSADevice::AudioMixerEvent(snd_mixer_elem_t* elem, unsigned int mask) {
    ALSADevice* ad = reinterpret_cast<ALSADevice*>(snd_mixer_elem_get_callback_private(elem));
    if (!ad) {
        return 0;
    }

    if (mask == SND_CTL_EVENT_MASK_REMOVE) {
        // Ignore

    } else if (mask & SND_CTL_EVENT_MASK_VALUE) {
        bool oldMute = ad->mMute;
        ad->GetMute(ad->mMute);
        if (oldMute != ad->mMute) {
            ad->mListenersMutex->Lock();
            ALSADevice::Listeners::iterator it = ad->mListeners.begin();
            while (it != ad->mListeners.end()) {
                AudioDeviceListener* listener = *it;
                listener->MuteChanged(ad->mMute);
                it = ad->mListeners.upper_bound(listener);
            }
            ad->mListenersMutex->Unlock();
        }

        long oldVolume = ad->mVolume;
        ad->GetVolume(ad->mVolume);
        if (oldVolume != ad->mVolume) {
            ad->mListenersMutex->Lock();
            ALSADevice::Listeners::iterator it = ad->mListeners.begin();
            while (it != ad->mListeners.end()) {
                AudioDeviceListener* listener = *it;
                int16_t volume;
                ad->GetVolume(volume);
                listener->VolumeChanged(volume);
                it = ad->mListeners.upper_bound(listener);
            }
            ad->mListenersMutex->Unlock();
        }
    }

    return 0;
}

void ALSADevice::AddListener(AudioDeviceListener* listener) {
    mListenersMutex->Lock();
    mListeners.insert(listener);
    mListenersMutex->Unlock();
}

void ALSADevice::RemoveListener(AudioDeviceListener* listener) {
    mListenersMutex->Lock();
    Listeners::iterator it = mListeners.find(listener);
    if (it != mListeners.end()) {
        mListeners.erase(it);
    }
    mListenersMutex->Unlock();
}

}
}
