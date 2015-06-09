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

#include "AudioPortGroup.hpp"
#include "RoutingElement.hpp"
#include <list>
#include <string>

namespace intel_audio
{

class AudioPort;

class AudioPortGroup : public RoutingElement
{
private:
    typedef std::list<AudioPort *>::iterator PortListIterator;
    typedef std::list<AudioPort *>::const_iterator PortListConstIterator;

public:
    AudioPortGroup(const std::string &name, uint32_t groupId);
    virtual ~AudioPortGroup();

    /**
     * Add a port to the group.
     *
     * An audio group represents a set of mutual exclusive ports that may be on a shared SSP bus.
     *
     * @param[in] port Port to add as a members of the group.
     */
    void addPortToGroup(AudioPort *port);

    /**
     * Block all the other port from this group except the requester.
     *
     * An audio group represents a set of mutual exclusive ports. Once a port from
     * this group is used, it protects the platform by blocking all the mutual exclusive
     * ports that may be on a shared SSP bus.
     *
     * @param[in] port Port in use that request to block all the members of the group.
     */
    void blockMutualExclusivePort(const AudioPort *port);

private:
    std::list<AudioPort *> mPortList; /**< mutual exlusive ports belonging to this PortGroup. */
};

} // namespace intel_audio
