/**
 * @file
 * Audio device access using the Android NDK API.
 */
/******************************************************************************
 * Copyright (c) 2013-2014, AllSeen Alliance. All rights reserved.
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

#ifndef _ANDROIDDEVICE_H
#define _ANDROIDDEVICE_H

#ifndef __cplusplus
#error Only include AndroidDevice.h in C++ code.
#endif

#include <alljoyn/audio/AudioDevice.h>
#include <set>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

namespace qcc { class Mutex; }

namespace ajn {
namespace services {

/**
 * A subclass of AudioDevice that implements access to an audio device using the Android NDK API.
 */
class AndroidDevice : public AudioDevice {
  public:
    AndroidDevice();
    ~AndroidDevice();

    bool Open(const char*format, uint32_t sampleRate, uint32_t numChannels, uint32_t& bufferSize);
    void Close(bool drain = false);
    bool Pause();
    bool Play();
    bool Recover();
    uint32_t GetDelay();
    uint32_t GetFramesWanted();
    bool Write(const uint8_t*buffer, uint32_t bufferSizeInFrames);

    bool GetMute(bool& mute);
    bool SetMute(bool mute);
    bool GetVolumeRange(int16_t& low, int16_t& high, int16_t& step);
    bool GetVolume(int16_t& volume);
    bool SetVolume(int16_t volume);
    bool GetEnabled() { return true; }

    void AddListener(AudioDeviceListener* listener);
    void RemoveListener(AudioDeviceListener* listener);

  private:
    typedef std::set<AudioDeviceListener*> Listeners;

    static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void* context);

    bool mMute;
    int16_t mVolumeValue;
    SLObjectItf mSLEngineObject;
    SLEngineItf mSLEngine;
    SLObjectItf mOutputMixObject;
    SLObjectItf mBufferQueuePlayerObject;
    SLPlayItf mPlay;
    SLVolumeItf mVolume;
    SLAndroidSimpleBufferQueueItf mBufferQueue;

    uint32_t mBytesPerFrame;
    qcc::Mutex* mBufferMutex;
    uint8_t mBuffersAvailable;
    uint8_t** mAudioBuffers;
    uint8_t mBufferIndex;

    qcc::Mutex* mListenersMutex;
    Listeners mListeners;
};

}
}

#endif /* _ANDROIDDEVICE_H */
