/**
 * @file
 * Audio device access using the Linux ALSA API.
 */

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

#ifndef _ALSADEVICE_H
#define _ALSADEVICE_H

#ifndef __cplusplus
#error Only include ALSADevice.h in C++ code.
#endif

#include <alljoyn/audio/AudioDevice.h>
#include <alsa/asoundlib.h>
#include <set>

namespace qcc {
class Mutex;
class Thread;
}

namespace ajn {
namespace services {

/**
 * A subclass of AudioDevice that implements access to an audio device using the Linux ALSA API.
 */
class ALSADevice : public AudioDevice {
  public:
    /**
     * Creates an ALSA AudioDevice.
     *
     * @param[in] deviceName the name of the ALSA PCM handle.
     * @param[in] mixerName the name of the ALSA mixer HCTL.
     */
    ALSADevice(const char* deviceName, const char* mixerName);
    ~ALSADevice();

    bool Open(const char* format, uint32_t sampleRate, uint32_t numChannels, uint32_t& bufferSize);
    void Close(bool drain = false);
    bool Pause();
    bool Play();
    bool Recover();
    uint32_t GetDelay();
    uint32_t GetFramesWanted();
    bool Write(const uint8_t* buffer, uint32_t bufferSizeInFrames);

    bool GetMute(bool& mute);
    bool SetMute(bool mute);
    bool GetVolumeRange(int16_t& low, int16_t& high, int16_t& step);
    bool GetVolume(int16_t& volume);
    bool SetVolume(int16_t volume);
    void AddListener(AudioDeviceListener* listener);
    void RemoveListener(AudioDeviceListener* listener);
    bool GetEnabled() { return true; }

  private:
    int16_t ALSAToAllJoyn(long volume);
    long AllJoynToALSA(int16_t volume);
    bool GetVolume(long& volume);
    void StartAudioMixerThread();
    void StopAudioMixerThread();
    static void* AudioMixerThread(void* arg);
    static int AudioMixerEvent(snd_mixer_elem_t* elem, unsigned int mask);

  private:
    typedef std::set<AudioDeviceListener*> Listeners;

    const char* mAudioDeviceName;
    const char* mAudioMixerName;
    qcc::Mutex* mMutex;
    bool mMute;
    long mVolume;
    long mMinVolume;
    long mMaxVolume;
    double mVolumeScale;
    long mVolumeOffset;
    snd_pcm_t* mAudioDeviceHandle;
    snd_mixer_t* mAudioMixerHandle;
    snd_mixer_elem_t* mAudioMixerElementMaster;
    snd_mixer_elem_t* mAudioMixerElementPCM;
    bool mHardwareCanPause;
    qcc::Thread* mAudioMixerThread;
    qcc::Mutex* mListenersMutex;
    Listeners mListeners;
};

}
}

#endif /* _ALSADEVICE_H */
