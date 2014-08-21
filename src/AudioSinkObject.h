/**
 * @file
 * The implementation of the org.alljoyn.Stream.Port.AudioSink interface.
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

#ifndef _AUDIOSINKOBJECT_H
#define _AUDIOSINKOBJECT_H

#ifndef __cplusplus
#error Only include AudioSinkObject.h in C++ code.
#endif

#include "PortObject.h"
#include <alljoyn/audio/AudioCodec.h>
#include <alljoyn/audio/AudioDevice.h>
#include <qcc/Thread.h>
#include <list>
#include <stdint.h>

namespace ajn {
namespace services {

/**
 * A segment of audio data.
 */
struct TimedSamples {
    uint64_t timestamp; /**< The time in nanoseconds since the UNIX
                             epoch to present the data. */
    uint32_t dataSize; /**< The size of data (in bytes). */
    uint32_t offset; /**< The offset into data to read from. */
    uint8_t* data;  /**< The data */
    bool resync; /**< True if the presentation time needs to be
                      resynchronized. */
};

/**
 * A list of audio data segments.
 */
typedef std::list<TimedSamples> TimedSamplesList;

/**
 * The object that implements the org.alljoyn.Stream.Port.AudioSink interface.
 */
class AudioSinkObject : public PortObject, public AudioDeviceListener {
    friend class StreamObject;

  public:
    /**
     * The constructor.
     *
     * @param[in] bus the bus that this object exists on.
     * @param[in] path the object path for object.
     * @param[in] stream the parent stream object.
     * @param[in] audioDevice the audio device object.
     *
     * @remark The supplied bus must not be deleted before this object
     * is deleted. If the caller registers this object on the AllJoyn
     * bus, it must be unregistered from the bus before it is deleted.
     */
    AudioSinkObject(ajn::BusAttachment* bus, const char* path, StreamObject* stream, AudioDevice* audioDevice);

    ~AudioSinkObject();

  protected:
    void Cleanup(bool drain = false);

  private:
    QStatus Get(const char* ifcName, const char* propName, ajn::MsgArg& val);
    void SetProp(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);

    QStatus EmitPlayStateChangedSignal(uint8_t oldState, uint8_t newState);
    QStatus EmitFifoPositionChangedSignal();
    QStatus EmitVolumeChangedSignal(int16_t volume);
    QStatus EmitMuteChangedSignal(bool mute);
    QStatus EmitVolumeControlEnabledChangedSignal();

    void DoConnect(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);
    void Play(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);
    void Pause(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);
    void Flush(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);
    void AdjustVolume(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);
    void AdjustVolumePercent(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);

    void DoPause();
    static qcc::ThreadReturn PauseThread(void* arg);

    void AudioDataSignalHandler(const ajn::InterfaceDescription::Member* member,
                                const char* sourcePath, ajn::Message& msg);

    void MuteChanged(bool mute);
    void VolumeChanged(int16_t volume);

    void StartAudioOutputThread();
    void StopAudioOutputThread();
    static qcc::ThreadReturn AudioOutputThread(void* arg);

    void StartDecodeThread();
    void StopDecodeThread();
    static qcc::ThreadReturn DecodeThread(void* arg);

    void SetPlayState(uint8_t newState);

    size_t GetCombinedBufferSize();
    size_t GetDecodeBufferSize();
    size_t GetBufferSize();
    void ClearBuffer();

  private:
    const ajn::InterfaceDescription::Member* mPlayStateChangedMember;
    const ajn::InterfaceDescription::Member* mFifoPositionChangedMember;
    const ajn::InterfaceDescription::Member* mVolumeChangedMember;
    const ajn::InterfaceDescription::Member* mMuteChangedMember;
    const ajn::InterfaceDescription::Member* mEnabledChangedMember;

    uint8_t mPlayState;

    size_t mMaxBufferSize;
    size_t mFifoLowThreshold;
    qcc::Mutex mBufferMutex;
    TimedSamplesList mBuffers;
    uint32_t mLateChunkCount;

    qcc::Event mDecodeEvent;
    qcc::Thread* mDecodeThread;
    qcc::Mutex mDecodeBufferMutex;
    TimedSamplesList mDecodeBuffers;

    AudioDecoder* mDecoder;

    qcc::Event* mAudioOutputEvent;
    qcc::Thread* mAudioOutputThread;

    uint32_t mSampleRate;
    uint32_t mBytesPerFrame;
    uint32_t mBytesPerSecond;
    uint32_t mChannelsPerFrame;

    AudioDevice* mAudioDevice;
    uint32_t mAudioDeviceBufferSize;
};

}
}

#endif /* _AUDIOSINKOBJECT_H */
