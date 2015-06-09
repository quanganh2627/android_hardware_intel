/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2014 Intel
 * Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or
 * disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */
#pragma once

#include "RoutingElement.hpp"
#include <Direction.hpp>
#include <list>
#include <stdint.h>

namespace intel_audio
{

class AudioPortGroup;
class AudioRoute;

class AudioPort : public RoutingElement
{
private:
    typedef std::list<AudioPortGroup *>::iterator PortGroupListIterator;
    typedef std::list<AudioPortGroup *>::const_iterator PortGroupListConstIterator;
    typedef std::list<AudioRoute *>::iterator RouteListIterator;
    typedef std::list<AudioRoute *>::const_iterator RouteListConstIterator;

public:
    AudioPort(const std::string &name, uint32_t portIndex);
    virtual ~AudioPort();

    /**
     * Blocks a port.
     *
     * Calling this API will prevent from using this port and all routes that
     * use this port as well.
     *
     * @param[in] blocked true if the port needs to be blocked, false otherwise.
     */
    void setBlocked(bool blocked);

    /**
     * Sets a port in use.
     *
     * An audio route is now in use and propagate the in use attribute to the ports that this
     * audio route is using.
     * It parses the list of Group Port in which this port is involved
     * and blocks all ports within these port group that are mutual exclusive with this one.
     *
     * @param[in] route Route that is in use and request to use the port.
     */
    void setUsed(AudioRoute *route);

    /**
     * Resets the availability of the port.
     *
     * It not only resets the used attribute of the port
     * but also the pointers on the routes using this port.
     * It does not reset exclusive access variable of the route parameter manager (as blocked
     * variable).
     */
    virtual void resetAvailability();

    /**
     * Add a route as a potential user of this port.
     *
     * Called at the construction of the audio platform, it pushes a route to the list of route
     * using this port. It will help propagating the blocking attribute when this port is declared
     * as blocked.
     *
     * @param[in] route Route using this port.
     */
    void addRouteToPortUsers(AudioRoute *route);

    /**
     * Add a group to the port.
     *
     * Called at the construction of the audio platform, it pushes a group in the list of groups
     * this port is belonging to. It will help propagating the blocking attribute when this port is
     * declared as blocked.
     *
     * @param[in] portGroup Group to add.
     */
    void addGroupToPort(AudioPortGroup *portGroup);

private:
    std::list<AudioPortGroup *> mPortGroupList; /**< list of groups this port belongs to. */

    /**
     * routes attached to this port.
     */
    AudioRoute *mRouteAttached[audio_comms::utilities::Direction::_nbDirections];

    std::list<AudioRoute *> mRouteList; /**< list of routes using potentially this port. */

    bool mIsBlocked; /**< blocked attribute, exclusive access to route parameter manager plugin. */
    bool mIsUsed; /**< used attribute. */
};

} // namespace intel_audio
