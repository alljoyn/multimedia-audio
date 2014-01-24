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

#include "PortObject.h"

#include <alljoyn/audio/StreamObject.h>
#include <qcc/Debug.h>

#define QCC_MODULE "ALLJOYN_AUDIO"

using namespace ajn;

namespace ajn {
namespace services {

PortObject::PortObject(BusAttachment* bus, const char* path, StreamObject* stream) : BusObject(path),
    mConfiguration(NULL), mNumCapabilities(0), mCapabilities(NULL), mStream(stream) {
    /* Add Port interface */
    const InterfaceDescription* portIntf = bus->GetInterface(PORT_INTERFACE);
    assert(portIntf);
    AddInterface(*portIntf);

    /* Register the method handlers with the object */
    const MethodEntry methodEntries[] = {
        { portIntf->GetMember("Connect"), static_cast<MessageReceiver::MethodHandler>(&PortObject::Connect) }
    };
    QStatus status = AddMethodHandlers(methodEntries, sizeof(methodEntries) / sizeof(methodEntries[0]));
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to register method handlers for PortObject"));
    }

    mOwnershipLostMember = portIntf->GetMember("OwnershipLost");
    assert(mOwnershipLostMember);
}

PortObject::~PortObject() {
    bus->UnregisterAllHandlers(this);

    if (mCapabilities != NULL) {
        for (size_t i = 0; i < mNumCapabilities; i++)
            delete [] mCapabilities[i].parameters;
        delete [] mCapabilities;
        mCapabilities = NULL;
    }
}

void PortObject::Cleanup(bool drain) {
    if (mConfiguration != NULL) {
        delete [] mConfiguration->parameters;
        delete mConfiguration;
        mConfiguration = NULL;
    }
}

QStatus PortObject::EmitOwnershipLostSignal(const char* newOwner) {
    MsgArg newOwnerArg("s", newOwner);

    uint8_t flags = 0;
    QStatus status = Signal(NULL, mStream->GetSessionId(), *mOwnershipLostMember, &newOwnerArg, 1, 0, flags);
    if (status != ER_OK) {
        QCC_LogError(status, ("Failed to emit OwnershipLost signal"));
    }

    return status;
}

QStatus PortObject::Get(const char* ifcName, const char* propName, MsgArg& val) {
    QStatus status = ER_OK;

    QCC_DbgTrace(("GetProperty called for %s.%s", ifcName, propName));

    if (0 == strcmp(ifcName, PORT_INTERFACE)) {
        if (0 == strcmp(propName, "Version")) {
            val.Set("q", INTERFACES_VERSION);
        } else if (0 == strcmp(propName, "Direction")) {
            val.Set("y", mDirection);
        } else if (0 == strcmp(propName, "Capabilities")) {
            MsgArg* tempCapabilitiesArg = new MsgArg[mNumCapabilities];
            for (unsigned int i = 0; i < mNumCapabilities; i++)
                tempCapabilitiesArg[i].Set("(sa{sv})", mCapabilities[i].type.c_str(), mCapabilities[i].numParameters, mCapabilities[i].parameters);
            val.Set("a(sa{sv})", mNumCapabilities, tempCapabilitiesArg);
            val.SetOwnershipFlags(MsgArg::OwnsArgs);
        } else {
            status = ER_BUS_NO_SUCH_PROPERTY;
        }
    } else {
        status = ER_BUS_NO_SUCH_INTERFACE;
    }

    if (status == ER_BUS_NO_SUCH_INTERFACE || status == ER_BUS_NO_SUCH_PROPERTY) {
        return BusObject::Get(ifcName, propName, val);
    }

    return status;
}

void PortObject::Connect(const InterfaceDescription::Member* member, Message& msg) {
    GET_ARGS(3);

    if (mConfiguration != NULL) {
        QCC_LogError(ER_FAIL, ("Already configured"));
        REPLY(ER_FAIL);
        return;
    }

    char* type = NULL;
    size_t numParameters = 0;
    MsgArg* parameters = NULL;
    QStatus status = args[2].Get("(sa{sv})", &type, &numParameters, &parameters);
    if (status != ER_OK) {
        QCC_LogError(status, ("Configure argument error"));
        REPLY(status);
        return;
    }

    if (!IsConfigurable(type, numParameters, parameters)) {
        QCC_LogError(ER_FAIL, ("Configure argument not valid"));
        REPLY(ER_FAIL);
        return;
    }

    mConfiguration = new Capability;
    mConfiguration->type = type;
    mConfiguration->numParameters = numParameters;
    mConfiguration->parameters = new MsgArg[numParameters];
    for (size_t i = 0; i < numParameters; i++)
        mConfiguration->parameters[i] = parameters[i];

    DoConnect(member, msg);
}

bool PortObject::IsConfigurable(char* type, size_t numParameters, MsgArg* parameters) {
    for (size_t i = 0; i < mNumCapabilities; i++) {
        Capability* capability = &mCapabilities[i];
        if (0 == strcmp(capability->type.c_str(), type)) {
            bool parametersMatched = true;

            for (size_t j = 0; j < capability->numParameters; j++) {
                const char* name = capability->parameters[j].v_dictEntry.key->v_string.str;
                MsgArg* p1 = capability->parameters[j].v_dictEntry.val->v_variant.val;
                MsgArg* p2 = GetParameterValue(parameters, numParameters, name);
                if (p2 == NULL) {
                    QCC_LogError(ER_FAIL, ("Configure is missing parameter: %s", name));
                    parametersMatched = false;
                    break;
                }

                bool matchFound = false;
                if (p1->typeId == ALLJOYN_BYTE_ARRAY && p2->typeId == ALLJOYN_BYTE) {
                    for (size_t y = 0; y < p1->v_scalarArray.numElements; y++) {
                        if (p1->v_scalarArray.v_byte[y] == p2->v_byte) {
                            matchFound = true;
                            break;
                        }
                    }
                } else if (p1->typeId == ALLJOYN_UINT16_ARRAY && p2->typeId == ALLJOYN_UINT16) {
                    for (size_t y = 0; y < p1->v_scalarArray.numElements; y++) {
                        if (p1->v_scalarArray.v_uint16[y] == p2->v_uint16) {
                            matchFound = true;
                            break;
                        }
                    }
                } else if (p1->typeId == ALLJOYN_ARRAY &&
                           strcmp(p1->v_array.GetElemSig(), "s") == 0 &&
                           p2->typeId == ALLJOYN_STRING) {
                    size_t count = p1->v_array.GetNumElements();
                    const MsgArg* elements = p1->v_array.GetElements();
                    for (size_t y = 0; y < count; y++) {
                        if (strcmp(elements[y].v_string.str, p2->v_string.str) == 0) {
                            matchFound = true;
                            break;
                        }
                    }
                } else {
                    QCC_LogError(ER_WARNING, ("Configure encountered unknown parameter type: %s vs %s",
                                              p1->Signature().c_str(), p2->Signature().c_str()));
                }

                if (!matchFound) {
                    QCC_LogError(ER_FAIL, ("Configure match not found for parameter: %s", name));
                    parametersMatched = false;
                    break;
                }
            }

            if (parametersMatched) {
                return true;
            }
        }
    }

    return false;
}

MsgArg* PortObject::GetParameterValue(MsgArg* parameters, size_t numParameters, const char* name) {
    for (size_t i = 0; i < numParameters; i++)
        if (0 == strcmp(parameters[i].v_dictEntry.key->v_string.str, name)) {
            return parameters[i].v_dictEntry.val->v_variant.val;
        }
    return NULL;
}

}
}
