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
#define LOG_TAG "KeyValuePairs"

#include "KeyValuePairs.hpp"
#include <AudioCommsAssert.hpp>
#include <string.h>
#include <stdlib.h>

using namespace audio_comms::utilities;
using namespace std;

namespace intel_audio
{

const char *const KeyValuePairs::mPairDelimiter = ";";
const char *const KeyValuePairs::mPairAssociator = "=";

KeyValuePairs::KeyValuePairs(const string &pairs)
{
    add(pairs);
}

KeyValuePairs::~KeyValuePairs()
{
    mMap.clear();
}

string KeyValuePairs::toString()
{
    string keyValueList;
    MapConstIterator it;
    for (it = mMap.begin(); it != mMap.end(); ++it) {
        keyValueList += it->first + mPairAssociator + it->second;
        if (it->first != mMap.rbegin()->first) {
            keyValueList += mPairDelimiter;
        }
    }
    return keyValueList;
}

android::status_t KeyValuePairs::remove(const string &key)
{
    if (mMap.find(key) == mMap.end()) {
        return android::BAD_VALUE;
    }
    mMap.erase(key);
    return android::OK;
}

android::status_t KeyValuePairs::add(const string &keyValuePairs)
{
    android::status_t status = android::OK;
    char *pairs = strdup(keyValuePairs.c_str());
    char *context;
    char *pair = strtok_r(pairs, mPairDelimiter, &context);
    while (pair != NULL) {
        if (strlen(pair) != 0) {
            string key;
            string value;
            // An audio parameter can be constructed with key;key or key=value;key=value
            if (strchr(pair, *mPairAssociator) != NULL) {
                if (strcspn(pair, mPairAssociator) == 0) {
                    // No key provided, bailing out
                    free(pairs);
                    return android::BAD_VALUE;
                }
                char *tmp = strtok(pair, mPairAssociator);
                AUDIOCOMMS_ASSERT(tmp != NULL, "No key nor value provided");
                key = tmp;
                tmp = strtok(NULL, mPairAssociator);
                if (tmp != NULL) {
                    value = tmp;
                }
            } else {
                key = pair;
            }
            android::status_t res = addLiteral(key, value);
            if (res != android::OK) {
                status = res;
            }
        }
        pair = strtok_r(NULL, mPairDelimiter, &context);
    }
    free(pairs);
    return status;
}

android::status_t KeyValuePairs::addLiteral(const string &key, const string &value)
{
    if (mMap.find(key) != mMap.end()) {
        mMap[key] = value;
        return android::ALREADY_EXISTS;
    }
    mMap[key] = value;
    return android::OK;
}

android::status_t KeyValuePairs::getLiteral(const string &key, string &value) const
{
    MapConstIterator it = mMap.find(key);
    if (it == mMap.end()) {
        return android::BAD_VALUE;
    }
    value = it->second;
    return android::OK;
}

}   // namespace intel_audio
