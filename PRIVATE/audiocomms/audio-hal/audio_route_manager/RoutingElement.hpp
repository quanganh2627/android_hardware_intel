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

#include <NonCopyable.hpp>
#include <stdint.h>
#include <string>

namespace intel_audio
{

class RoutingElement : public audio_comms::utilities::NonCopyable
{
public:
    RoutingElement(const std::string &name, uint32_t index);
    virtual ~RoutingElement();

    /**
     * Returns identifier of current routing element
     *
     * @returns string representing the name of the routing element
     */
    const std::string &getName() const { return mName; }

    /**
     * Returns numerical id of current routing element
     *
     * @returns value representing the id of the routing element
     */
    uint32_t getId() const { return mId; }

    virtual void resetAvailability() {}

private:
    /** Unique Identifier of a routing element */
    std::string mName;

    /** More than one routing element can share the same id (full duplex route for example)*/
    uint32_t mId;
};

} // namespace intel_audio
