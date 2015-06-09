/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (c) 2014 Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors.
 *
 * Title to the Material remains with Intel Corporation or its suppliers and
 * licensors. The Material contains trade secrets and proprietary and
 * confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and treaty
 * provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or disclosed
 * in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 */
#pragma once

#include <convert.hpp>
#include <string>
#include <map>
#include <utils/Errors.h>

namespace intel_audio
{

/**
 * Helper class to parse / retrieve a semi-colon separated string of {key, value} pairs.
 */
class KeyValuePairs
{
private:
    typedef std::map<std::string, std::string> Map;
    typedef Map::iterator MapIterator;
    typedef Map::const_iterator MapConstIterator;

public:
    KeyValuePairs() {}
    KeyValuePairs(const std::string &keyValuePairs);
    virtual ~KeyValuePairs();

    /**
     * Convert the AudioParameter into a semi-colon separated string of {key, value} pairs.
     *
     * @return semi-colon separated string of {key, value} pairs
     */
    std::string toString();

    /**
     * Add all pairs contained in a semi-colon separated string of {key, value} to the collection.
     * The collection may contains keys and/or keys-value pairs.
     * If the key is found, it will just update the value.
     * If the key is not found, it will add the new key and its associated value.
     *
     * @param[in] keyValuePairs semi-colon separated string of {key, value} pairs.
     *
     * @return OK if all key / value pairs were added correctly.
     * @return error code otherwise, at least one pair addition failed.
     */
    android::status_t add(const std::string &keyValuePairs);

    /**
     * Add a new value pair to the collection.
     *
     * @tparam T type of the value to add.
     * @param[in] key to add.
     * @param[in] value to add.
     *
     * @return OK if the key was added with its corresponding value.
     * @return ALREADY_EXISTS if the key was already added, the value is updated however.
     * @return error code otherwise.
     */
    template <typename T>
    android::status_t add(const std::string &key, const T &value)
    {
        std::string literal;
        if (!audio_comms::utilities::convertTo(value, literal)) {
            return android::BAD_VALUE;
        }
        return addLiteral(key, literal);
    }

    /**
     * Remove a value pair from the collection.
     *
     * @param[in] key to remove.
     *
     * @return OK if the key was removed successfuly.
     * @return BAD_VALUE if the key was not found.
     */
    android::status_t remove(const std::string &key);

    /**
     * Get a value from a given key from the collection.
     *
     * @tparam T type of the value to get.
     * @param[in] key associated to the value to get.
     * @param[out] value to get.
     *
     * @return OK if the key was found and the value is returned into value parameter.
     * @return error code otherwise.
     */
    template <typename T>
    android::status_t get(const std::string &key, T &value) const
    {
        std::string literalValue;
        android::status_t status = getLiteral(key, literalValue);
        if (status != android::OK) {
            return status;
        }
        if (!audio_comms::utilities::convertTo(literalValue, value)) {
            return android::BAD_VALUE;
        }
        return android::OK;
    }

    /**
     * @return the number of {key, value} pairs found in the collection.
     */
    size_t size()
    {
        return mMap.size();
    }

private:
    /**
     * Add a new value pair to the collection.
     *
     * @param[in] key to add.
     * @param[in] value to add (as literal).
     *
     * @return OK if the key was added with its corresponding value.
     * @return error code otherwise.
     */
    android::status_t addLiteral(const std::string &key, const std::string &value);

    /**
     * Get a literal value from a given key from the collection.
     *
     * @param[in] key associated to the value to get.
     * @param[out] value to get.
     *
     * @return OK if the key was found and the value is returned into value parameter.
     * @return error code otherwise.
     */
    android::status_t getLiteral(const std::string &key, std::string &value) const;

    std::map<std::string, std::string> mMap; /**< value pair collection Map indexed by the key. */

    static const char *const mPairDelimiter; /**< Delimiter between {key, value} pairs. */
    static const char *const mPairAssociator; /**< key value Pair token. */
};

}   // namespace intel_audio
