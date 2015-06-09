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

#define LOG_TAG "IntelPreProcessingFx/EffectSession"

#include "AudioEffectSession.hpp"
#include "LpePreProcessing.hpp"
#include <audio_effects/effect_aec.h>
#include <audio_effects/effect_ns.h>
#include <audio_effects/effect_agc.h>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <functional>
#include <algorithm>

using android::status_t;
using android::OK;
using android::BAD_VALUE;
using audio_comms::utilities::Log;

AudioEffectSession::AudioEffectSession(uint32_t sessionId)
    : mId(sessionId)
{
    init();
}

void AudioEffectSession::init()
{
    mIoHandle = mSessionNone;
    mSource = AUDIO_SOURCE_DEFAULT;
}

void AudioEffectSession::setIoHandle(int ioHandle)
{
    Log::Debug() << __FUNCTION__ << ": setting io=" << ioHandle << " for session=" << mId;
    mIoHandle = ioHandle;
}

status_t AudioEffectSession::addEffect(AudioEffect *effect)
{
    AUDIOCOMMS_ASSERT(effect != NULL, "trying to add null Effect");
    effect->setSession(this);
    mEffectsList.push_back(effect);
    return OK;
}

AudioEffect *AudioEffectSession::findEffectByUuid(const effect_uuid_t *uuid)
{
    AUDIOCOMMS_ASSERT(uuid != NULL, "Invalid UUID");
    EffectListIterator it;
    it = std::find_if(mEffectsList.begin(), mEffectsList.end(), std::bind2nd(MatchUuid(), uuid));

    return (it != mEffectsList.end()) ? *it : NULL;
}

status_t AudioEffectSession::createEffect(const effect_uuid_t *uuid, effect_handle_t *interface)
{
    AUDIOCOMMS_ASSERT(uuid != NULL, "Invalid UUID");
    AudioEffect *effect = findEffectByUuid(uuid);
    if (effect == NULL) {
        Log::Error() << __FUNCTION__ << ": could not find effect for requested uuid";
        return BAD_VALUE;
    }
    Log::Debug() << __FUNCTION__ << ": requesting to create effect "
                 << effect->getDescriptor()->name << " on session=" << mId;
    // Set the interface handle
    *interface = effect->getHandle();

    mEffectsCreatedList.push_back(effect);

    return OK;
}

status_t AudioEffectSession::removeEffect(AudioEffect *effect)
{
    AUDIOCOMMS_ASSERT(effect != NULL, "trying to remove null Effect");
    Log::Debug() << __FUNCTION__ << ": requesting to remove effect "
                 << effect->getDescriptor()->name << " on session=" << mId;
    mEffectsCreatedList.remove(effect);

    // Reset the session if no more effects are created on it.
    if (mEffectsCreatedList.empty()) {
        Log::Debug() << __FUNCTION__ << ": no more effect within session=" << mId
                     << ", reinitialising session";
        init();
    }

    return OK;
}
