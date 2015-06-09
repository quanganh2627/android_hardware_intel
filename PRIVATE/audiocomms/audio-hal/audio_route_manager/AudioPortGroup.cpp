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
#define LOG_TAG "AudioPortGroup"

#include "AudioPortGroup.hpp"
#include "AudioPort.hpp"
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using std::string;
using audio_comms::utilities::Log;

namespace intel_audio
{

AudioPortGroup::AudioPortGroup(const string &name, uint32_t groupId)
    : RoutingElement(name, groupId),
      mPortList(0)
{
}

AudioPortGroup::~AudioPortGroup()
{
}

void AudioPortGroup::addPortToGroup(AudioPort *port)
{
    AUDIOCOMMS_ASSERT(port != NULL, "Invalid port requested");

    mPortList.push_back(port);

    // Give the pointer on Group port back to the port
    port->addGroupToPort(this);

    Log::Verbose() << __FUNCTION__ << ": added " << port->getId() << " to port group";
}

void AudioPortGroup::blockMutualExclusivePort(const AudioPort *port)
{
    AUDIOCOMMS_ASSERT(port != NULL, "Invalid port requested");

    Log::Verbose() << __FUNCTION__ << ": of port " << port->getId();

    PortListIterator it;

    // Find the applicable route for this route request
    for (it = mPortList.begin(); it != mPortList.end(); ++it) {

        AudioPort *itPort = *it;
        if (port != itPort) {

            itPort->setBlocked(true);
        }
    }
}

} // namespace intel_audio
