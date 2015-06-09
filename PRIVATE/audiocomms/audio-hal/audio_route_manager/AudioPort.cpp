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
#define LOG_TAG "RouteManager/Port"

#include "AudioPort.hpp"
#include "AudioPortGroup.hpp"
#include "AudioRoute.hpp"
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using std::string;
using audio_comms::utilities::Direction;
using audio_comms::utilities::Log;

namespace intel_audio
{

AudioPort::AudioPort(const string &name, uint32_t portIndex)
    : RoutingElement(name, portIndex),
      mPortGroupList(0),
      mIsBlocked(false),
      mIsUsed(false)
{
    mRouteAttached[Direction::Output] = NULL;
    mRouteAttached[Direction::Input] = NULL;
}

AudioPort::~AudioPort()
{
}

void AudioPort::resetAvailability()
{
    mIsUsed = false;
    mRouteAttached[Direction::Output] = NULL;
    mRouteAttached[Direction::Input] = NULL;
}

void AudioPort::addGroupToPort(AudioPortGroup *portGroup)
{
    Log::Verbose() << __FUNCTION__;
    if (portGroup) {
        mPortGroupList.push_back(portGroup);
    }

}

void AudioPort::setBlocked(bool blocked)
{
    if (mIsBlocked == blocked) {

        return;
    }
    Log::Verbose() << __FUNCTION__ << ": port " << getName() << " is blocked";

    mIsBlocked = blocked;

    if (blocked) {

        AUDIOCOMMS_ASSERT(mRouteAttached != NULL, "Attached route is not initialized");

        // Blocks now all route that use this port
        RouteListIterator it;

        // Find the applicable route for this route request
        for (it = mRouteList.begin(); it != mRouteList.end(); ++it) {

            AudioRoute *route = *it;
            Log::Verbose() << __FUNCTION__ << ":  blocking " << route->getName();
            route->setBlocked();
        }
    }
}

void AudioPort::setUsed(AudioRoute *route)
{
    AUDIOCOMMS_ASSERT(route != NULL, "Invalid route requested");

    if (mIsUsed) {

        // Port is already in use, bailing out
        return;
    }
    Log::Verbose() << __FUNCTION__ << ": port " << getName() << " is in use";

    mIsUsed = true;
    mRouteAttached[route->isOut()] = route;

    PortGroupListIterator it;

    for (it = mPortGroupList.begin(); it != mPortGroupList.end(); ++it) {

        AudioPortGroup *portGroup = *it;

        portGroup->blockMutualExclusivePort(this);
    }

    // Block all route using this port as well (except the route in opposite direction
    // with same name/id as full duplex is allowed)

    // Blocks now all route that use this port except the one that uses the port
    RouteListIterator routeIt;

    // Find the applicable route for this route request
    for (routeIt = mRouteList.begin(); routeIt != mRouteList.end(); ++routeIt) {

        AudioRoute *routeUsingThisPort = *routeIt;
        if ((routeUsingThisPort == route) || (route->getId() == routeUsingThisPort->getId())) {

            continue;
        }
        Log::Verbose() << __FUNCTION__ << ":  blocking " << routeUsingThisPort->getName();
        routeUsingThisPort->setBlocked();
    }
}

void AudioPort::addRouteToPortUsers(AudioRoute *route)
{
    AUDIOCOMMS_ASSERT(route != NULL, "Invalid route requested");

    mRouteList.push_back(route);

    Log::Verbose() << __FUNCTION__ << ": added " << route->getName()
                   << " route to " << getName() << " port users";
}

} // namespace intel_audio
