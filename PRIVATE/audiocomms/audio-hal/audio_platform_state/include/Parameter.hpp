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

#include "ParameterChangedObserver.hpp"
#include <NonCopyable.hpp>
#include <map>
#include <string>

namespace intel_audio
{

/**
 * This class materialize a wrapping object between an android-parameter refered by its key
 * and a Parameter Manager element (criterion or rogue) refered by its name (for a criterion)
 * or path (for a rogue parameter). This object will allow to wrap setter and getter on this
 * android-parameter to the associated element in the Parameter Manager.
 */
class Parameter : private audio_comms::utilities::NonCopyable
{
public:
    typedef std::map<std::string, std::string>::const_iterator MappingValuesMapConstIterator;

    Parameter(ParameterChangedObserver *observer,
              const std::string &key,
              const std::string &name,
              const std::string &defaultValue)
        : mObserver(observer),
          mDefaultLiteralValue(defaultValue),
          mAndroidParameterKey(key),
          mAndroidParameter(name)
    {
    }

    virtual ~Parameter() {}

    /**
     * Get the key of the android parameter associated to the PFW parameter.
     *
     * @return android-parameter key.
     */
    const std::string &getKey() const { return mAndroidParameterKey; }

    /**
     * Returns the name of the Android Parameter.
     *
     * @return name of the android parameter.
     */
    const std::string &getName() const { return mAndroidParameter; }

    /**
     * Returns the default literal value of the parameter (in the PFW domain, not in android).
     *
     * @return default literal value of the PFW parameter
     */
    const std::string &getDefaultLiteralValue() const { return mDefaultLiteralValue; }

    /**
     * Adds a mapping value pair, i.e. a pair or Android Value, PFW Parameter Value.
     *
     * @param[in] name android parameter value.
     * @param[in] value PFW parameter value.
     */
    void setMappingValuePair(const std::string &name, const std::string &value);

    /**
     * Sets a value to the parameter.
     *
     * @param[in] value to set (received from the keyValue pair).
     *
     * @return true if set is successful, false otherwise.
     */
    virtual bool setValue(const std::string &value) = 0;

    /**
     * Gets the value from the Parameter. The value returned must be in the domain
     * of the android parameter.
     *
     * @param[out] value to return (associated to the key of the android parameter).
     *
     * @return true if get is successful, false otherwise.
     */
    virtual bool getValue(std::string &value) const = 0;

    /**
     * Synchronise the pfw Parameter associated to an android-parameter.
     *
     * @return true if sync is successful, false otherwise.
     */
    virtual bool sync() = 0;

protected:
    /**
     * Checks the validity of an android parameter value.
     *
     * @param[in] value android parameter value to check upon validity.
     *
     * @return true if value found in the mapping table, false otherwise.
     */
    bool isAndroidParameterValueValid(const std::string value) const
    {
        return mMappingValuesMap.find(value) != mMappingValuesMap.end();
    }

    /**
     * Checks the validity of an android parameter value.
     *
     * @param[in] androidParam android parameter to wrap.
     * @param[out] literalValue associated to the androidParam value. Set only if return is true.
     *
     * @return true if value found in the mapping table, false otherwise.
     */
    bool getLiteralValueFromParam(const std::string &androidParam, std::string &literalValue) const;

    /**
     * Checks the validity of a rogue parameter value.
     *
     * @param[out] androidParam android-parameter associated to the. Set only if return is true.
     * @param[in] literalValue to wrap.
     *
     * @return true if value found in the mapping table, false otherwise.
     */
    bool getParamFromLiteralValue(std::string &androidParam, const std::string &literalValue) const;

    /**
     * associate android parameter values to PFW element values (it may be criterion literal values
     * or rogue parameter literal values).
     * This Map may be empty, in this case, any value of the Android Parameter will be applied as it
     * to the parameter / criterion without any validity check.
     */
    std::map<std::string, std::string> mMappingValuesMap;

    /**
     * Observer handle to notify any change on this parameter.
     */
    ParameterChangedObserver *mObserver;

private:
    /**
     * Default literal value.
     * The default is in the parameter domain.
     */
    const std::string mDefaultLiteralValue;

    const std::string mAndroidParameterKey; /**< key name of the parameter. */

    const std::string mAndroidParameter; /**< Name of the parameter. */
};

} // namespace intel_audio
