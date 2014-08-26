/**
 * @file
 * Definitions common to both service and client.
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

#ifndef _SINK_H
#define _SINK_H

#include <alljoyn/audio/Audio.h>
#if defined(QCC_OS_ANDROID)
#include <time.h>
#endif

namespace ajn {
namespace services {

#define STREAM_INTERFACE            "org.alljoyn.Stream" /**< The stream interface name. */
#define CLOCK_INTERFACE             "org.alljoyn.Stream.Clock" /**< The clock interface name. */
#define PORT_INTERFACE              "org.alljoyn.Stream.Port" /**< The stream port interface name. */
#define AUDIO_SINK_INTERFACE        "org.alljoyn.Stream.Port.AudioSink" /**< The audioSink port interface name. */
#define AUDIO_SOURCE_INTERFACE      "org.alljoyn.Stream.Port.AudioSource" /**< The audioSource port interface name. */
#define IMAGE_SINK_INTERFACE        "org.alljoyn.Stream.Port.ImageSink" /**< The imageSink port interface name. */
#define IMAGE_SOURCE_INTERFACE      "org.alljoyn.Stream.Port.ImageSource" /**< The imageSource port interface name. */
#define METADATA_SINK_INTERFACE     "org.alljoyn.Stream.Port.Application.MetadataSink" /**< The metadataSink port interface name. */
#define METADATA_SOURCE_INTERFACE   "org.alljoyn.Stream.Port.Application.MetadataSource" /**< The metadataSource port interface name. */
#define VOLUME_INTERFACE            "org.alljoyn.Control.Volume" /**< The volumeControl interface name. */

#define DIRECTION_SOURCE    0 /**< The value of a source port's Direction property. */
#define DIRECTION_SINK      1 /**< The vaule of a sink port's Direction property. */

/**
 * The state of an AudioSink port.
 */
struct PlayState {
    /**
     * The state of an AudioSink port.
     */
    typedef enum {
        IDLE = 0, /**< No audio data to present. */
        PLAYING = 1, /**< Presenting audio data. */
        PAUSED = 2 /**< Suspended presentation of audio data. */
    } Type;
};

/**
 * Unmarshals a MsgArg into an array of Capabilities.
 */
#define MSGARG_TO_CAPABILITIES(msgArg, capabilities, numCapabilities) { \
        MsgArg* capabilitiesArgs = NULL; \
        msgArg.Get("a(sa{sv})", &numCapabilities, &capabilitiesArgs); \
        capabilities = new Capability[numCapabilities]; \
        for (size_t i = 0; i < numCapabilities; i++) { \
            char* type = NULL; \
            size_t numParameters = 0; \
            MsgArg* parameters = NULL; \
            capabilitiesArgs[i].Get("(sa{sv})", &type, &numParameters, &parameters); \
            capabilities[i].type = type; \
            capabilities[i].numParameters = numParameters; \
            capabilities[i].parameters = parameters; \
        } \
}

/**
 * Marshals a Capability into a MsgArg.
 */
#define CAPABILITY_TO_MSGARG(capability, msgArg) \
    msgArg.Set("(sa{sv})", capability.type.c_str(), capability.numParameters, capability.parameters);

/**
 * Prints the capabilities to stdout.
 */
#define PRINT_CAPABILITIES(capabilities, numCapabilities) { \
        for (size_t i = 0; i < numCapabilities; i++) { \
            QCC_DbgHLPrintf(("Capability type=[%s] numParameters=[%zu]", capabilities[i].type.c_str(), capabilities[i].numParameters)); \
            for (size_t j = 0; j < capabilities[i].numParameters; j++) { \
                MsgArg* key = capabilities[i].parameters[j].v_dictEntry.key; \
                MsgArg* val = capabilities[i].parameters[j].v_dictEntry.val->v_variant.val; \
                switch (val->typeId) { \
                case ALLJOYN_STRING: \
                    QCC_DbgHLPrintf(("\tALLJOYN_STRING \"%s\" = %s", key->v_string.str, val->v_string.str)); \
                    break; \
                case ALLJOYN_BYTE: \
                    QCC_DbgHLPrintf(("\tALLJOYN_BYTE \"%s\" = %u", key->v_string.str, val->v_byte)); \
                    break; \
                case ALLJOYN_UINT16: \
                    QCC_DbgHLPrintf(("\tALLJOYN_UINT16 \"%s\" = %u", key->v_string.str, val->v_uint16)); \
                    break; \
                case ALLJOYN_UINT32: \
                    QCC_DbgHLPrintf(("\tALLJOYN_UINT32 \"%s\" = %u", key->v_string.str, val->v_uint32)); \
                    break; \
                default: break; \
                } \
            } \
        } }

/**
 * Helper macro extracts args from a Message.
 */
#define GET_ARGS(numExpected) \
    size_t numArgs; \
    const MsgArg* args; \
    msg->GetArgs(numArgs, args); \
    if (numArgs != numExpected) { \
        QCC_LogError(ER_FAIL, ("%s invalid number of arguments: %zu", member->name.c_str(), numArgs)); \
        QStatus status = MethodReply(msg, ER_FAIL); \
        if (status != ER_OK) { \
            QCC_LogError(status, ("%s reply failed", member->name.c_str())); } \
        return; \
    }

/**
 * Helper macro replies with an error to a Message.
 */
#define REPLY(error) { \
        QStatus replyStatus = MethodReply(msg, error); \
        if (replyStatus != ER_OK) { \
            QCC_LogError(replyStatus, ("%s reply failed", member->name.c_str())); } \
}

/**
 * Helper macro replies with success to a Message.
 */
#define REPLY_OK() { \
        QStatus replyStatus = MethodReply(msg); \
        if (replyStatus != ER_OK) { \
            QCC_LogError(replyStatus, ("%s reply failed", member->name.c_str())); } \
}

/** The Announce interface name. */
#define ABOUT_INTERFACE "org.alljoyn.About"
/** The Announce interface XML. */
#define ABOUT_XML " \
<interface name=\"" ABOUT_INTERFACE "\"> \
  <signal name=\"Announce\"> \
    <arg name=\"version\" type=\"q\"/> \
    <arg name=\"port\" type=\"q\"/> \
    <arg name=\"objectDescriptions\" type=\"a(oas)\"/> \
    <arg name=\"aboutData\" type=\"a{sv}\"/> \
  </signal> \
  <method name=\"GetAboutData\">\
    <arg name=\"languageTag\" type=\"s\" direction=\"in\"/> \
    <arg name=\"aboutData\" type=\"a{sv}\" direction=\"out\"/> \
  </method> \
  <method name=\"GetObjectDescription\">\
    <arg name=\"Control\" type=\"a(oas)\" direction=\"out\"/> \
  </method> \
  <property name=\"Version\" type=\"q\" access=\"read\"/> \
</interface>"
/** The match rule for receiving Announce signals. */
#define ANNOUNCE_MATCH_RULE "type='signal',interface='" ABOUT_INTERFACE "',member='Announce',sessionless='t'"
/** The Announce key for the friendly name. */
#define ANNOUNCE_DEVICE_NAME "DeviceName"

/** The audio interfaces XML. */
#define INTERFACES_XML " \
<node name=\"\"> \
<interface name=\"org.alljoyn.Stream\"> \
  <property name=\"Version\" type=\"q\" access=\"read\"/> \
  <method name=\"Open\" /> \
  <method name=\"Close\"/> \
</interface> \
<interface name=\"org.alljoyn.Stream.Port\"> \
  <property name=\"Version\" type=\"q\" access=\"read\"/> \
  <signal name=\"OwnershipLost\"> \
    <arg type=\"s\"/> \
  </signal> \
  <property name=\"Direction\" type=\"y\" access=\"read\"/> \
  <property name=\"Capabilities\" type=\"a(sa{sv})\" access=\"read\"/> \
  <method name=\"Connect\"> \
    <arg name=\"host\" type=\"s\" direction=\"in\"/> \
    <arg name=\"path\" type=\"o\" direction=\"in\"/> \
    <arg name=\"configuration\" type=\"(sa{sv})\" direction=\"in\"/> \
  </method> \
</interface> \
<interface name=\"org.alljoyn.Stream.Port.AudioSink\"> \
  <property name=\"Version\" type=\"q\" access=\"read\"/> \
  <signal name=\"PlayStateChanged\"> \
    <arg type=\"y\"/> \
    <arg type=\"y\"/> \
  </signal> \
  <property name=\"FifoSize\" type=\"u\" access=\"read\"/> \
  <property name=\"FifoPosition\" type=\"u\" access=\"read\"/> \
  <property name=\"Delay\" type=\"(uu)\" access=\"read\"/> \
  <signal name=\"FifoPositionChanged\" /> \
  <method name=\"Play\"/> \
  <method name=\"Pause\"> \
    <arg name=\"timeNanos\" type=\"t\" direction=\"in\"/> \
  </method> \
  <method name=\"Flush\"> \
    <arg name=\"timeNanos\" type=\"t\" direction=\"in\"/> \
    <arg name=\"count\" type=\"u\" direction=\"out\"/> \
  </method> \
</interface> \
<interface name=\"org.alljoyn.Stream.Port.AudioSource\"> \
  <property name=\"Version\" type=\"q\" access=\"read\"/> \
  <signal name=\"Data\"> \
    <arg name=\"timestamp\" type=\"t\"/> \
    <arg name=\"bytes\" type=\"ay\"/> \
  </signal> \
</interface> \
<interface name=\"org.alljoyn.Stream.Port.ImageSink\"> \
  <property name=\"Version\" type=\"q\" access=\"read\"/> \
</interface> \
<interface name=\"org.alljoyn.Stream.Port.ImageSource\"> \
  <property name=\"Version\" type=\"q\" access=\"read\"/> \
  <signal name=\"Data\"> \
    <arg name=\"bytes\" type=\"ay\"/> \
  </signal> \
</interface> \
<interface name=\"org.alljoyn.Stream.Port.Application.MetadataSink\"> \
  <property name=\"Version\" type=\"q\" access=\"read\"/> \
</interface> \
<interface name=\"org.alljoyn.Stream.Port.Application.MetadataSource\"> \
  <property name=\"Version\" type=\"q\" access=\"read\"/> \
  <signal name=\"Data\"> \
    <arg name=\"dictionary\" type=\"a{sv}\"/> \
  </signal> \
</interface> \
<interface name=\"org.alljoyn.Control.Volume\"> \
  <property name=\"Version\" type=\"q\" access=\"read\"/> \
  <property name=\"Volume\" type=\"n\" access=\"readwrite\"/> \
  <property name=\"VolumeRange\" type=\"(nnn)\" access=\"read\"/> \
  <property name=\"Mute\" type=\"b\" access=\"readwrite\"/> \
  <signal name=\"VolumeChanged\"> \
    <arg name=\"newVolume\" type=\"n\"/> \
  </signal> \
  <signal name=\"MuteChanged\"> \
    <arg name=\"newMute\" type=\"b\"/> \
  </signal> \
  <method name=\"AdjustVolume\"> \
    <arg name=\"delta\" type=\"n\" direction=\"in\"/> \
  </method> \
  <method name=\"AdjustVolumePercent\"> \
    <arg name=\"change\" type=\"d\" direction=\"in\"/> \
  </method> \
  <property name=\"Enabled\" type=\"b\" access=\"read\"/> \
  <signal name=\"EnabledChanged\"> \
    <arg name=\"enabled\" type=\"b\"/> \
  </signal> \
</interface> \
<interface name=\"org.alljoyn.Stream.Clock\"> \
  <property name=\"Version\" type=\"q\" access=\"read\"/> \
  <method name=\"SetTime\"> \
    <arg name=\"timeNanos\" type=\"t\" direction=\"in\"/> \
  </method> \
  <method name=\"AdjustTime\"> \
    <arg name=\"adjustNanos\" type=\"x\" direction=\"in\"/> \
  </method> \
</interface> \
</node>"

/** The version of audio interfaces implemented. */
#define INTERFACES_VERSION 1

}
}

#endif /* _SINK_H */
