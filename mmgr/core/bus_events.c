/* Modem Manager - bus events source file
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

#include <inttypes.h>
#include <linux/limits.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <usbhost/usbhost.h>
#include "errors.h"
#include "logs.h"
#include "tty.h"
#include "events_manager.h"
#include "bus_events.h"

typedef enum {
    EV_NONE,
    EV_ADDED,
    EV_DELETED
} e_bus_ev_t;

typedef struct {
    char path[PATH_MAX];
    e_bus_ev_t event;
} bus_event_t;

typedef struct {
    int i;
    bus_event_t evs[BUS_EV_CAPACITY];
} bus_ev_cli_ctx_t;

typedef struct {
    struct usb_host_context *ctx;
    bus_ev_cli_ctx_t cli_ctx;
    int mdm_state;
    int wd_fd;
    char modem_flash_path[PATH_MAX];
    char modem_bb_path[PATH_MAX];
    char modem_cd_path[PATH_MAX];
    uint16_t modem_flash_pid;
    uint16_t modem_flash_vid;
    uint16_t modem_bb_pid;
    uint16_t modem_bb_vid;
    uint16_t modem_reconfig_pid;
    uint16_t modem_reconfig_vid;
    uint16_t mcdr_bb_pid;
    uint16_t mcdr_bb_vid;
} bus_ev_t;

/**
 * add bus event
 *
 * @private
 *
 * @param [in] path device path
 * @param [in] event event type (remove or add)
 * @param [out] ctx context
 *
 * @return E_ERR_FAILED if bus events capacity has been reached
 * @return E_ERR_SUCCESS if successful
 */
static e_mmgr_errors_t add_ev(const char *path, uint8_t event, void *ctx)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    bus_ev_cli_ctx_t *ev_ctx = (bus_ev_cli_ctx_t *)ctx;
    int i = ev_ctx->i;

    if (i >= BUS_EV_CAPACITY) {
        LOG_ERROR("too much events at once, reached %d CAPACITY -> lost event",
                  BUS_EV_CAPACITY);
        ret = E_ERR_FAILED;
    } else {
        bus_event_t *evs = ev_ctx->evs;
        evs[i].event = event;
        strncpy(evs[i].path, path, sizeof(evs[i].path));
        ev_ctx->i++;
    }

    return ret;
}

/**
 * add device
 *
 * @private
 *
 * @param [in] path device path
 * @param [out] ctx context
 *
 * @return E_ERR_FAILED if bus events capacity has been reached
 * @return E_ERR_SUCCESS if successful
 */
static int device_added_cb(const char *path, void *ctx)
{
    return add_ev(path, EV_ADDED, ctx);
}

/**
 * remove device
 *
 * @private
 *
 * @param [in] path device path
 * @param [out] ctx context
 *
 * @return E_ERR_FAILED if bus events capacity has been reached
 * @return E_ERR_SUCCESS if successful
 */
static int device_rmed_cb(const char *path, void *ctx)
{
    return add_ev(path, EV_DELETED, ctx);
}

/**
 * check PID and VID
 *
 * @private
 *
 * @param [in] path device path
 * @param [in] pid
 * @param [out] vid
 *
 * @return 1 if PID and VID are correct
 * @return 0 if PID and VID are not correct
 */
int is_pid_and_vid(struct usb_device *dev, uint16_t pid, uint16_t vid)
{
    uint16_t ppid = usb_device_get_product_id(dev);
    uint16_t pvid = usb_device_get_vendor_id(dev);

    LOG_DEBUG("Event bus pid 0x%.4x vid 0x%.4x", ppid, pvid);

    return ppid == pid && pvid == vid;
}

/**
 * Returns the current event
 *
 * @param [in] h
 *
 * @return 0 if h is NULL
 * @return current event otherwise
 */
int bus_ev_read(bus_ev_hdle_t *h)
{
    bus_ev_t *bus_events = (bus_ev_t *)h;
    int ev = 0;

    if (bus_events)
        ev = usb_host_read_event(bus_events->ctx);

    return ev;
}

/**
 * handle bus event
 *
 * @param [in, out] h
 *
 * @return E_ERR_SUCCESS
 */
e_mmgr_errors_t bus_ev_hdle_events(bus_ev_hdle_t *h)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;
    bus_ev_t *bus_events = (bus_ev_t *)h;

    for (int i = 0; i < bus_events->cli_ctx.i; i++) {
        LOG_DEBUG("Event: %s %d", bus_events->cli_ctx.evs[i].path,
                  bus_events->cli_ctx.evs[i].event);
        if (bus_events->cli_ctx.evs[i].event == EV_ADDED) {
            struct usb_device *dev;

            LOG_DEBUG("EVENT ADDED");

            dev = usb_device_open(bus_events->cli_ctx.evs[i].path);
            if (dev == NULL) {
                LOG_ERROR("Failed to open path: %s",
                          bus_events->cli_ctx.evs[i].path);
            } else {
                if (is_pid_and_vid(dev,
                                   bus_events->modem_flash_pid,
                                   bus_events->modem_flash_vid)) {
                    bus_events->mdm_state |= MDM_FLASH_READY;
                    strncpy(bus_events->modem_flash_path,
                            bus_events->cli_ctx.evs[i].path, PATH_MAX);
                    ret = E_ERR_SUCCESS;
                    LOG_DEBUG("+Modem flash is READY");
                } else if (is_pid_and_vid(dev,
                                          bus_events->modem_bb_pid,
                                          bus_events->modem_bb_vid)) {
                    bus_events->mdm_state |= MDM_BB_READY;
                    strncpy(bus_events->modem_bb_path,
                            bus_events->cli_ctx.evs[i].path, PATH_MAX);
                    ret = E_ERR_SUCCESS;
                    LOG_DEBUG("+Modem base band READY");
                } else if (is_pid_and_vid(dev,
                                          bus_events->modem_reconfig_pid,
                                          bus_events->modem_reconfig_vid)) {
                    bus_events->mdm_state |= MDM_BB_READY;
                    strncpy(bus_events->modem_bb_path,
                            bus_events->cli_ctx.evs[i].path, PATH_MAX);
                    ret = E_ERR_SUCCESS;
                    LOG_DEBUG("+Modem base band READY");
                } else if (is_pid_and_vid(dev,
                                          bus_events->mcdr_bb_pid,
                                          bus_events->mcdr_bb_vid)) {
                    bus_events->mdm_state |= MDM_CD_READY;
                    strncpy(bus_events->modem_cd_path,
                            bus_events->cli_ctx.evs[i].path, PATH_MAX);
                    ret = E_ERR_SUCCESS;
                    LOG_DEBUG("+Modem core dump READY");
                }

                usb_device_close(dev);
            }
        } else if (bus_events->cli_ctx.evs[i].event == EV_DELETED) {
            LOG_DEBUG("EVENT DELETED");
            if (strncmp(bus_events->modem_flash_path,
                        bus_events->cli_ctx.evs[i].path, PATH_MAX) == 0) {
                bus_events->mdm_state &= ~MDM_FLASH_READY;
                bus_events->modem_flash_path[0] = '\0';
                ret = E_ERR_SUCCESS;
                LOG_DEBUG("-Modem flash not READY");
            } else if (strncmp(bus_events->modem_bb_path,
                               bus_events->cli_ctx.evs[i].path,
                               PATH_MAX) == 0) {
                bus_events->mdm_state &= ~MDM_BB_READY;
                bus_events->modem_bb_path[0] = '\0';
                ret = E_ERR_SUCCESS;
                LOG_DEBUG("-Modem base band not READY");
            } else if (strncmp(bus_events->modem_cd_path,
                               bus_events->cli_ctx.evs[i].path,
                               PATH_MAX) == 0) {
                bus_events->mdm_state &= ~MDM_CD_READY;
                bus_events->modem_cd_path[0] = '\0';
                ret = E_ERR_SUCCESS;
                LOG_DEBUG("-Modem core dump not READY");
            }
        }
    }

    bus_events->cli_ctx.i = 0;
    return ret;
}

/**
 * Returns current bus state
 *
 * @param [in] h bus handler
 *
 * @return 0 if h is NULL
 * @return current state
 */
int bus_ev_get_state(bus_ev_hdle_t *h)
{
    bus_ev_t *bus_events = (bus_ev_t *)h;
    int state = 0;

    if (bus_events)
        state = bus_events->mdm_state;

    return state;
}

/**
 * Returns bus file descriptor
 *
 * @param [in] h bus handler
 *
 * @return CLOSED_FD if h is NULL
 * @return file descriptor otherwise
 */
int bus_ev_get_fd(bus_ev_hdle_t *h)
{
    int fd = CLOSED_FD;
    bus_ev_t *bus_events = (bus_ev_t *)h;

    if (bus_events)
        fd = bus_events->wd_fd;

    return fd;
}

/**
 * Starts bus module
 *
 * @param [in] h bus handler
 *
 * @return E_ERR_SUCCESS otherwise
 */
e_mmgr_errors_t bus_ev_start(bus_ev_hdle_t *h)
{
    bus_ev_t *bus_events = (bus_ev_t *)h;

    ASSERT(bus_events != NULL);

    bus_events->wd_fd = usb_host_get_fd(bus_events->ctx);

    return E_ERR_SUCCESS;
}

/**
 * bus event initialization
 *
 * @param [in] flash
 * @param [in] bb baseband
 * @param [in] mcdr core dump
 *
 * @return a valid bus_ev_hdle_t pointer if succeed
 * @return NULL otherwise
 */
bus_ev_hdle_t *bus_ev_init(link_t *flash, link_t *bb, link_t *reconfig_usb,
                           link_t *mcdr)
{
    bool err = false;
    bool usb = false;
    bus_ev_t *bus_events = NULL;

    ASSERT(flash != NULL);
    ASSERT(bb != NULL);
    ASSERT(mcdr != NULL);

    bus_events = calloc(1, sizeof(bus_ev_t));
    if (!bus_events) {
        LOG_ERROR("memory allocation failed");
        goto err;
    }

    bus_events->wd_fd = CLOSED_FD;

    if (flash->type == E_LINK_USB) {
        usb = true;
        if ((flash->usb.pid != 0) && (flash->usb.vid != 0)) {
            bus_events->modem_flash_pid = flash->usb.pid;
            bus_events->modem_flash_vid = flash->usb.vid;
        } else {
            LOG_ERROR("wrong PID/VID for the flashing interface");
            err = true;
        }
    }

    if (bb->type == E_LINK_USB) {
        usb = true;
        if ((bb->usb.pid != 0) && (bb->usb.vid != 0)) {
            bus_events->modem_bb_pid = bb->usb.pid;
            bus_events->modem_bb_vid = bb->usb.vid;
        } else {
            LOG_ERROR("wrong PID/VID for the baseband interface");
            err = true;
        }
    }

    if (reconfig_usb->type == E_LINK_USB) {
        usb = true;
        if ((reconfig_usb->usb.pid != 0) && (reconfig_usb->usb.vid != 0)) {
            bus_events->modem_reconfig_pid = reconfig_usb->usb.pid;
            bus_events->modem_reconfig_vid = reconfig_usb->usb.vid;
        } else {
            LOG_ERROR("Wrong PID/VID for the reconfig_usb interface");
            err = true;
        }
    }

    if (mcdr->type == E_LINK_USB) {
        usb = true;
        if ((mcdr->usb.pid != 0) && (mcdr->usb.vid != 0)) {
            bus_events->mcdr_bb_pid = mcdr->usb.pid;
            bus_events->mcdr_bb_vid = mcdr->usb.vid;
        } else {
            LOG_ERROR("wrong PID/VID for the core dump interface");
            err = true;
        }
    }

    if (usb && !err) {
        if ((bus_events->ctx = usb_host_init()) == NULL)
            goto err;

        /* @TODO: handle errors */
        usb_host_load(bus_events->ctx, device_added_cb, device_rmed_cb, NULL,
                      &bus_events->cli_ctx);
        /* when calling usb_host_load, there's a call to find_existing_devices
        * which triggers added_cb events so, there's been events ... maybe. */
        bus_ev_hdle_events((bus_ev_hdle_t *)bus_events);
    }

    return (bus_ev_hdle_t *)bus_events;

err:
    bus_ev_dispose((bus_ev_hdle_t *)bus_events);
    return NULL;
}

/**
 * Disposes the bus event module
 *
 * param [in] h bus handle
 *
 * @return E_ERR_SUCCESS otherwise
 */
e_mmgr_errors_t bus_ev_dispose(bus_ev_hdle_t *h)
{
    bus_ev_t *bus_events = (bus_ev_t *)h;

    if (bus_events) {
        if (bus_events->ctx)
            usb_host_cleanup(bus_events->ctx);
        free(bus_events);
    }

    return E_ERR_SUCCESS;
}

/**
 * Returns current flash interface
 * The return pointer must not be freed by caller.
 *
 * @param [in] h bus handle
 *
 * @return NULL if h is NULL
 * @return flash interface
 */
const char *bus_ev_get_flash_interface(const bus_ev_hdle_t *h)
{
    bus_ev_t *bus_events = (bus_ev_t *)h;
    char *path = NULL;

    if (bus_events)
        path = bus_events->modem_flash_path;

    return path;
}
