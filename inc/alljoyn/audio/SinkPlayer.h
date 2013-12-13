/**
 * @file
 * Stream audio data to an audio sink.
 */

/******************************************************************************
 * Copyright (c) 2013, doubleTwist Corporation and AllSeen Alliance. All rights reserved.
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

#ifndef _SINKPLAYER_H
#define _SINKPLAYER_H

#ifndef __cplusplus
#error Only include SinkPlayer.h in C++ code.
#endif

#include <alljoyn/audio/DataSource.h>
#include <alljoyn/BusAttachment.h>
#include <list>
#include <map>
#include <set>

namespace qcc {
class Mutex;
class Thread;
}

namespace ajn {
namespace services {

struct SinkInfo;
class SinkSessionListener;
class SignallingObject;

/**
 * The various player states.
 */
struct PlayerState {
    /**
     * The various player states.
     */
    typedef enum {
        IDLE, /**< The starting state. */
        INIT, /**< The data source is set. */
        PLAYING, /**< Sending audio data. */
        PAUSED /**< Suspended sending audio data. */
    } Type;
};

/**
 * Base class for sink events.
 */
class SinkListener {
  public:
    virtual ~SinkListener() { }

    /**
     * Called when a sink has been added.
     *
     * @param[in] name the name of sink that was added.
     */
    virtual void SinkAdded(const char* name) = 0;

    /**
     * Called when a sink failed to be added.
     *
     * @param[in] name the name of sink that failed to be added.
     */
    virtual void SinkAddFailed(const char* name) = 0;

    /**
     * Called when a sink has been removed.
     *
     * @param[in] name the name of sink that was removed.
     * @param[in] lost true if sink was removed because ownership was lost.
     */
    virtual void SinkRemoved(const char* name, bool lost) = 0;

    /**
     * Called when the mute state changes.
     *
     * @param[in] name the name of the sink.
     * @param[in] mute true if mute
     */
    virtual void MuteChanged(const char* name, bool mute) { };

    /**
     * Called when the volume changes.
     *
     * @param[in] name the name of the sink.
     * @param[in] volume the volume.
     */
    virtual void VolumeChanged(const char* name, int16_t volume) { };
};

/**
 * The object that implements streaming of WAV files to an audio sink.
 */
class SinkPlayer : public ajn::MessageReceiver {
    friend class SinkSessionListener;

  public:
    /**
     * The constructor.
     *
     * @param[in] bus the bus to use.
     *
     * @remark The supplied bus must not be deleted before this object
     * is deleted.
     */
    SinkPlayer(ajn::BusAttachment* bus);

    ~SinkPlayer();

    /**
     * Sets the data source to be streamed.
     *
     * @param[in] dataSource the data source.
     *
     * @return true if data source is supported.
     */
    bool SetDataSource(DataSource* dataSource);

    /**
     * Sets the preferred format for streaming.
     *
     * @param[in] format the format to use, either "audio/x-raw" or
     *                   "audio/x-alac" (if ALAC support is built-in).
     *
     * @return true if format is supported.
     *
     * @remark This should be called before any sinks are added via
     * AddSink().
     */
    bool SetPreferredFormat(const char* format);

    /**
     * Adds a listener for sink add/remove events.
     *
     * @param[in] listener the listener to add.
     */
    void AddListener(SinkListener* listener);

    /**
     * Removes a listener for sink add/remove events.
     *
     * @param[in] listener the listener to remove.
     *
     * @see AddListener()
     */
    void RemoveListener(SinkListener* listener);

    /**
     * Adds a sink to the streaming session.
     *
     * @param[in] name the name of the sink.
     * @param[in] port the session port of the sink.
     * @param[in] path the object path of the sink.
     *
     * @return true if the sink was accepted for asynchronous add.
     *
     * @see SinkSearcher::Service
     */
    bool AddSink(const char* name, ajn::SessionPort port, const char* path);

    /**
     * Removes a sink from the streaming session.
     *
     * @param[in] name the name of the sink.
     *
     * @return true if the sink was accepted for asynchronous remove.
     */
    bool RemoveSink(const char* name);

    /**
     * Checks if a sink is part of the streaming session.
     *
     * @param[in] name the name of a sink.
     *
     * @return true if the sink is part of the streaming session.
     */
    bool HasSink(const char* name);

    /**
     * Removes all sinks from the streaming session.
     *
     * @return true if any sinks were scheduled for asynchronous
     * removal.
     */
    bool RemoveAllSinks();

    /**
     * Gets the number of sinks that are part of the streaming session.
     *
     * @return the number of sinks that are part of the streaming session.
     */
    size_t GetSinkCount();

    /**
     * Opens the stream to the sink.
     *
     * @param[in] name the name of the sink.
     *
     * @return true if the stream is opened.
     */
    bool OpenSink(const char* name);

    /**
     * Opens the stream to all sinks that are part of the streaming session.
     *
     * @return true on success.
     */
    bool OpenAllSinks();

    /**
     * Closes the stream to the sink.
     *
     * @param[in] name the name of the sink.
     *
     * @return true if the stream is closed.
     */
    bool CloseSink(const char* name);

    /**
     * Closes the stream to all sinks that are part of the streaming session.
     *
     * @return true on success.
     */
    bool CloseAllSinks();

    /**
     * Checks if player state is PLAYING.
     *
     * @return true if playing.
     */
    bool IsPlaying();

    /**
     * Starts playing.
     *
     * @return true on success.
     *
     * @remark SetDataSource() must be called before Play.
     */
    bool Play();

    /**
     * Pauses playback.
     *
     * @return true on success.
     *
     * @see Play()
     *
     * @remark Play() must have been called before Pause.
     */
    bool Pause();

    /**
     * Gets the mute state of sinks.
     *
     * @param[in] name the name of a sink or NULL to get all sinks.
     * @param[out] mute true if mute, false if unmute.  If name is
     *                   NULL, true only if all sinks are mute.
     *
     * @return true on success.
     */
    bool GetMute(const char* name, bool& mute);

    /**
     * Mutes or unmutes the output of sinks.
     *
     * @param[in] name the name of a sink or NULL to set all sinks.
     * @param[in] mute true to mute, false to unmute.
     *
     * @return true on success.
     */
    bool SetMute(const char* name, bool mute);

    /**
     * Gets the volume range of sinks.
     *
     * @param[in] name the name of a sink.
     * @param[out] low the minimum volume.
     * @param[out] high the maximum volume.
     * @param[out] step the volume step.
     *
     * @return true on success.
     */
    bool GetVolumeRange(const char* name, int16_t& low, int16_t& high, int16_t& step);

    /**
     * Gets the volume of sinks.
     *
     * @param[in] name the name of a sink.
     * @param[out] volume the volume.
     *
     * @return true on success.
     */
    bool GetVolume(const char* name, int16_t& volume);

    /**
     * Sets the volume of sinks.
     *
     * @param[in] name the name of a sink.
     * @param[in] volume the volume.
     *
     * @return true on success.
     */
    bool SetVolume(const char* name, int16_t volume);

  private:

    static void* AddSinkThread(void* arg);
    static void* EmitAudioThread(void* arg);

    bool RemoveSink(const char* name, bool lost);
    bool RemoveSink(ajn::SessionId sessionId, bool lost);
    static void* RemoveSinkThread(void* arg);

    void FlushReplyHandler(ajn::Message& msg, void* context);
    void MuteChangedSignalHandler(const ajn::InterfaceDescription::Member* member, const char* sourcePath, ajn::Message& msg);
    void VolumeChangedSignalHandler(const ajn::InterfaceDescription::Member* member, const char* sourcePath, ajn::Message& msg);

    QStatus CloseSink(SinkInfo* si, bool lost = false);
    void FreeSinkInfo(SinkInfo* si);

    static void* SinkListenerThread(void* arg);

  private:
    typedef std::set<SinkListener*> SinkListeners;
    typedef std::map<qcc::String, qcc::Thread*> ThreadMap;

    SignallingObject* mSignallingObject;
    qcc::Mutex* mSinkListenersMutex;
    SinkSessionListener* mSessionListener;
    ajn::BusAttachment* mMsgBus;
    char* mPreferredFormat;
    char* mCurrentFormat;
    DataSource* mDataSource;
    ajn::MsgArg mChannelsArg;
    ajn::MsgArg mRateArg;
    ajn::MsgArg mFormatArg;
    qcc::Mutex* mSinksMutex;
    std::list<SinkInfo> mSinks;
    qcc::Mutex* mAddThreadsMutex;
    ThreadMap mAddThreads;
    qcc::Mutex* mRemoveThreadsMutex;
    ThreadMap mRemoveThreads;
    qcc::Mutex* mEmitThreadsMutex;
    ThreadMap mEmitThreads;
    PlayerState::Type mState;
    SinkListeners mSinkListeners;
    std::list<ajn::Message> mSinkListenerQueue;
    qcc::Thread* mSinkListenerThread;
};

}
}

#endif /* _SINKPLAYER_H */
