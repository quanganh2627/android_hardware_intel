/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2014 Intel
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

#include <string>

namespace intel_audio
{

static const char *const gAudioHalConfFilePath =
    "/system/etc/route_criteria.conf";
static const char *const gAudioHalVendorConfFilePath =
    "/vendor/etc/route_criteria.conf";

/**
 * PFW instances tags
 */
static const std::string gAudioConfTag = "Audio";
static const std::string gRouteConfTag = "Route";

/**
 * PFW elements tags
 */
static const std::string gInclusiveCriterionTypeTag = "InclusiveCriterionType";
static const std::string gExclusiveCriterionTypeTag = "ExclusiveCriterionType";
static const std::string gCriterionTag = "Criterion";
static const std::string gRogueParameterTag = "RogueParameter";

/**
 * PFW Elements specific tags
 */
static const std::string gParameterDefaultTag = "Default";
static const std::string gAndroidParameterTag = "Parameter";
static const std::string gMappingTableTag = "Mapping";
static const std::string gTypeTag = "Type";
static const std::string gPathTag = "Path";
static const std::string gUnsignedIntegerTypeTag = "uint32_t";
static const std::string gStringTypeTag = "string";

/**
 * ValueSet tags
 */
static const std::string gValueSet = "ValueSet";
static const std::string gModemValueSet = "ModemValueSet";
static const std::string gInterfaceLibraryName = "InterfaceLibrary";
static const std::string gInterfaceLibraryInstance = "Instance";

} // namespace intel_audio
