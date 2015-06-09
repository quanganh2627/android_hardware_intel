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

#include "LpePreProcessing.hpp"

extern "C" {

/**
 * Create an effect
 * Creates an effect engine of the specified implementation uuid and
 * returns an effect control interface on this engine. The function will allocate the
 * resources for an instance of the requested effect engine and return
 * a handle on the effect control interface.
 *
 * @param[in] uuid  pointer to the effect uuid.
 * @param[in] sessionId audio session to which this effect instance will be attached. All effects
 *              created with the same session ID are connected in series and process the same signal
 *              stream. Knowing that two effects are part of the same effect chain can help the
 *              library implement some kind of optimizations.
 * @param[in] ioId identifies the output or input stream this effect is directed to at audio HAL.
 *              For future use especially with tunneled HW accelerated effects
 * @param[out] interface effect interface handle.
 *
 * @return 0 if success, effect interface handle is valid,
 *         -ENODEV is library failed to initialize
 *         -ENOENT     no effect with this uuid found
 *         -EINVAL if invalid interface handle
 */
int lpeCreate(const effect_uuid_t *uuid,
              int32_t sessionId,
              int32_t ioId,
              effect_handle_t *interface)
{
    LpePreProcessing *lpeProcessing = LpePreProcessing::getInstance();
    return lpeProcessing->createEffect(uuid, sessionId, ioId, interface);
}

/**
 * Release an effect
 * Releases the effect engine whose handle is given as argument.
 * All resources allocated to this particular instance of the effect are released.
 *
 * @param[in] interface handle on the effect interface to be released
 *
 * @return 0 if success,
 *         -ENODEV is library failed to initialize
 *         -EINVAL if invalid interface handle
 */
int lpeRelease(effect_handle_t interface)
{
    LpePreProcessing *lpeProcessing = LpePreProcessing::getInstance();
    return lpeProcessing->releaseEffect(interface);
}

/**
 * get effet descriptor.
 * Returns the descriptor of the effect engine which implementation UUID is
 * given as argument.
 *
 * @param[in] uuid pointer to the effect uuid.
 * @param[out] descriptor effect descriptor
 *
 * @return 0 if success, in this case, descriptor is set correctly,
 *         -ENODEV is library failed to initialize
 *         -EINVAL if invalid descriptor or uuid
 */
int lpeGetDescriptor(const effect_uuid_t *uuid, effect_descriptor_t *descriptor)
{
    LpePreProcessing *lpeProcessing = LpePreProcessing::getInstance();
    lpeProcessing->getEffectDescriptor(uuid, descriptor);
    return 0;
}

/**
 * Audio Effect Library entry point structure.
 * Every effect library must have a data structure named AUDIO_EFFECT_LIBRARY_INFO_SYM
 * and the fields of this data structure must begin with audio_effect_library_t
 */
audio_effect_library_t AUDIO_EFFECT_LIBRARY_INFO_SYM = {
    tag : AUDIO_EFFECT_LIBRARY_TAG,
    version : EFFECT_LIBRARY_API_VERSION,
    name : "Intel LPE Preprocessing Library",
    implementor : "Intel",
    create_effect : lpeCreate,
    release_effect : lpeRelease,
    get_descriptor : lpeGetDescriptor
};

} // extern "C"
