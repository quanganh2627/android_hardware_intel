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
#define LOG_TAG "VolumeKeys"

#include "VolumeKeys.hpp"
#include <utilities/Log.hpp>
#include <fcntl.h>

using audio_comms::utilities::Log;

namespace intel_audio
{

const char *const VolumeKeys::mGpioKeysWakeupEnable =
    "/sys/devices/platform/gpio-keys/enabled_wakeup";
const char *const VolumeKeys::mGpioKeysWakeupDisable =
    "/sys/devices/platform/gpio-keys/disabled_wakeup";

const char *const VolumeKeys::mKeyVolumeDown = "114";
const char *const VolumeKeys::mKeyVolumeUp = "115";

bool VolumeKeys::mWakeupEnabled = false;

int VolumeKeys::wakeup(bool isEnabled)
{
    if (mWakeupEnabled == isEnabled) {

        // Nothing to do, bailing out
        return 0;
    }
    Log::Debug() << __FUNCTION__ << ": volume keys wakeup=" << (isEnabled ? "enabled" : "disabled");

    int fd;
    int rc;

    const char *const gpioKeysWakeup =
        isEnabled ? mGpioKeysWakeupEnable : mGpioKeysWakeupDisable;
    fd = open(gpioKeysWakeup, O_RDWR);
    if (fd < 0) {
        Log::Error() << __FUNCTION__ << ": " << (isEnabled ? "enable" : "disable")
                     << " failed: Cannot open sysfs gpio-keys interface ("
                     << fd << ")";
        return -1;
    }
    rc = write(fd, mKeyVolumeDown, sizeof(mKeyVolumeDown));
    rc += write(fd, mKeyVolumeUp, sizeof(mKeyVolumeUp));
    close(fd);
    if (rc != (sizeof(mKeyVolumeDown) + sizeof(mKeyVolumeUp))) {
        Log::Error() << __FUNCTION__ << ": " << (isEnabled ? "enable" : "disable")
                     << " failed: sysfs gpio-keys write error";
        return -1;
    }
    mWakeupEnabled = isEnabled;
    Log::Debug() << __FUNCTION__ << ": " << (isEnabled ? "enable" : "disable")
                 << ": OK";
    return 0;
}

} // namespace intel_audio
