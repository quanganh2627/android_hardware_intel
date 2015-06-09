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
#include "Criterion.hpp"
#include "CriterionType.hpp"
#include "ParameterMgrPlatformConnector.h"
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using std::string;
using audio_comms::utilities::Log;

Criterion::Criterion(const string &name,
                     CriterionType *criterionType,
                     CParameterMgrPlatformConnector *parameterMgrConnector,
                     int32_t defaultValue)
    : mCriterionType(criterionType),
      mParameterMgrConnector(parameterMgrConnector),
      mName(name)
{
    init(defaultValue);
}

Criterion::Criterion(const string &name,
                     CriterionType *criterionType,
                     CParameterMgrPlatformConnector *parameterMgrConnector,
                     const std::string &defaultLiteralValue)
    : mCriterionType(criterionType),
      mParameterMgrConnector(parameterMgrConnector),
      mName(name)
{
    init(getNumericalFromLiteral(defaultLiteralValue));
}

void Criterion::init(int32_t defaultValue)
{
    AUDIOCOMMS_ASSERT(mCriterionType != NULL, "NULL criterion Type");
    mSelectionCriterionInterface =
        mParameterMgrConnector->createSelectionCriterion(mName,
                                                         mCriterionType->getTypeInterface());
    mValue = defaultValue;
    setCriterionState();
}

Criterion::~Criterion()
{
}

string Criterion::getFormattedValue() const
{
    return mCriterionType->getTypeInterface()->getFormattedState(mValue);
}

template <>
bool Criterion::setValue<uint32_t>(const uint32_t &value)
{
    if (mValue != value) {

        mValue = value;
        return true;
    }
    return false;
}

template <>
bool Criterion::setValue<std::string>(const std::string &literalValue)
{
    return setValue<uint32_t>(getNumericalFromLiteral(literalValue));
}

void Criterion::setCriterionState()
{
    AUDIOCOMMS_ASSERT(mSelectionCriterionInterface != NULL, "NULL criterion interface");
    mSelectionCriterionInterface->setCriterionState(mValue);
}

template <>
bool Criterion::setCriterionState<int32_t>(const int32_t &value)
{
    if (setValue<uint32_t>(value)) {

        AUDIOCOMMS_ASSERT(mSelectionCriterionInterface != NULL, "NULL criterion interface");
        mSelectionCriterionInterface->setCriterionState(mValue);
        return true;
    }
    return false;
}

template <>
bool Criterion::setCriterionState<string>(const string &value)
{
    return setCriterionState<int32_t>(getNumericalFromLiteral(value));
}

int Criterion::getNumericalFromLiteral(const std::string &literalValue) const
{
    AUDIOCOMMS_ASSERT(mCriterionType != NULL, "NULL criterion interface");
    int numericalValue = 0;
    if (!literalValue.empty() &&
        !mCriterionType->getTypeInterface()->getNumericalValue(literalValue,
                                                               numericalValue)) {
        Log::Error() << __FUNCTION__
                     << ": could not retrieve numerical value " << literalValue
                     << " for criterion " << mName;
    }
    return numericalValue;
}
