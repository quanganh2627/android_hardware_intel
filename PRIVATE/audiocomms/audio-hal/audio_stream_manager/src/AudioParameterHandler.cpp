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
#define LOG_TAG "StreamManager/ParameterHandler"

#include "AudioParameterHandler.hpp"
#include <utilities/Log.hpp>
#include <utils/Errors.h>

using android::status_t;
using namespace std;
using audio_comms::utilities::Log;

namespace intel_audio
{

const char AudioParameterHandler::mFilePath[] = "/mnt/asec/media/audio_param.dat";
const int AudioParameterHandler::mReadBufSize = 500;

AudioParameterHandler::AudioParameterHandler()
{
    restore();
}

status_t AudioParameterHandler::saveParameters(const string &keyValuePairs)
{
    add(keyValuePairs);
    return save();
}

void AudioParameterHandler::add(const string &keyValuePairs)
{
    mPairs.add(keyValuePairs);
}

status_t AudioParameterHandler::save()
{
    FILE *fp = fopen(mFilePath, "w+");
    if (!fp) {
        Log::Error() << __FUNCTION__ << ": error " << strerror(errno);
        return android::UNKNOWN_ERROR;
    }

    string param = mPairs.toString();

    size_t ret = fwrite(param.c_str(), sizeof(char), param.length(), fp);
    fclose(fp);

    return ret != param.length() ? android::UNKNOWN_ERROR : android::OK;
}

status_t AudioParameterHandler::restore()
{
    FILE *fp = fopen(mFilePath, "r");
    if (!fp) {
        Log::Error() << __FUNCTION__ << ": error " << strerror(errno);
        return android::UNKNOWN_ERROR;
    }
    char str[mReadBufSize];
    size_t readSize = fread(str, sizeof(char), mReadBufSize - 1, fp);
    fclose(fp);
    if (readSize == 0) {

        return android::UNKNOWN_ERROR;
    }
    str[readSize] = '\0';

    add(str);
    return android::OK;
}

string AudioParameterHandler::getParameters() const
{
    return mPairs.toString();
}
}
