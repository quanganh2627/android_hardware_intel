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
#include "IoStream.hpp"
#include "AudioDevice.hpp"
#include <IStreamRoute.hpp>
#include <AudioCommsAssert.hpp>
#include <SampleSpec.hpp>
#include <utils/RWLock.h>

using std::string;

namespace intel_audio
{

bool IoStream::isRouted() const
{
    AutoR lock(mStreamLock);
    return isRoutedL();
}

bool IoStream::isRoutedL() const
{
    return mCurrentStreamRoute != NULL;
}

bool IoStream::isNewRouteAvailable() const
{
    AutoR lock(mStreamLock);
    return mNewStreamRoute != NULL;
}

android::status_t IoStream::attachRoute()
{
    AutoW lock(mStreamLock);
    return attachRouteL();
}


android::status_t IoStream::detachRoute()
{
    AutoW lock(mStreamLock);
    return detachRouteL();
}

android::status_t IoStream::attachRouteL()
{
    AUDIOCOMMS_ASSERT(mNewStreamRoute != NULL, "NULL route pointer");
    setCurrentStreamRouteL(mNewStreamRoute);
    AUDIOCOMMS_ASSERT(mCurrentStreamRoute != NULL, "NULL route pointer");
    setRouteSampleSpecL(mCurrentStreamRoute->getSampleSpec());
    return android::OK;
}


android::status_t IoStream::detachRouteL()
{
    mCurrentStreamRoute = NULL;
    return android::OK;
}

void IoStream::addRequestedEffect(uint32_t effectId)
{
    mEffectsRequestedMask |= effectId;
}

void IoStream::removeRequestedEffect(uint32_t effectId)
{
    mEffectsRequestedMask &= ~effectId;
}

uint32_t IoStream::getOutputSilencePrologMs() const
{
    AUDIOCOMMS_ASSERT(mCurrentStreamRoute != NULL, "NULL route pointer");
    return mCurrentStreamRoute->getOutputSilencePrologMs();
}

void IoStream::resetNewStreamRoute()
{
    mNewStreamRoute = NULL;
}

void IoStream::setNewStreamRoute(IStreamRoute *newStreamRoute)
{
    mNewStreamRoute = newStreamRoute;
}

void IoStream::setCurrentStreamRouteL(IStreamRoute *currentStreamRoute)
{
    mCurrentStreamRoute = currentStreamRoute;
}

void IoStream::setRouteSampleSpecL(SampleSpec sampleSpec)
{
    mRouteSampleSpec = sampleSpec;
}

} // namespace intel_audio
