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

#include "Interface.h"
#include "StreamRouteConfig.hpp"
#include <string>

namespace intel_audio
{

class StreamRouteConfig;

struct IRouteInterface : public NInterfaceProvider::IInterface
{
    INTERFACE_NAME("RouteInterface");

    /**
     * Add a port to route manager.
     * Called at audio platform discovery.
     *
     * @param[in] name: Port name
     * @param[in] portId: port identifier
     */
    virtual void addPort(const std::string &name, uint32_t portId) = 0;

    /**
     * Add a port group and / or a port member of this port group.
     * Called at audio platform discovery.
     * Group of port represents ports that are mutual exclusive. Groups allows to apply
     * protection of port while a member of the group is in use or a port is in an unknown state.
     * In general, Groups of port represents all port connected to a shared I2S bus.
     *
     * @param[in] name: Group port name
     * @param[in] groupId: gorup port identifier
     * @param[in] portMember: port member of the group identified by its name.
     */
    virtual void addPortGroup(const std::string &name,
                              uint32_t groupId,
                              const std::string &portMember) = 0;

    /**
     * Add an Audio Route to route manager.
     * Called at audio platform discovery.
     *
     * @param[in] name: route name
     * @param[in] routeId: route identifier.
     * @param[in] portSrc: source port used by route, may be null if no protection needed.
     * @param[in] portDst: destination port used by route, may be null if no protection needed.
     * @param[in] isOut: route direction (true for output, false for input)
     */
    virtual void addAudioRoute(const std::string &name,
                               uint32_t routeId,
                               const std::string &portSrc, const std::string &portDst,
                               bool isOut) = 0;

    /**
     * Add an Audio Stream Route to route manager.
     * A stream route is a route that is in charge of opening a tiny alsa device and will
     * forward the handle to the stream applicable on this route.
     * Called at audio platform discovery.
     *
     * @param[in] name: route name
     * @param[in] routeId: route identifier.
     * @param[in] portSrc: source port used by route, may be null if no protection needed.
     * @param[in] portDst: destination port used by route, may be null if no protection needed.
     * @param[in] isOut: route direction (true for output, false for input)
     */
    virtual void addAudioStreamRoute(const std::string &name,
                                     uint32_t routeId,
                                     const std::string &portSrc, const std::string &portDst,
                                     bool isOut) = 0;

    /**
     * Update the configuration of a stream route.
     * Configuration is not only the card, device to open, but also the pcm configuration to use.
     *
     * @param[in] name: route name
     * @param[in] config: Route configuration.
     */
    virtual void updateStreamRouteConfig(const std::string &name,
                                         const StreamRouteConfig &config) = 0;

    /**
     * Add an HW effect supported by a route.
     * It sets the capability of a route to provide audio effect. The route manager will always
     * prefer using HW effect when supported by the route than using SW effects.
     *
     * @param[in] name: name of the route supporting the HW effect.
     * @param[in] name: name of the Effect supported by the audio route.
     */
    virtual void addRouteSupportedEffect(const std::string &name, const std::string &effect) = 0;

    /**
     * Sets the applicable attribute of an audio route.
     * Based upon settings file of the Route Manager plugin, this function informs the route
     * manager whether a route is applicable or not.
     *
     * @param[in] name: name of the route applicable.
     * @param[in] isApplicable: true if applicable, false otherwise.
     */
    virtual void setRouteApplicable(const std::string &name, bool isApplicable) = 0;

    /**
     * Sets the need reconfigure attribute of an audio route.
     * Based upon settings file of the Route Manager plugin, this function informs the route
     * manager whether a route needs to be reconfigure or not.
     *
     * @param[in] name: name of the route that needs reconfiguration.
     * @param[in] needReconfigure: reconfiguration flag.
     */
    virtual void setRouteNeedReconfigure(const std::string &name,
                                         bool needReconfigure) = 0;

    /**
     * Sets the need reroute attribute of an audio route.
     * Based upon settings file of the Route Manager plugin, this function informs the route
     * manager whether a route needs to be closed/reopened or not.
     *
     * @param[in] name: name of the route that needs rerouting.
     * @param[in] needReroute: rerouting flag.
     */
    virtual void setRouteNeedReroute(const std::string &name,
                                     bool needReroute) = 0;

    /**
     * Sets the blocked attribute of an audio port.
     * Based upon settings file of the Route Manager plugin, this function informs the route
     * manager whether a port is blocked or not.
     *
     * @param[in] name: name of the port to be blocked/unblocked.
     * @param[in] isBlocked: true if blocked, false otherwise.
     */
    virtual void setPortBlocked(const std::string &name, bool isBlocked) = 0;

    /**
     * Adds a criterion type for Audio PFW instance to route manager.
     * Called at audio platform discovery.
     *
     * @param[in] name: name of the criterion type.
     * @param[in] isInclusive: true if criterion is inclusive, false if exclusive.
     *
     * @return true if criterion type added, false if criterion type is already added.
     */
    virtual bool addAudioCriterionType(const std::string &name, bool isInclusive) = 0;

    /**
     * Adds a criterion type value pair for Audio PFW instance to route manager.
     * Called at audio platform discovery.
     *
     * @param[in] name: name of the criterion type.
     * @param[in] literal: name of the criterion value pair.
     * @param[in] value: value of the criterion value pair.
     */
    virtual void addAudioCriterionTypeValuePair(const std::string &name,
                                                const std::string &literal,
                                                uint32_t value) = 0;

    /**
     * Adds a criterion for Audio PFW instance to route manager.
     * Called at audio platform discovery.
     *
     * @param[in] name: name of the criterion.
     * @param[in] criteriaType: name of the criterion type used for this criterion.
     * @param[in] defaultLiteralValue: default literal value of the criterion.
     *
     * @return true if criterion type added, false if criterion type is already added.
     */
    virtual void addAudioCriterion(const std::string &name, const std::string &criterionType,
                                   const std::string &defaultLiteralValue = "") = 0;

    /**
     * Sets a parameter to a value.
     * Parameters of the route manager plugin will feed the criteria of the audio parameter
     * framework.
     *
     * @param[in] name: parameter name (which matches the criterion).
     * @param[in] name: value to set.
     */
    virtual void setParameter(const std::string &name, uint32_t value) = 0;
};

} // namespace intel_audio
