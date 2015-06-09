/* Modem Manager - modem mcd source file
**
** Copyright (C) Intel 2014
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "logs.h"
#include "mdm_mcd.h"
#include "delay_do.h"
#include "common.h"

typedef struct mmgr_mcd_ctx {
    int fd;
    bool ssic_hack;
    bool off_allowed;
    bool flashless;
    bool ipc_ready_present;
    int filter;
    link_hdle_t *link;
} mmgr_mcd_ctx_t;

/**
 * Turns off the modem when the modem is declared out of service by MMGR.
 * In some platforms, the modem cannot be turned off. That is why a cold
 * reset is done instead. This is relevant for a flashless modem. But it
 * needs to be enhanced for a flashbased modem.
 * @TODO: fix this.
 *
 * @param [in] hdle module handle
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t mdm_mcd_off(const mdm_mcd_hdle_t *hdle)
{
    e_mmgr_errors_t ret;
    const mmgr_mcd_ctx_t *mcd = (mmgr_mcd_ctx_t *)hdle;

    ASSERT(mcd != NULL);

    if (mcd->off_allowed) {
        ret = mdm_mcd_down(hdle);
    } else {
        LOG_INFO("modem shutdown not allowed. Cold reset performed instead");
        ret = mdm_mcd_cold_reset(hdle);
    }

    return ret;
}

/**
 * Performs a modem cold reset
 *
 * @param [in] hdle module handle
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t mdm_mcd_cold_reset(const mdm_mcd_hdle_t *hdle)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    const mmgr_mcd_ctx_t *mcd = (mmgr_mcd_ctx_t *)hdle;

    ASSERT(mcd != NULL);

    if (mcd->ssic_hack) {
        if (!ioctl(mcd->fd, MDM_CTRL_POWER_OFF)) {
            LOG_INFO("MODEM OFF");
            /* wait for usb ssic interface to be removed */
            pthread_t thr;
            static delay_t cold_reset;
            cold_reset.delay_sec = 8;
            cold_reset.h = (void *)hdle;
            cold_reset.fn = (void (*)(void *))(mdm_mcd_up);
            pthread_create(&thr, NULL, delay_do, &cold_reset);
            pthread_detach(thr);
        } else {
            LOG_ERROR("couldn't power off modem: %s", strerror(errno));
            ret = E_ERR_FAILED;
        }
    } else if (!ioctl(mcd->fd, MDM_CTRL_COLD_RESET)) {
        LOG_INFO("MODEM COLD RESET");
    } else {
        LOG_DEBUG("COLD RESET failure: %s", strerror(errno));
        ret = E_ERR_FAILED;
    }

    return ret;
}

/**
 * Shuts down the modem
 *
 * @param [in] hdle module handle
 *
 * @return E_ERR_FAILED
 * @return E_ERR_SUCCESS
 */
e_mmgr_errors_t mdm_mcd_down(const mdm_mcd_hdle_t *hdle)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    const mmgr_mcd_ctx_t *mcd = (mmgr_mcd_ctx_t *)hdle;

    ASSERT(mcd != NULL);

    if (!mcd->ssic_hack)
        link_on_mdm_down(mcd->link);

    if (!ioctl(mcd->fd, MDM_CTRL_POWER_OFF)) {
        LOG_INFO("MODEM ELECTRICALLY SHUTDOWN");
        ret = E_ERR_SUCCESS;
    } else {
        LOG_DEBUG("Modem shutdown failure: %s", strerror(errno));
    }

    if (mcd->ssic_hack) {
        sleep(8);
        link_on_mdm_down(mcd->link);
    }

    return ret;
}

/**
 * Powers on modem
 *
 * @param [in] hdle module handle
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t mdm_mcd_up(const mdm_mcd_hdle_t *hdle)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    int state;
    const mmgr_mcd_ctx_t *mcd = (mmgr_mcd_ctx_t *)hdle;

    ASSERT(mcd != NULL);

    int delay = 0;
    if (mcd->ssic_hack)
        delay = 1;
    link_on_mdm_reset(mcd->link, delay);

    errno = 0;
    if (!ioctl(mcd->fd, MDM_CTRL_GET_STATE, &state)) {
        if (state & MDM_CTRL_STATE_OFF) {
            if (ioctl(mcd->fd, MDM_CTRL_POWER_ON)) {
                LOG_ERROR("failed to power on the modem: %s", strerror(errno));
                ret = E_ERR_FAILED;
            }
            /* W/A: For flashbased modem, the flashing window is missed after
             * a power on sequence. A cold reset is needed */
            if (!mcd->flashless) {
                sleep(1);
                ret = mdm_mcd_cold_reset((mdm_mcd_hdle_t *)mcd);
            }
        } else {
            ret = mdm_mcd_cold_reset((mdm_mcd_hdle_t *)mcd);
        }

        if (ret == E_ERR_SUCCESS)
            LOG_DEBUG("Modem successfully POWERED-UP");
        else
            link_on_mdm_down(mcd->link);
    } else {
        LOG_ERROR("failed to get current modem state");
        ret = E_ERR_FAILED;
    }

    return ret;
}

/**
 * Gets the modem state
 *
 * @param hdle module handle
 *
 * @return
 */
e_modem_events_type_t mdm_mcd_get_state(const mdm_mcd_hdle_t *hdle)
{
    int read = 0;
    const mmgr_mcd_ctx_t *mcd = (mmgr_mcd_ctx_t *)hdle;
    e_modem_events_type_t state = E_EV_NONE;

    ASSERT(mcd != NULL);

    if (ioctl(mcd->fd, MDM_CTRL_GET_STATE, &read)) {
        LOG_ERROR("couldn't get modem state %s", strerror(errno));
        goto out;
    }
    LOG_DEBUG("read: 0x%02X", read);

    if (read & MDM_CTRL_STATE_OFF) {
        LOG_INFO("state: OFF");
        state |= E_EV_MODEM_OFF;
    }

    if (read & MDM_CTRL_STATE_IPC_READY) {
        LOG_INFO("state: IPC_READY");
        state |= E_EV_IPC_READY;
    }

    if (read & MDM_CTRL_STATE_COREDUMP) {
        LOG_INFO("state: CORE DUMP");
        state |= E_EV_CORE_DUMP;
    }

    if (read & MDM_CTRL_STATE_FW_DOWNLOAD_READY) {
        LOG_INFO("state: firmware upload ready");
        state |= E_EV_FW_DOWNLOAD_READY;
    }

    read = 0;
    if (ioctl(mcd->fd, MDM_CTRL_GET_HANGUP_REASONS, &read)) {
        LOG_ERROR("failed to get hangup reason: %s", strerror(errno));
        goto out;
    }

    if (read & MDM_CTRL_HU_RESET) {
        LOG_INFO("state: SELF-RESET");
        state |= E_EV_MODEM_SELF_RESET;
    }

out:
    if (ioctl(mcd->fd, MDM_CTRL_CLEAR_HANGUP_REASONS))
        LOG_ERROR("failed to clear hangup reason");
    return state;
}

/**
 * Updates mcd filter
 *
 * @param [in] mcd module context
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
static e_mmgr_errors_t mdm_mcd_update_filter(const mmgr_mcd_ctx_t *mcd)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;

    ASSERT(mcd != NULL);

    if (!ioctl(mcd->fd, MDM_CTRL_SET_POLLED_STATES, &mcd->filter)) {
        LOG_INFO("MCD filter has been updated. Registered to: %s%s%s%s%s%s",
                 (mcd->filter & MDM_CTRL_STATE_OFF) ? "modem off, " : "",
                 (mcd->filter & MDM_CTRL_STATE_COLD_BOOT) ? "cold boot, " : "",
                 (mcd->filter & MDM_CTRL_STATE_WARM_BOOT) ? "warm boot, " : "",
                 (mcd->filter & MDM_CTRL_STATE_COREDUMP) ? "core dump, " : "",
                 (mcd->filter & MDM_CTRL_STATE_IPC_READY) ? "ipc ready, " : "",
                 (mcd->filter & MDM_CTRL_STATE_FW_DOWNLOAD_READY) ?
                 "firmware download ready, " : "");
    } else {
        LOG_ERROR("failed to set new filter: %s", strerror(errno));
        ret = E_ERR_FAILED;
    }

    return ret;
}

/**
 * Updates filter by adding new events
 *
 * @param [in] hdle module handle
 * @param [in] events new events
 * @param [in] overwrite
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t mdm_mcd_register(mdm_mcd_hdle_t *hdle, int events,
                                 bool overwrite)
{
    mmgr_mcd_ctx_t *mcd = (mmgr_mcd_ctx_t *)hdle;

    ASSERT(mcd != NULL);

    if (!overwrite)
        mcd->filter |= events;
    else
        mcd->filter = events;

    return mdm_mcd_update_filter(mcd);
}

/**
 * Updates filter by removing new events
 *
 * @param [in] hdle module handle
 * @param [in] events new events
 *
 * @return E_ERR_SUCCESS if successful
 * @return E_ERR_FAILED otherwise
 */
e_mmgr_errors_t mdm_mcd_unregister(mdm_mcd_hdle_t *hdle, int events)
{
    mmgr_mcd_ctx_t *mcd = (mmgr_mcd_ctx_t *)hdle;

    ASSERT(mcd != NULL);
    mcd->filter &= ~events;

    return mdm_mcd_update_filter(mcd);
}

static e_mmgr_errors_t mdm_mcd_configure(int fd, enum mdm_ctrl_board_type board,
                                         const char *mdm_name)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    struct mdm_ctrl_cfg cfg = { board, MODEM_UNSUP };

    ASSERT(mdm_name != NULL);

    if (!strcmp(mdm_name, "6360"))
        cfg.type = MODEM_6360;
    else if (!strcmp(mdm_name, "7160"))
        cfg.type = MODEM_7160;
    else if (!strcmp(mdm_name, "7260"))
        cfg.type = MODEM_7260;
    else if (!strcmp(mdm_name, "2230"))
        cfg.type = MODEM_2230;

    LOG_DEBUG("(board type: %d) (family :%d)", cfg.board, cfg.type);

    errno = 0;
    if (ioctl(fd, MDM_CTRL_SET_CFG, &cfg)) {
        if (EBUSY == errno) {
            LOG_DEBUG("MCD already initialized");
        } else {
            LOG_ERROR("failed to configure MCD (board type: %d) (family :%d)",
                      cfg.board, cfg.type);
            ret = E_ERR_FAILED;
        }
    }

    return ret;
}

/**
 * Gets the file descriptor
 *
 * @param hdle
 *
 * @return file descriptor
 */
int mdm_mcd_get_fd(const mdm_mcd_hdle_t *hdle)
{
    const mmgr_mcd_ctx_t *mcd = (mmgr_mcd_ctx_t *)hdle;

    ASSERT(mcd != NULL);

    return mcd->fd;
}

bool mdm_mcd_is_ipc_ready_present(const mdm_mcd_hdle_t *hdle)
{
    const mmgr_mcd_ctx_t *mcd = (mmgr_mcd_ctx_t *)hdle;

    ASSERT(mcd != NULL);

    return mcd->ipc_ready_present;
}

/**
 * Initializes the MCD module
 *
 * @param [in] mcd_cfg MCD configuration
 * @param [in] mdm_core modem configuration
 * @param [in] link link control module
 * @param [in] off_allowed off_allowed boolean
 * @param [in] ssic_hack
 *
 * @return a valid pointer. must be freed by user
 */
mdm_mcd_hdle_t *mdm_mcd_init(const mmgr_mcd_t *mcd_cfg,
                             const mdm_core_t *mdm_core,
                             link_hdle_t *link,
                             bool off_allowed,
                             bool ssic_hack)
{
    mmgr_mcd_ctx_t *mcd = calloc(1, sizeof(mmgr_mcd_ctx_t));

    ASSERT(mcd != NULL);

    ASSERT(mcd_cfg != NULL);
    ASSERT(mdm_core != NULL);
    ASSERT(link != NULL);

    mcd->fd = open(mcd_cfg->path, O_RDWR);

    if (mcd->fd == -1) {
        LOG_DEBUG("failed to open Modem Control Driver (%s) interface: %s",
                  mcd_cfg->path, strerror(errno));
        mdm_mcd_dispose((mdm_mcd_hdle_t *)mcd);
        mcd = NULL;
    } else {
        mdm_mcd_configure(mcd->fd, mcd_cfg->board, mdm_core->name);
        mcd->link = link;
        mcd->off_allowed = off_allowed;
        mcd->ssic_hack = ssic_hack;
        mcd->flashless = mdm_core->flashless;
        mcd->ipc_ready_present = mcd_cfg->board == BOARD_AOB;

        mcd->filter = MDM_CTRL_STATE_COREDUMP;
        mdm_mcd_update_filter(mcd);
    }

    return (mdm_mcd_hdle_t *)mcd;
}

/**
 * @brief mdm_mcd_dispose
 * @param hdle
 */
void mdm_mcd_dispose(mdm_mcd_hdle_t *hdle)
{
    mmgr_mcd_ctx_t *mcd = (mmgr_mcd_ctx_t *)hdle;

    ASSERT(mcd != NULL);

    close(mcd->fd);
    free(mcd);
}
