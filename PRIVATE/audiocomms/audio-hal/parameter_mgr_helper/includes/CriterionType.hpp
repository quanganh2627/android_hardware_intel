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

#include <AudioCommsAssert.hpp>
#include <NonCopyable.hpp>
#include <stdint.h>
#include <string>

class CParameterMgrPlatformConnector;
class ISelectionCriterionTypeInterface;

class CriterionType : public audio_comms::utilities::NonCopyable
{
public:
    typedef std::pair<int, const char *> ValuePair;

    CriterionType(const std::string &name,
                  bool isInclusive,
                  CParameterMgrPlatformConnector *parameterMgrConnector);
    virtual ~CriterionType();

    /**
     * Get the name of the criterion type.
     *
     * @return name of the criterion type.
     */
    const std::string &getName() const { return mName; }

    /**
     * Adds a value pair for this criterion type to the parameter manager.
     *
     * @param[in] numerical part of the value pair.
     * @param[in] literal part of the value pair.
     */
    void addValuePair(int numerical, const std::string &literal);

    /**
     * Adds value pairs for this criterion type to the parameter manager.
     *
     * @param[in] pairs array of value pairs.
     * @param[in] nbPairs number of value pairs to add.
     */
    void addValuePairs(const ValuePair *pairs, uint32_t nbPairs);

    /**
     * Checks if a literal value belongs to the criterion type value pairs.
     *
     * @param[in] name to check if belongs to value pairs.
     *
     * @return true if literal is belonging to the value pairs of the criterion type,
     *              false otherwise.
     */
    bool hasValuePairByName(const std::string &name);

    /**
     * Get the criterion type interface.
     *
     * @return criterion type interface.
     */
    ISelectionCriterionTypeInterface *getTypeInterface()
    {
        AUDIOCOMMS_ASSERT(mCriterionTypeInterface != NULL, "Invalid Interface");
        return mCriterionTypeInterface;
    }

    /**
     * Get the literal value of the criterion.
     *
     * @return the literal value associated to the numerical local value.
     */
    std::string getFormattedState(uint32_t value);

private:
    /**
     * criterion type interface for parameter manager operations.
     */
    ISelectionCriterionTypeInterface *mCriterionTypeInterface;

    std::string mName; /**< criterion type name. */
    bool mIsInclusive; /**< inclusive attribute. */
    CParameterMgrPlatformConnector *mParameterMgrConnector; /**< parameter manager connector. */
};
