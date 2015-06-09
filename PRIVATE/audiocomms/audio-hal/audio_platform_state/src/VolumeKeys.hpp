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

namespace intel_audio
{

class VolumeKeys
{
public:
    /**
     * Enable or disable the wakeup from volume keys.
     *
     * @param[in] isEnabled true if enable from volume keys is requested, false otherwise.
     *
     * @return 0 is succesfull operation, -1 otherwise.
     */
    static int wakeup(bool isEnabled);

private:
    /**< tracks down state of wakeup property */
    static bool mWakeupEnabled;

    /**< used to represent the filepath to enabled_wakeup property file */
    static const char *const mGpioKeysWakeupEnable;

    /**< used to represent the filepath to disabled_wakeup property file */
    static const char *const mGpioKeysWakeupDisable;

    /**< id of volume down key */
    static const char *const mKeyVolumeDown;

    /**< id of volume up key */
    static const char *const mKeyVolumeUp;
};

} // namespace intel_audio
