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

#include <hardware/audio_effect.h>
#include <NonCopyable.hpp>
#include <utils/String8.h>
#include <string>

class AudioEffectSession;

class AudioEffect : private audio_comms::utilities::NonCopyable
{
public:
    AudioEffect(const struct effect_interface_s *itfe, const effect_descriptor_t *descriptor);

    virtual ~AudioEffect();

    /**
     * Create the effect.
     *
     * @return 0 if success, error code otherwise.
     */
    virtual int create();

    /**
     * Initialize the effect.
     *
     * @return 0 if success, error code otherwise.
     */
    virtual int init();

    /**
     * Reset the effect.
     *
     * @return 0 if success, error code otherwise.
     */
    virtual int reset();

    /**
     * Enable the effect.
     * Called by the framework before the first call to process.
     */
    virtual void enable();

    /**
     * Disable the effect.
     * Called by the framework after the last call to process.
     */
    virtual void disable();

    /**
     * Set Parameter to the effect.
     *
     * @param[in] param parameter effect structure
     *
     * @return 0 if success, error code otherwise.
     */
    virtual int setParameter(const effect_param_t *param);

    /**
     * Get the effect parameter.
     *
     * @param[in,out] param parameter effect structure
     *
     * @return 0 if success, error code otherwise.
     */
    virtual int getParameter(effect_param_t *param) const;

    /**
     * Set the effect rendering device.
     *
     * @param[in] device mask of the devices.
     *
     * @return 0 if success, error code otherwise.
     */
    virtual int setDevice(uint32_t device);

    /**
     * Get the effect descriptor.
     *
     * @return effect descriptor structure.
     */
    const effect_descriptor_t *getDescriptor() const { return mDescriptor; }

    /**
     * Get the effect UUID.
     *
     * @return Unique effect ID.
     */
    const effect_uuid_t *getUuid() const;

    /**
     * Set a Session to attach to this effect.
     *
     * @param[in] session to attach.
     */
    void setSession(AudioEffectSession *session);

    /**
     * Get Session attached to this effect.
     *
     * @return session.
     */
    AudioEffectSession *getSession() const { return mSession; }

    /**
     * Get effect Handle.
     * Note that by design choice of Android, effect interface address is considered
     * as the handle of the effect.
     *
     * @return address the of the effect interface.
     */
    effect_handle_t getHandle() { return (effect_handle_t)(&mItfe); }

private:
    /**
     * Extract from the effect_param_t structure the parameter Id.
     * It supports only single parameter. If more than one paramId is found in the structure,
     * it will return an error code.
     *
     * @param[in] param: Effect Parameter structure
     * @param[out] paramId: retrieve param Id, valid only if return code is 0.
     *
     * @return 0 if the paramId could be extracted, error code otherwise.
     */
    int getParamId(const effect_param_t *param, int32_t &paramId) const;

    /**
     * Format the Parameter key from the effect parameter structure.
     * The followed formalism is:
     *      <human readable type name>-<paramId>[-<subParamId1>-<subParamId2>-...]
     *
     * @param[in] param: Effect Parameter structure
     *
     * @return valid AudioParameter key if success, empty key if failure.
     */
    int formatParamKey(const effect_param_t *param, android::String8 &key) const;

    /**
     * Effect Descriptor structure.
     * The effect descriptor contains necessary information to facilitate the enumeration of the
     * effect.
     */
    const effect_descriptor_t *mDescriptor;
    const struct effect_interface_s *mItfe; /**< Effect control interface structure. */
    uint32_t mPreProcessorId; /**< type of preprocessor. */
    uint32_t mState; /**< state of the effect. */
    AudioEffectSession *mSession; /**< Session on which the effect is on. */
    static const std::string mParamKeyDelimiter; /**< Delimiter chosen to format the key. */
};
