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
#define LOG_TAG "ParameterHelper"

#include "ParameterMgrHelper.hpp"
#include "ParameterMgrPlatformConnector.h"
#include "SelectionCriterionInterface.h"
#include <utilities/Log.hpp>
#include <string>

using audio_comms::utilities::Log;
using std::map;
using std::string;
using std::vector;
using namespace android;

ParameterMgrHelper::ParameterMgrHelper(CParameterMgrPlatformConnector *pfwConnector)
    : mPfwConnector(pfwConnector)
{
}

ParameterMgrHelper::~ParameterMgrHelper()
{
    ParameterHandleMapIterator it;

    for (it = mParameterHandleMap.begin(); it != mParameterHandleMap.end(); ++it) {

        delete it->second;
    }
    mParameterHandleMap.clear();
}

template <>
bool ParameterMgrHelper::setAsTypedValue<uint32_t>(CParameterHandle *parameterHandle,
                                                   const uint32_t &value, string &error)
{
    if (!parameterHandle->setAsInteger(value, error)) {
        Log::Error() << "Unable to set value: " << error
                     << ", from parameter path: " << parameterHandle->getPath();
        return false;
    }
    return true;
}

template <>
bool ParameterMgrHelper::getAsTypedValue<uint32_t>(CParameterHandle *parameterHandle,
                                                   uint32_t &value, string &error)
{
    if (!parameterHandle->getAsInteger(value, error)) {
        Log::Error() << "Unable to get value: " << error
                     << ", from parameter path: " << parameterHandle->getPath();
        return false;
    }
    return true;
}

template <>
bool ParameterMgrHelper::setAsTypedValue<vector<uint32_t> >(CParameterHandle *parameterHandle,
                                                            const vector<uint32_t> &value,
                                                            string &error)
{
    if (!parameterHandle->setAsIntegerArray(value, error)) {
        Log::Error() << "Unable to set value: " << error
                     << ", from parameter path: " << parameterHandle->getPath();
        return false;
    }
    return true;
}

template <>
bool ParameterMgrHelper::setAsTypedValue<string>(CParameterHandle *parameterHandle,
                                                 const string &value, string &error)
{
    if (!parameterHandle->setAsString(value, error)) {
        Log::Error() << "Unable to get value: " << error
                     << ", from parameter path: " << parameterHandle->getPath();
        return false;
    }
    return true;
}

template <>
bool ParameterMgrHelper::getAsTypedValue<string>(CParameterHandle *parameterHandle,
                                                 string &value, string &error)
{
    if (!parameterHandle->getAsString(value, error)) {
        Log::Error() << "Unable to get value: " << error
                     << ", from parameter path: " << parameterHandle->getPath();
        return false;
    }
    return true;
}

bool ParameterMgrHelper::getParameterHandle(CParameterMgrPlatformConnector *pfwConnector,
                                            CParameterHandle * &handle,
                                            const string &path)
{
    if (pfwConnector == NULL || !pfwConnector->isStarted()) {
        Log::Error() << __FUNCTION__ << ": PFW connector is NULL or PFW is not started";
        return false;
    }
    string error;
    handle = pfwConnector->createParameterHandle(path, error);
    if (!handle) {
        Log::Error() << __FUNCTION__ << ": Unable to get handle for " << path << "' '" << error;
        return false;
    }
    return true;
}

CParameterHandle *ParameterMgrHelper::getPlatformParameterHandle(const string &paramPath) const
{
    string platformParamPath;

    // First retrieve the platform dependant parameter path
    if (!getParameterValue<string>(mPfwConnector, paramPath, platformParamPath)) {
        Log::Error() << "Could not retrieve parameter path handler";
        return NULL;
    }
    Log::Debug() << __FUNCTION__ << ": Platform specific parameter path=" << platformParamPath;

    // Initialise handle to NULL to avoid KW "false-positive".
    CParameterHandle *handle = NULL;
    if (!getParameterHandle(mPfwConnector, handle, platformParamPath)) {
        return NULL;
    }
    return handle;
}

CParameterHandle *ParameterMgrHelper::getDynamicParameterHandle(const string &dynamicParamPath)
{
    if (mParameterHandleMap.find(dynamicParamPath) == mParameterHandleMap.end()) {
        Log::Debug() << __FUNCTION__ << ": Dynamic parameter " << dynamicParamPath
                     << " not found in map, get a handle and push it in the map";
        mParameterHandleMap[dynamicParamPath] = getPlatformParameterHandle(dynamicParamPath);
    }
    return mParameterHandleMap[dynamicParamPath];
}
