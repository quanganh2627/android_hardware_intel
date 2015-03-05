/*
 * Copyright (c) 2012-2013, Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

//#define LOG_NDEBUG 0

#include <utils/Log.h>
#include <errno.h>
#include <sys/types.h>
#include <stdbool.h>
#include "drm_hdmi.h"
#include "drm_hdcp.h"
#include "linux/psb_drm.h"
#include "xf86drm.h"
#include "xf86drmMode.h"
extern "C" {
#include "libsepdrm/sepdrm.h"
}



static int g_hdcpStatusCheckTimer = 0;
#define HDCP_ENABLE_NUM_OF_TRY      4
#define HDCP_CHECK_NUM_OF_TRY       1
#define HDCP_ENABLE_DELAY_USEC      30000 // 30 ms
#define HDCP_STATUS_CHECK_INTERVAL  2 // 2 seconds
// 120ms delay after disabling IED is required for successful hdcp
// authentication with some AV receivers anything less than 100ms
// resulted in Ri mismatch
#define HDCP_DISABLE_IED_DELAY_USEC 120000 // 120 ms

#define IED_SESSION_ID      0x11

// Forward declaration
static void drm_hdcp_check_link_status();
static bool drm_hdcp_start_link_checking();
static void drm_hdcp_stop_link_checking();
static bool drm_hdcp_enable_and_check();
static bool drm_hdcp_enable_hdcp_work();

static bool drm_hdcp_isSupported()
{
    int fd = drm_get_dev_fd();
    if (fd <= 0) {
        LOGE("Invalid DRM file descriptor.");
        return false;
    }
    unsigned int caps = 0;
    int ret = drmCommandRead(fd, DRM_PSB_QUERY_HDCP, &caps, sizeof(caps));
    if (ret != 0) {
        LOGE("Failed to query HDCP capability.");
        return false;
    }
    return caps != 0;
}

static bool drm_hdcp_query_display_ied_caps()
{
    int fd, ret, platform;
    fd = drm_get_dev_fd();
    if (fd <= 0) {
        LOGE("Invalid DRM file descriptor.");
        return false;
    }
    ret = drmCommandRead(fd, DRM_PSB_QUERY_HDCP_DISPLAY_IED_CAPS, &platform, sizeof(platform));
    if (ret != 0) {
        LOGE("Failed to query platform type.");
        return false;
    }
    return platform != 0;
}

static bool drm_hdcp_disable_display_ied()
{
    int fd, ret;
    fd = drm_get_dev_fd();
    if (fd <= 0) {
        LOGE("Invalid DRM file descriptor.");
        return false;
    }
    ret = drmCommandNone(fd, DRM_PSB_HDCP_DISPLAY_IED_OFF);
    if (ret != 0) {
        LOGE("Failed to disable HDCP-Display-IED.");
        return false;
    }
    return true;
}

static bool drm_hdcp_enable_display_ied()
{
    int fd, ret;
    fd = drm_get_dev_fd();
    if (fd <= 0) {
        LOGE("Invalid DRM file descriptor.");
        return false;
    }
    ret = drmCommandNone(fd, DRM_PSB_HDCP_DISPLAY_IED_ON);
    if (ret != 0) {
        LOGE("Failed to enable HDCP-Display-IED.");
        return false;
    }
    return true;
}


static bool drm_hdcp_enable()
{
    int fd = drm_get_dev_fd();
    if (fd <= 0) {
        LOGE("Invalid DRM file descriptor.");
        return false;
    }
    int ret = drmCommandNone(fd, DRM_PSB_ENABLE_HDCP);
    if (ret != 0) {
        LOGE("Failed to enable HDCP.");
        return false;
    }
    return true;
}

static bool drm_hdcp_disable()
{
    int fd = drm_get_dev_fd();
    if (fd <= 0) {
        LOGE("Invalid DRM file descriptor.");
        return false;
    }
    int ret = drmCommandNone(fd, DRM_PSB_DISABLE_HDCP);
    if (ret != 0) {
        LOGW("Failed to disable HDCP.");
        return false;
    }
    return true;
}

static bool drm_hdcp_isAuthenticated()
{
    int fd = drm_get_dev_fd();
    if (fd <= 0) {
        LOGE("Invalid DRM file descriptor.");
        return false;
    }
    unsigned int match = 0;
    int ret = drmCommandRead(fd, DRM_PSB_GET_HDCP_LINK_STATUS, &match, sizeof(match));
    if (ret != 0) {
        LOGE("Failed to check hdcp link status.");
        return false;
    }
    if (match) {
        LOGV("HDCP is authenticated.");
        return true;
    } else {
        LOGE("HDCP is not authenticated.");
        return false;
    }
}

// check whether there is an active IED session (Inline Decryption and Encryption)
static bool drm_check_ied_session()
{
    struct drm_lnc_video_getparam_arg arg;
    unsigned long temp;
    int ret = 0;
    int fd, offset;
    fd = drm_get_dev_fd();
    if (fd <= 0) {
        LOGE("Invalid DRM file descriptor.");
        return false;
    }
    offset = drm_get_ioctl_offset();
    if (offset <= 0) {
        LOGE("Invalid IOCTL offset.");
        return false;
    }

    arg.key = IMG_VIDEO_IED_STATE;
    arg.value = (uint64_t)((unsigned long) & temp);
    ret = drmCommandWriteRead(fd, offset, &arg, sizeof(arg));

    if (ret != 0) {
        LOGE("Failed to get IED session status.");
        return false;
    }
    if (temp == 1) {
        LOGI("IED session is active.");
        return true;
    } else {
        LOGI("IED session is inactive.");
        return false;
    }
}

static void drm_hdcp_check_link_status(union sigval sig)
{
    bool b = false;
    b = drm_hdcp_isAuthenticated();
    if (!b) {
	    LOGI("HDCP is not authenticated, restarting authentication process.");
	    drm_hdcp_enable_hdcp_work();
    }
}

static bool drm_hdcp_start_link_checking()
{
    int ret;
    struct sigevent sev;
    struct itimerspec its;

    if (g_hdcpStatusCheckTimer) {
        LOGW("HDCP status checking timer has been created.");
        return false;
    }

    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_value.sival_ptr = NULL;
    sev.sigev_notify_function = drm_hdcp_check_link_status;

    ret = timer_create(CLOCK_REALTIME, &sev, &g_hdcpStatusCheckTimer);
    if (ret != 0) {
        LOGE("Failed to create HDCP status checking timer.");
        return false;
    }

    its.it_value.tv_sec = -1; // never expire
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = HDCP_STATUS_CHECK_INTERVAL; // 2 seconds
    its.it_interval.tv_nsec = 0;

    ret = timer_settime(g_hdcpStatusCheckTimer, TIMER_ABSTIME, &its, NULL);
    if (ret != 0) {
        LOGE("Failed to set HDCP status checking timer.");
        timer_delete(g_hdcpStatusCheckTimer);
        g_hdcpStatusCheckTimer = 0;
        return false;
    }
    return true;
}

static void drm_hdcp_stop_link_checking()
{
    int ret;
    struct itimerspec its;

    if (g_hdcpStatusCheckTimer == 0) {
        LOGV("HDCP status checking timer has been deleted.");
        return;
    }

    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;

    ret = timer_settime(g_hdcpStatusCheckTimer, TIMER_ABSTIME, &its, NULL);
    if (ret != 0) {
        LOGE("Failed to reset HDCP status checking timer.");
    }

    timer_delete(g_hdcpStatusCheckTimer);
    g_hdcpStatusCheckTimer = 0;
}

static bool drm_hdcp_enable_and_check()
{
    LOGV("Entering %s", __func__);
    bool ret = false;
    int i, j;

    for (i = 0; i < HDCP_ENABLE_NUM_OF_TRY; i++) {
        LOGV("Try to enable and check HDCP at iteration %d", i);
        if (drm_hdcp_enable() == false) {
            if (drm_hdmi_getConnectionStatus() == 0) {
                LOGW("HDMI is disconnected, abort HDCP enabling and checking.");
                return true;
            }
        } else {
            for (j = 0; j < HDCP_CHECK_NUM_OF_TRY; j++) {
                if (drm_hdcp_isAuthenticated() == false) {
                    break;
                }
            }
            if (j == HDCP_CHECK_NUM_OF_TRY) {
                ret = true;
                break;
            }
        }
        // Adding delay to make sure panel receives video signal so it can start HDCP authentication.
        // (HDCP spec 1.3, section 2.3)
        usleep(HDCP_ENABLE_DELAY_USEC);
    }

    LOGV("Leaving %s", __func__);
    return ret;
}

static bool drm_hdcp_enable_hdcp_work()
{
    bool ret = true;
    sec_result_t res;

    if (drm_check_ied_session()) {
        if (drm_hdcp_query_display_ied_caps()) {
            LOGV("Disabling Display IED ");
            if (!drm_hdcp_disable_display_ied()) {
                LOGE("drm_hdcp_disable_display_ied FAILED!!!");
            }

            ret = drm_hdcp_enable_and_check();
            if (!ret) {
                // Don't return here as HDCP can be re-authenticated during periodic time check.
                LOGI("HDCP authentication will be restarted in %d seconds.", HDCP_STATUS_CHECK_INTERVAL);
            }
            LOGV("Re-enabling Display IED ");
            if (!drm_hdcp_enable_display_ied()) {
                LOGE("drm_hdcp_enable_display_ied FAILED!!!");
            }

        } else {
            res = Drm_Library_Init();
            if (res != 0) {
                LOGW("Drm_Library_Init failed. Error = %#x", res);
            }

            // Disable IED temporarily so HDCP authentication can succeed
            LOGV("Disabling IED session.");
            res = Drm_Playback_Pause(IED_SESSION_ID);
            if (res != 0) {
                LOGW("Failed to disable IED session. Error = %#x", res);
            } else {
                // Delay for IED disable to take effect and hence successfully
                // authenticate with some AV receivers
                LOGV("IED session disabled delay for %dusec",
                                   HDCP_DISABLE_IED_DELAY_USEC);
                usleep(HDCP_DISABLE_IED_DELAY_USEC);
            }

            ret = drm_hdcp_enable_and_check();
            if (!ret) {
                // Don't return here as HDCP can be re-authenticated during periodic time check.
                LOGI("HDCP authentication will be restarted in %d seconds.", HDCP_STATUS_CHECK_INTERVAL);
            }

            LOGV("Re-enabling IED session.");
            res = Drm_Playback_Resume(IED_SESSION_ID);
            if (res != 0) {
                LOGW("Failed to enable IED session. Error = %#x", res);
            }
        }
    } else {
        // IED session is inactive yet, start HDCP enabling and checking without tearing down IED session.
        ret = drm_hdcp_enable_and_check();
    }

    return ret;
}

void drm_hdcp_disable_hdcp(bool connected)
{
    LOGV("Entering %s", __func__);
    drm_hdcp_stop_link_checking();
    if (connected) {
        // disable HDCP if HDMI is  connected.
        drm_hdcp_disable();
    }

    LOGV("Leaving %s", __func__);
}

bool drm_hdcp_enable_hdcp()
{
    LOGV("Entering %s", __func__);
    bool ret = true;

    // We no longer check HDCP capability of the connected device here, the way to determine whether HDCP is supported in kernel is not reliable as it tries to read BKsv from the device,
    //which may not return BKsv as it has not received any frame when IED is enabled.
    //if (drm_hdcp_isSupported() == false) {
    if (false) {
        LOGW("HDCP is not supported, abort HDCP enabling.");
        ret = false;
        // this may be fake indication during quick plug/unplug cycle, and unplug event may be filtered out, so status timer still needs to be set.
    } else {
        ret = drm_hdcp_enable_hdcp_work();
    }

    // Ignore return value
    drm_hdcp_start_link_checking();

    LOGV("Leaving %s", __func__);
    // Return success as HDCP authentication failure may be recoverable.
    return ret;
}

