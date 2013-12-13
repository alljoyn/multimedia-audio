/**
 * @file
 * A helper object for discovering sinks.
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

#ifndef _SINKSEARCHER_H
#define _SINKSEARCHER_H

#ifndef __cplusplus
#error Only include SinkSearcher.h in C++ code.
#endif

#include <alljoyn/BusAttachment.h>
#include <map>

namespace ajn {
namespace services {

/**
 * The helper object for discovering sinks.
 */
class SinkSearcher : public ajn::BusListener, public ajn::MessageReceiver {
  public:
    /**
     * The constructor.
     *
     * @remark If Register() is called on this object, then
     * Unregister() must be called before this object is deleted.
     */
    SinkSearcher();

    /**
     * Registers the searcher.
     *
     * @param[in] bus the bus to use.
     *
     * @return ER_OK if successful.
     *
     * @remark The supplied bus must not be deleted before
     * Unregister() is called on this object.
     */
    QStatus Register(ajn::BusAttachment* bus);

    /**
     * Unregisters the searcher.
     */
    void Unregister();

    /**
     * Refreshes the search.
     *
     * This is intended to be kicked off by a user via the UI.
     */
    void Refresh();

  protected:
    /**
     * The description of a discovered sink.
     */
    struct Service {
        qcc::String name; /**< The name of the sink. */
        qcc::String path; /**< The object path of the sink. */
        uint16_t port; /**< The session port of the sink. */
        qcc::String friendlyName; /**< The friendly name of the sink, suitable for display in the UI. */
        bool found; /**< True if the device is found, false if it is lost. */
        Service() : port(0), found(false) { }
    };

    /**
     * Called when a sink is found.
     *
     * @param[in] sink the sink that was found.
     */
    virtual void SinkFound(Service* sink) = 0;

    /**
     * Called when a sink is lost.
     *
     * @param[in] sink the sink that was lost.
     */
    virtual void SinkLost(Service* sink) = 0;

  private:
    void ListenerRegistered(ajn::BusAttachment* bus);
    void FoundAdvertisedName(const char* name, ajn::TransportMask transport, const char* namePrefix);
    void LostAdvertisedName(const char* name, ajn::TransportMask transport, const char* namePrefix);
    void OnAnnounce(const ajn::InterfaceDescription::Member* member, const char* srcPath, ajn::Message& message);

  private:
    /** The sinks that have been found during the lifetime of this helper. */
    std::map<qcc::String, Service> mServices;

    /** The bus this helper was register on. */
    ajn::BusAttachment* mBus;
};

}
}

#endif /* _SINKSEARCHER_H */
