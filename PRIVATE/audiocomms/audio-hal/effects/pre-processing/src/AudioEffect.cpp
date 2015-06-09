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

#define LOG_TAG "IntelPreProcessingFx/Effect"

#include "AudioEffect.hpp"
#include "LpePreProcessing.hpp"
#include "AudioEffectSession.hpp"
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <convert.hpp>
#include <media/AudioSystem.h>
#include <media/AudioParameter.h>

using android::status_t;
using android::AudioParameter;
using android::String8;
using android::AudioSystem;
using android::NO_ERROR;
using audio_comms::utilities::convertTo;
using audio_comms::utilities::Log;

const std::string AudioEffect::mParamKeyDelimiter = "-";

AudioEffect::AudioEffect(const effect_interface_s *itfe,
                         const effect_descriptor_t *descriptor)
    : mDescriptor(descriptor),
      mItfe(itfe),
      mSession(NULL)
{
}

AudioEffect::~AudioEffect()
{
}

void AudioEffect::setSession(AudioEffectSession *session)
{
    AUDIOCOMMS_ASSERT(mSession == NULL, "Effects already in use for a session");
    mSession = session;
}

const effect_uuid_t *AudioEffect::getUuid() const
{
    AUDIOCOMMS_ASSERT(mDescriptor != NULL, "Effects descriptor invalid");
    return &mDescriptor->uuid;
}

int AudioEffect::create()
{
    Log::Verbose() << __FUNCTION__ << ": NOP";
    return 0;
}

int AudioEffect::init()
{
    Log::Verbose() << __FUNCTION__ << ": NOP";
    return 0;
}

int AudioEffect::reset()
{
    Log::Verbose() << __FUNCTION__ << ": NOP";
    return 0;
}

void AudioEffect::enable()
{
    Log::Verbose() << __FUNCTION__ << ": NOP";
}

void AudioEffect::disable()
{
    Log::Verbose() << __FUNCTION__ << ": NOP";
}

int AudioEffect::getParamId(const effect_param_t *param, int32_t &paramId) const
{
    // Retrieve the parameter(s) - Only supports until now a single paramId
    int32_t *pParamTemp = (int32_t *)param->data;
    if (param->psize != sizeof(int32_t)) {
        Log::Verbose() << __FUNCTION__ << ": effect = " << getDescriptor()->name
                       << ", only single paramId supported";
        // @todo: Manage sub-parameters appending it to the key with delimiter.
        return -EINVAL;
    }
    paramId = *pParamTemp;
    return 0;
}

int AudioEffect::formatParamKey(const effect_param_t *param, android::String8 &key) const
{
    /**
     * Retrieve the parameter(s) - Only supports a single paramId at the moment.
     * @todo: supports of multiple paramId.
     */
    int32_t paramId = 0;
    if (getParamId(param, paramId)) {
        return -EINVAL;
    }

    /**
     * Format the key. key is made of <Name of the effect>-<paramId>
     */
    std::ostringstream ostr;
    ostr << paramId;
    key = String8(std::string(getDescriptor()->name + mParamKeyDelimiter + ostr.str()).c_str());
    return 0;
}

int AudioEffect::setParameter(const effect_param_t *param)
{
    // Format a key from the effect param structure.
    String8 key;
    int ret = formatParamKey(param, key);
    if (ret) {
        return ret;
    }
    Log::Verbose() << __FUNCTION__
                   << ": effect " << getDescriptor()->name << " key " << key.string();

    /**
     * Retrieve the value(s) - Only supports a single value at the moment.
     * @todo: supports of multiple values.
     */
    uint32_t paramSizeWithPadding =
        ((param->psize - 1) / sizeof(uint32_t) + 1) * sizeof(uint32_t);

    const int32_t *pValue =  reinterpret_cast<const int32_t *>(param->data + paramSizeWithPadding);

    int32_t value;
    if (param->vsize == sizeof(int16_t)) {
        value = *reinterpret_cast<const int16_t *>(pValue);
    } else if (param->vsize == sizeof(int32_t)) {
        value = *reinterpret_cast<const int32_t *>(pValue);
    } else {
        Log::Verbose() << __FUNCTION__
                       << ": effect" << getDescriptor()->name << ", only single value supported";
        // @todo: Manage array of value by formatting a string with [<value[0]>,<value[1]>, ...].
        return -EINVAL;
    }

    AudioParameter audioParam;
    audioParam.addInt(key, value);

    if (AudioSystem::setParameters(0, audioParam.toString()) != NO_ERROR) {
        return -EINVAL;
    }
    return 0;
}

int AudioEffect::getParameter(effect_param_t *param) const
{
    // Format a key from the effect param structure.
    String8 key;
    int ret = formatParamKey(param, key);
    if (ret) {
        return ret;
    }
    Log::Verbose() << __FUNCTION__
                   << ":  effect " << getDescriptor()->name << " key " << key.string();

    String8 keyValuePair = AudioSystem::getParameters(0, key);

    AudioParameter audioParam(keyValuePair);

    String8 strValue;
    if (audioParam.get(key, strValue) != NO_ERROR) {
        Log::Error() << __FUNCTION__
                     << ": effect " << getDescriptor()->name << ", could not get the read value";
        return -EINVAL;
    }
    int32_t *pValue = reinterpret_cast<int32_t *>(param->data + param->psize);
    if (param->vsize == sizeof(int16_t)) {
        if (!convertTo(std::string(strValue.string()),
                       *reinterpret_cast<int16_t *>(pValue))) {
            Log::Error() << __FUNCTION__
                         << ": effect " << getDescriptor()->name
                         << ", could not get the read value as int16_t";
            return -EINVAL;
        }
    } else if (param->vsize == sizeof(int32_t)) {
        if (!convertTo(std::string(strValue.string()), *pValue)) {
            Log::Error() << __FUNCTION__
                         << ": effect " << getDescriptor()->name
                         << ", could not get the read value as int32_t";
            return -EINVAL;
        }
    } else {
        Log::Error() << __FUNCTION__
                     << ": effect = " << getDescriptor()->name << ", only single value supported";
        // @todo: Manage array of value by formatting a string with [<value[0]>,<value[1]>, ...].
        return -EINVAL;
    }
    return 0;
}

int AudioEffect::setDevice(uint32_t device)
{
    Log::Verbose() << __FUNCTION__ << ": NOP";
    return 0;
}
