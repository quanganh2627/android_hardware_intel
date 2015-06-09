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
#define LOG_TAG "RouteManager/Route"

#include "AudioPort.hpp"
#include "AudioRoute.hpp"
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using std::string;
using audio_comms::utilities::Log;

namespace intel_audio
{

AudioRoute::AudioRoute(const string &name, uint32_t routeId)
    : RoutingElement(name, routeId),
      mIsUsed(false),
      mPreviouslyUsed(false),
      mIsApplicable(false),
      mRoutingStageRequested(0)
{
    mPort[EPortSource] = NULL;
    mPort[EPortDest] = NULL;
}

AudioRoute::~AudioRoute()
{
}


void AudioRoute::addPort(AudioPort *port)
{
    AUDIOCOMMS_ASSERT(port != NULL, "Invalid port requested");

    Log::Verbose() << __FUNCTION__ << ": " << port->getId() << " to route " << getName();

    port->addRouteToPortUsers(this);
    if (!mPort[EPortSource]) {

        mPort[EPortSource] = port;
    } else {
        mPort[EPortDest] = port;
    }
}

void AudioRoute::resetAvailability()
{
    mBlocked = false;
    mPreviouslyUsed = mIsUsed;
    mIsUsed = false;
}

bool AudioRoute::isApplicable() const
{
    Log::Verbose() << __FUNCTION__ << ": " << getName()
                   << "!isBlocked()=" << !isBlocked() << " && _isApplicable=" << mIsApplicable;
    return !isBlocked() && mIsApplicable;
}

void AudioRoute::setUsed(bool isUsed)
{
    if (!isUsed) {

        return;
    }

    AUDIOCOMMS_ASSERT(isBlocked() != true, "Requested route blocked");

    if (!mIsUsed) {
        Log::Verbose() << __FUNCTION__ << ": route " << getName() << " is now in use in "
                       << (mIsOut ? "PLAYBACK" : "CAPTURE");
        mIsUsed = true;

        // Propagate the in use attribute to the ports
        // used by this route
        for (int i = 0; i < ENbPorts; i++) {

            if (mPort[i]) {

                mPort[i]->setUsed(this);
            }
        }
    }
}

void AudioRoute::setBlocked()
{
    if (!mBlocked) {
        Log::Verbose() << __FUNCTION__ << ": route " << getName() << " is now blocked";
        mBlocked = true;
    }
}

} // namespace intel_audio
