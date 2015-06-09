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

#include "SampleSpec.hpp"
#include <tinyalsa/asoundlib.h>
#include <stdint.h>
#include <sys/types.h>


namespace intel_audio
{

class AudioUtils
{
public:
    /**
     * Align on the nearest lower multiple of 16.
     * This function intends to align a value on the nearest lower value of 16 as AudioFlinger
     * expects a buffer to be a multiple of 16 in terms of frames.
     *
     * @param[in] u integer that may represent a number of frames.
     *
     * @return nearest lower multiple of 16.
     */
    static uint32_t alignOn16(uint32_t u);

    /**
     * Converts a number of bytes from one sample specification to another.
     * It translates a number of bytes in the source sample specification to a number of bytes
     * in the destination sample specification corresponding to the same period of time represented
     * by the source bytes.
     * This function asserts if overflow is detected.
     *
     * @param[in] bytes in the source sample specification.
     * @param[in] ssSrc source sample specifications.
     * @param[in] ssDst destination sample specifications.
     *
     * @return number of bytes in the destination sample specification.
     */
    static size_t convertSrcToDstInBytes(size_t bytes,
                                         const SampleSpec &ssSrc,
                                         const SampleSpec &ssDst);

    /**
     * Converts a number of frames from one sample specification to another.
     * It translates a number of frames in the source sample specification to a number of frames
     * in the destination sample specification corresponding to the same period of time represented
     * by the source frames.
     * This function asserts if overflow is detected.
     *
     * @param[in] frames in the source sample specification.
     * @param[in] ssSrc source sample specifications.
     * @param[in] ssDst destination sample specifications.
     *
     * @return number of frames in the destination sample specification.
     */
    static size_t convertSrcToDstInFrames(size_t frames,
                                          const SampleSpec &ssSrc,
                                          const SampleSpec &ssDst);

    /**
     * Converts a format from Tiny alsa to HAL domains.
     *
     * @param[in] format in Tiny alsa domain.
     *
     * @return format in HAL domain, note that AUDIO_FORMAT_PCM_8_24_BIT of tiny alsa is mapped on
     *              PCM_FORMAT_S32_LE of Audio HAL.
     *              It returns AUDIO_FORMAT_INVALID in case of unsupported Tiny format.
     */
    static audio_format_t convertTinyToHalFormat(pcm_format format);

    /**
     * Converts a format from HAL to Tiny alsa domains.
     *
     * @param[in] format in HAL domain.
     *
     * @return format in HAL domain, note that AUDIO_FORMAT_PCM_8_24_BIT of AudioHAL is mapped on
     *              PCM_FORMAT_S32_LE of Tiny alsa. It returns PCM_FORMAT_S16_LE format in case of
     *              unrecognized tiny alsa format.
     */
    static pcm_format convertHalToTinyFormat(audio_format_t format);

    /**
     * Converts a card name into its index.
     * Tiny ALSA does not provide any utility to translate a name into a card index.
     * This function gets information from procfs to translate a card name into the corresponding
     * index
     *
     * @param[in] name of the sound card.
     *
     * @return index if found, negative value otherwise.
     */
    static int getCardIndexByName(const char *name);

    /**
     * Converts a time in micro to milliseconds.
     * It converts a period of time in microsecond to millisecond rounding up to the nearest milli
     * second.
     *
     * @param[in] timeUsec time to convert in micro seconds.
     *
     * @return converted value in milliseconds.
     */
    static uint32_t convertUsecToMsec(uint32_t timeUsec);

    static const uint32_t mUsecToSec = 1000000; /**< Constant used or delays computation */

private:
    static const uint32_t mFrameAlignementOn16 = 16; /**< 16 is an audioflinger requirement. */

    static const uint32_t mUsecPerMsec = 1000;
};
}  // namespace intel_audio
