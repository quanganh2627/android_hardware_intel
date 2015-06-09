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

#define LOG_TAG "IntelPreProcessingFx"

#include "LpePreProcessing.hpp"
#include "AudioEffectSession.hpp"
#include "LpeAec.hpp"
#include "LpeBmf.hpp"
#include "LpeWnr.hpp"
#include "LpeNs.hpp"
#include "LpeAgc.hpp"
#include <utils/Errors.h>
#include <utilities/Log.hpp>
#include <fcntl.h>
#include <functional>
#include <algorithm>

using android::status_t;
using android::OK;
using android::BAD_VALUE;
using audio_comms::utilities::Mutex;
using audio_comms::utilities::Log;

const struct effect_interface_s LpePreProcessing::mEffectInterface = {
    NULL, /**< process. Not implemented as this lib deals with HW effects. */
    /**
     * Send a command and receive a response to/from effect engine.
     */
    &LpePreProcessing::intelLpeFxCommand,
    &LpePreProcessing::intelLpeFxGetDescriptor, /**< get the effect descriptor. */
    NULL /**< process reverse. Not implemented as this lib deals with HW effects. */
};

int LpePreProcessing::intelLpeFxCommand(effect_handle_t interface,
                                        uint32_t cmdCode,
                                        uint32_t cmdSize,
                                        void *cmdData,
                                        uint32_t *replySize,
                                        void *replyData)
{
    LpePreProcessing *self = getInstance();
    AudioEffect *effect = self->findEffectByInterface(interface);
    if (effect == NULL) {
        Log::Error() << __FUNCTION__ << ": could not find effect for requested interface";
        return BAD_VALUE;
    }

    switch (cmdCode) {
    case EFFECT_CMD_INIT:
        if (replyData == NULL || *replySize != sizeof(int)) {
            return -EINVAL;
        }
        effect->init();
        *static_cast<int *>(replyData) = 0;
        break;

    case EFFECT_CMD_SET_CONFIG:
        if (cmdData == NULL ||
            cmdSize != sizeof(effect_config_t) ||
            replyData == NULL ||
            *replySize != sizeof(int)) {
            Log::Verbose() << __FUNCTION__ << ": EFFECT_CMD_SET_CONFIG: ERROR";
            return -EINVAL;
        }
        *static_cast<int *>(replyData) = 0;
        break;

    case EFFECT_CMD_GET_CONFIG:
        if (replyData == NULL || *replySize != sizeof(effect_config_t)) {
            Log::Verbose() << __FUNCTION__ << ": EFFECT_CMD_GET_CONFIG: ERROR";
            return -EINVAL;
        }
        break;

    case EFFECT_CMD_SET_CONFIG_REVERSE:
        if (cmdData == NULL ||
            cmdSize != sizeof(effect_config_t) ||
            replyData == NULL ||
            *replySize != sizeof(int)) {
            Log::Verbose() << __FUNCTION__ << ": EFFECT_CMD_SET_CONFIG_REVERSE: ERROR";
            return -EINVAL;
        }
        *static_cast<int *>(replyData) = 0;
        break;

    case EFFECT_CMD_GET_CONFIG_REVERSE:
        if (replyData == NULL ||
            *replySize != sizeof(effect_config_t)) {
            Log::Verbose() << __FUNCTION__ << ": EFFECT_CMD_GET_CONFIG_REVERSE: ERROR";
            return -EINVAL;
        }
        break;

    case EFFECT_CMD_RESET:
        effect->reset();
        break;

    case EFFECT_CMD_GET_PARAM: {
        if (cmdData == NULL ||
            cmdSize < static_cast<int>(sizeof(effect_param_t)) ||
            replyData == NULL ||
            *replySize < static_cast<int>(sizeof(effect_param_t))) {
            Log::Error() << __FUNCTION__ << ": EFFECT_CMD_GET_PARAM: ERROR";
            return -EINVAL;
        }
        effect_param_t *p = static_cast<effect_param_t *>(cmdData);
        memcpy(replyData, cmdData, sizeof(effect_param_t) + p->psize);
        p = static_cast<effect_param_t *>(replyData);

        /**
         * The start of value field inside the data field is always on a 32 bit boundary
         * Need to take padding into consideration to point on value.
         * cf to audio_effect.h for the strucutre of the effect_params_s structure.
         */
        int voffset = ((p->psize - 1) / sizeof(int32_t) + 1) * sizeof(int32_t);

        p->status = effect->getParameter(p);
        *replySize = sizeof(effect_param_t) + voffset + p->vsize;
        break;
    }

    case EFFECT_CMD_SET_PARAM: {
        if (cmdData == NULL ||
            cmdSize < static_cast<int>(sizeof(effect_param_t)) ||
            replyData == NULL ||
            *replySize != sizeof(int32_t)) {
            Log::Verbose() << __FUNCTION__ << ": EFFECT_CMD_SET_PARAM: ERROR";
            return -EINVAL;
        }
        effect_param_t *p = static_cast<effect_param_t *>(cmdData);

        *static_cast<int *>(replyData) = effect->setParameter(p);
        break;
    }

    case EFFECT_CMD_ENABLE:
        if (replyData == NULL || *replySize != sizeof(int)) {
            Log::Verbose() << __FUNCTION__ << ": EFFECT_CMD_ENABLE: ERROR";
            return -EINVAL;
        }
        *static_cast<int *>(replyData) = 0;
        break;

    case EFFECT_CMD_DISABLE:
        if (replyData == NULL || *replySize != sizeof(int)) {
            Log::Verbose() << __FUNCTION__ << ": EFFECT_CMD_DISABLE: ERROR";
            return -EINVAL;
        }
        *static_cast<int *>(replyData) = 0;
        break;

    case EFFECT_CMD_SET_DEVICE:
    case EFFECT_CMD_SET_INPUT_DEVICE:
        if (cmdData == NULL ||
            cmdSize != sizeof(uint32_t)) {
            Log::Verbose() << __FUNCTION__ << ": EFFECT_CMD_SET_INPUT_DEVICE: ERROR";
            return -EINVAL;
        }
        effect->setDevice(*static_cast<uint32_t *>(cmdData));
        break;

    case EFFECT_CMD_SET_VOLUME:
    case EFFECT_CMD_SET_AUDIO_MODE:
    case EFFECT_CMD_SET_FEATURE_CONFIG:
        break;

    case EFFECT_CMD_SET_AUDIO_SOURCE:
        if (cmdData == NULL ||
            cmdSize != sizeof(uint32_t)) {
            Log::Verbose() << __FUNCTION__ << ": EFFECT_CMD_SET_AUDIO_SOURCE: ERROR";
            return -EINVAL;
        }
        *static_cast<int *>(replyData) = 0;
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

int LpePreProcessing::intelLpeFxGetDescriptor(effect_handle_t interface,
                                              effect_descriptor_t *descriptor)
{
    LpePreProcessing *self = getInstance();
    AudioEffect *effect = self->findEffectByInterface(interface);
    if (effect == NULL) {
        Log::Error() << __FUNCTION__ << ": could not find effect for requested interface";
        return BAD_VALUE;
    }
    memcpy(descriptor, effect->getDescriptor(), sizeof(effect_descriptor_t));
    return 0;
}

LpePreProcessing::LpePreProcessing()
{
    Log::Debug() << __FUNCTION__;
    init();
}

status_t LpePreProcessing::getEffectDescriptor(const effect_uuid_t *uuid,
                                               effect_descriptor_t *descriptor)
{
    if (descriptor == NULL || uuid == NULL) {
        Log::Error() << __FUNCTION__ << ": invalue interface and/or uuid";
        return BAD_VALUE;
    }
    const AudioEffect *effect = findEffectByUuid(uuid);
    if (effect == NULL) {
        Log::Error() << __FUNCTION__ << ": could not find effect for requested uuid";
        return BAD_VALUE;
    }
    const effect_descriptor_t *desc = effect->getDescriptor();
    if (desc == NULL) {
        Log::Error() << __FUNCTION__ << ": invalid effect descriptor";
        return BAD_VALUE;
    }
    memcpy(descriptor, desc, sizeof(effect_descriptor_t));
    return OK;
}


status_t LpePreProcessing::createEffect(const effect_uuid_t *uuid,
                                        int32_t sessionId,
                                        int32_t ioId,
                                        effect_handle_t *interface)
{
    Log::Debug() << __FUNCTION__;
    if (interface == NULL || uuid == NULL) {
        Log::Error() << __FUNCTION__ << ": invalue interface and/or uuid";
        return BAD_VALUE;
    }
    Mutex::Locker Locker(mLpeEffectsLock);
    AudioEffectSession *session = getSession(ioId);
    if (session == NULL) {
        Log::Error() << __FUNCTION__
                     << ": could not get session for sessionId=" << sessionId
                     << ", ioHandleId=" << ioId;
        return BAD_VALUE;
    }
    return session->createEffect(uuid, interface) == OK ?
           OK :
           BAD_VALUE;
}


status_t LpePreProcessing::releaseEffect(effect_handle_t interface)
{
    Log::Debug() << __FUNCTION__;
    Mutex::Locker Locker(mLpeEffectsLock);
    AudioEffect *effect = findEffectByInterface(interface);
    if (effect == NULL) {
        Log::Error() << __FUNCTION__ << ": could not find effect for requested interface";
        return BAD_VALUE;
    }
    AudioEffectSession *session = effect->getSession();
    if (session == NULL) {
        Log::Error() << __FUNCTION__ << ": no session for effect";
        return BAD_VALUE;
    }
    return session->removeEffect(effect);
}

status_t LpePreProcessing::init()
{
    for (uint32_t i = 0; i < mMaxEffectSessions; i++) {

        AudioEffectSession *effectSession = new AudioEffectSession(i);

        // Each session has an instance of effects provided by LPE
        AgcAudioEffect *agc = new AgcAudioEffect(&mEffectInterface);
        mEffectsList.push_back(agc);
        effectSession->addEffect(agc);
        NsAudioEffect *ns = new NsAudioEffect(&mEffectInterface);
        mEffectsList.push_back(ns);
        effectSession->addEffect(ns);
        AecAudioEffect *aec = new AecAudioEffect(&mEffectInterface);
        mEffectsList.push_back(aec);
        effectSession->addEffect(aec);
        BmfAudioEffect *bmf = new BmfAudioEffect(&mEffectInterface);
        mEffectsList.push_back(bmf);
        effectSession->addEffect(bmf);
        WnrAudioEffect *wnr = new WnrAudioEffect(&mEffectInterface);
        mEffectsList.push_back(wnr);
        effectSession->addEffect(wnr);

        mEffectSessionsList.push_back(effectSession);
    }
    return OK;
}

// Function to be used as the predicate in find_if call.
struct MatchUuid : public std::binary_function<AudioEffect *, const effect_uuid_t *, bool>
{

    bool operator()(const AudioEffect *effect,
                    const effect_uuid_t *uuid) const
    {

        return memcmp(effect->getUuid(), uuid, sizeof(effect_uuid_t)) == 0;
    }
};

AudioEffect *LpePreProcessing::findEffectByUuid(const effect_uuid_t *uuid)
{
    EffectListIterator it;
    it = std::find_if(mEffectsList.begin(), mEffectsList.end(), std::bind2nd(MatchUuid(), uuid));

    return (it != mEffectsList.end()) ? *it : NULL;
}

/**
 * Function to be used as the predicate in find_if call.
 */
struct MatchInterface : public std::binary_function<AudioEffect *, const effect_handle_t, bool>
{

    bool operator()(AudioEffect *effect,
                    const effect_handle_t interface) const
    {

        return effect->getHandle() == interface;
    }
};

AudioEffect *LpePreProcessing::findEffectByInterface(const effect_handle_t interface)
{
    EffectListIterator it;
    it = std::find_if(mEffectsList.begin(), mEffectsList.end(),
                      std::bind2nd(MatchInterface(), interface));

    return (it != mEffectsList.end()) ? *it : NULL;
}

/**
 * Function to be used as the predicate in find_if call.
 */
struct MatchSession : public std::binary_function<AudioEffectSession *, int, bool>
{

    bool operator()(const AudioEffectSession *effectSession,
                    int ioId) const
    {
        return effectSession->getIoHandle() == ioId;
    }
};

AudioEffectSession *LpePreProcessing::findSession(int ioId)
{
    EffectSessionListIterator it;
    it = std::find_if(mEffectSessionsList.begin(), mEffectSessionsList.end(),
                      std::bind2nd(MatchSession(), ioId));

    return (it != mEffectSessionsList.end()) ? *it : NULL;
}

AudioEffectSession *LpePreProcessing::getSession(uint32_t ioId)
{
    AudioEffectSession *session = findSession(ioId);
    if (session == NULL) {
        Log::Debug() << __FUNCTION__
                     << ": no session assigned for io=" << ioId
                     << ", try to get one...";
        session = findSession(AudioEffectSession::mSessionNone);
        if (session != NULL) {

            session->setIoHandle(ioId);
        }
    }
    return session;
}

LpePreProcessing *LpePreProcessing::getInstance()
{
    static LpePreProcessing instance;
    return &instance;
}
