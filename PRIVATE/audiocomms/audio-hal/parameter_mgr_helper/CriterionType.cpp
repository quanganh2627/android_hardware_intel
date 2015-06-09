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
#include "CriterionType.hpp"
#include "ParameterMgrPlatformConnector.h"
#include <AudioCommsAssert.hpp>

using std::string;

CriterionType::CriterionType(const string &name,
                             bool isInclusive,
                             CParameterMgrPlatformConnector *parameterMgrConnector)
    : mName(name),
      mIsInclusive(isInclusive),
      mParameterMgrConnector(parameterMgrConnector)
{
    mCriterionTypeInterface = mParameterMgrConnector->createSelectionCriterionType(mIsInclusive);
}

CriterionType::~CriterionType()
{
}

string CriterionType::getFormattedState(uint32_t value)
{
    return getTypeInterface()->getFormattedState(value);
}

void CriterionType::addValuePair(int numerical, const string &literal)
{
    getTypeInterface()->addValuePair(numerical, literal.c_str());
}

void CriterionType::addValuePairs(const ValuePair *pairs, uint32_t nbPairs)
{
    AUDIOCOMMS_ASSERT(pairs != NULL, "NULL value pairs");
    uint32_t index;
    for (index = 0; index < nbPairs; index++) {

        const ValuePair *valuePair = &pairs[index];
        getTypeInterface()->addValuePair(valuePair->first, valuePair->second);
    }
}

bool CriterionType::hasValuePairByName(const std::string &name)
{
    int value = 0;
    return getTypeInterface()->getNumericalValue(name, value);
}
