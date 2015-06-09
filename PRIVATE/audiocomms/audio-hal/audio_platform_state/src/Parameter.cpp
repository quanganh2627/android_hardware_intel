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

#include "Parameter.hpp"
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using std::string;
using audio_comms::utilities::Log;

namespace intel_audio
{

void Parameter::setMappingValuePair(const string &name, const string &value)
{
    if (mMappingValuesMap.find(name) != mMappingValuesMap.end()) {
        Log::Warning() << __FUNCTION__ << ": parameter value " << name << " already appended";
        return;
    }
    mMappingValuesMap[name] = value;
}

bool Parameter::getLiteralValueFromParam(const string &androidParam, string &literalValue) const
{
    if (mMappingValuesMap.empty()) {

        // No Mapping table has been provided, all value that may take the Android Param
        // will be propagated to the PFW Parameter
        literalValue = androidParam;
        return true;
    }
    if (!isAndroidParameterValueValid(androidParam)) {

        Log::Verbose() << __FUNCTION__ << ": unknown parameter value(" << androidParam
                       << ") for " << mAndroidParameterKey << "";
        return false;
    }
    MappingValuesMapConstIterator it = mMappingValuesMap.find(androidParam);
    AUDIOCOMMS_ASSERT(it != mMappingValuesMap.end(), "Value not valid");
    literalValue = it->second;
    return true;
}

bool Parameter::getParamFromLiteralValue(string &androidParam, const string &literalValue) const
{
    // No Mapping table has been provided, all value that may take the PFW Param
    // will be propagated to the Android Parameter
    if (mMappingValuesMap.empty()) {

        androidParam = literalValue;
        return true;
    }
    for (MappingValuesMapConstIterator it = mMappingValuesMap.begin();
         it != mMappingValuesMap.end();
         ++it) {

        if (it->second == literalValue) {
            androidParam = it->first;
            return true;
        }
    }
    return false;
}

} // namespace intel_audio
