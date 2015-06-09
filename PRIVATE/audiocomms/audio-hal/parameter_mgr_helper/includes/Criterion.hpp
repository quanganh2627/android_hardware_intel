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

class CParameterMgrPlatformConnector;
class ISelectionCriterionInterface;
class CriterionType;

class Criterion : public audio_comms::utilities::NonCopyable
{
public:
    Criterion(const std::string &name,
              CriterionType *criterionType,
              CParameterMgrPlatformConnector *parameterMgrConnector,
              int32_t defaultValue = 0);

    Criterion(const std::string &name,
              CriterionType *criterionType,
              CParameterMgrPlatformConnector *parameterMgrConnector,
              const std::string &defaultLiteralValue);
    virtual ~Criterion();

    /**
     * Get the name of the criterion.
     *
     * @return name of the criterion.
     */
    const std::string &getName() const
    {
        return mName;
    }

    /**
     * Set the local value of the criterion.
     * Upon this call, the value is not written in the parameter manager.
     *
     * @tparam T type of the value to set. uint32_t and std::string specialization provided.
     * @param[in] value to set.
     *
     * @return true if the value was changed, false if same value requested to be set.
     */
    template <typename T>
    bool setValue(const T &value);

    /**
     * Get the local value of the criterion.
     * No call is make on the parameter manager.
     *
     * @return value of the criterion.
     */
    int getValue() const
    {
        return mValue;
    }

    /**
     * Set the local value to the parameter manager.
     */
    void setCriterionState();

    /**
     * Set the local value to the parameter manager.
     *
     * @tparam T type of the value to set.
     * @param[in] value value to set to the criterion of the parameter manager.
     *
     * @return true if the value was changed, false if same value requested to be set.
     */
    template <typename T>
    bool setCriterionState(const T &value);

    /**
     * Get the literal value of the criterion.
     *
     * @return the literal value associated to the numerical local value.
     */
    std::string getFormattedValue() const;

    /**
     * Get the criterion type handler.
     *
     * @return criterion type handle.
     */
    CriterionType *getCriterionType()
    {
        return mCriterionType;
    }

private:
    /**
     * Initialize the criterion, i.e. get the criterion interface and set the criterion
     * init value.
     *
     * @param[in] defaultValue Default numerical value of the criterion.
     */
    void init(int32_t defaultValue);

    /**
     * Helper function to retrieve the numerical value from the literal representation of the
     * criterion.
     *
     * @param[in] literalValue: literal representation of the criterion.
     *
     * @return numerical value of the criterion associated to this literal.
     */
    int getNumericalFromLiteral(const std::string &literalValue) const;

    /**
     * criterion interface for parameter manager operations.
     */
    ISelectionCriterionInterface *mSelectionCriterionInterface;

    CriterionType *mCriterionType; /**< Criterion type to which this criterion is associated. */

    CParameterMgrPlatformConnector *mParameterMgrConnector; /**< parameter manager connector. */

    std::string mName; /**< name of the criterion. */

    uint32_t mValue; /**< value of the criterion. */
};
