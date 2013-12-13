/**
 * @file
 * The implementation of the org.alljoyn.Stream.Port.MetadataSink interface.
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

#ifndef _METADATASINKOBJECT_H
#define _METADATASINKOBJECT_H

#ifndef __cplusplus
#error Only include MetadataSinkObject.h in C++ code.
#endif

#include "PortObject.h"

namespace ajn {
namespace services {

/**
 * The object that implements the org.alljoyn.Stream.Port.MetadataSink interface.
 */
class MetadataSinkObject : public PortObject {
    friend class StreamObject;

  public:
    /**
     * The constructor.
     *
     * @param[in] bus the bus that this object exists on.
     * @param[in] path the object path for object.
     * @param[in] stream the parent stream object.
     *
     * @remark The supplied bus must not be deleted before this object
     * is deleted. If the caller registers this object on the AllJoyn
     * bus, it must be unregistered from the bus before it is deleted.
     */
    MetadataSinkObject(ajn::BusAttachment* bus, const char* path, StreamObject* stream);

  private:
    QStatus Get(const char* ifcName, const char* propName, ajn::MsgArg& val);

    void DoConnect(const ajn::InterfaceDescription::Member* member, ajn::Message& msg);

    void MetadataSignalHandler(const ajn::InterfaceDescription::Member* member,
                               const char* sourcePath, ajn::Message& msg);
};

}
}

#endif /* _METADATASINKOBJECT_H */
