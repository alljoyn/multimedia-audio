/**
 * @file
 * The implementation of the org.alljoyn.Stream interface.
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

#ifndef _STREAMOBJECT_H
#define _STREAMOBJECT_H

#ifndef __cplusplus
#error Only include StreamObject.h in C++ code.
#endif

#include <alljoyn/about/AboutService.h>
#include <alljoyn/about/PropertyStore.h>
#include <alljoyn/audio/AudioDevice.h>
#include <alljoyn/BusObject.h>

namespace qcc { class Mutex; }

namespace ajn {
namespace services {

class PortObject;

/**
 * The object that implements the org.alljoyn.Stream interface to
 * expose an AudioSink port.
 *
 * When the object is registered on the bus, it will automatically
 * announce itself.
 */
class StreamObject : public ajn::BusObject {
  public:
    /**
     * The constructor.
     *
     * @param[in] bus the bus that this object exists on.
     * @param[in] path the object path for the object.
     * @param[in] audioDevice the audio device object used by the AudioSink port.
     * @param[in] sp the session port for announcement.
     * @param[in] props the properties for announcement, including the
     *                  friendly name.
     *
     * @remark The supplied bus must not be deleted before this object
     * is deleted. If the caller registers this object on the AllJoyn
     * bus, it must be unregistered from the bus before it is deleted.
     */
    StreamObject(ajn::BusAttachment* bus, const char* path, AudioDevice* audioDevice, ajn::SessionPort sp, PropertyStore* props);

    ~StreamObject();

    /**
     * Registers the stream object.
     *
     * @param[in] bus the bus to use.
     *
     * @return ER_OK if successful.
     */
    QStatus Register(ajn::BusAttachment* bus);

    /**
     * Unregisters the stream object.
     */
    void Unregister();

    /// @cond ALLJOYN_DEV
    /**
     * @internal Gets the SessionId used by current owner.
     *
     * @return the SessionId used by current owner
     */
    uint32_t GetSessionId() { return mSessionId; }

    /**
     * @internal Gets the current time of the stream clock.
     *
     * @return the time in nanoseconds since the UNIX epoch.
     */
    uint64_t GetCurrentTimeNanos();

    /**
     * @internal Sleeps the calling thread until the time on the stream clock is
     * reached.
     *
     * @param[in] timeNanos the time in nanoseconds since the UNIX
     *                      epoch.
     */
    void SleepUntilTimeNanos(uint64_t timeNanos);

    /**
     * @internal Closes the stream as if the owner had sent a Close
     * method call.
     */
    void Close();
    /// @endcond

  private:
    QStatus Get(const char* ifcName, const char* propName, ajn::MsgArg& val);

    void Open(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);
    void Close(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);
    void SetTime(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);
    void AdjustTime(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);

  private:
    /** The current owner of the AudioSink port */
    const char* mOwner;

    /** SessionId used by current owner */
    uint32_t mSessionId;

    /** The audio device object used by the AudioSink port */
    AudioDevice* mAudioDevice;

    /** Session port for annoncement */
    ajn::SessionPort mSessionPort;

    /** Helper object used for announcement */
    AboutService* mAbout;

    /** Object paths for ports */
    const char* mAudioSinkObjectPath;
    const char* mImageSinkObjectPath;
    const char* mMetadataSinkObjectPath;

    /** Ports (Audio, Image, Metadata) */
    qcc::Mutex* mPortsMutex;
    std::vector<PortObject*> mPorts;

    /** Delta between local clock and stream clock. */
    int64_t mClockAdjustment;
};

}
}

#endif /* _STREAMOBJECT_H */
