/* Modem Manager - modem link source file
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

#include "file.h"
#include "logs.h"
#include "pm.h"
#include "ctrl.h"
#include "link.h"


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <linux/spi/xmm2230_spi.h>

typedef struct link_ctx {
    link_t flash_ebl;
    link_t flash_fw;
    link_t bb;
    e_link_t cd_type;
    ctrl_handle_t ctrl;
    pm_handle_t pm;
    const bus_ev_hdle_t *bus_ev;
    bool ssic_hack;
} link_ctx_t;

static e_mmgr_errors_t link_set_spi_speed(link_spi_t *spi)
{
    e_mmgr_errors_t ret = E_ERR_FAILED;

    ASSERT(spi != NULL);

    int fd = open(spi->device, O_RDWR);
    if (fd > 0) {
        static const char *mode[] = { "normal", "high" };
        int cmd = SPI_IOC_FRAME_LEN_NORMAL;
        if (spi->high_speed)
            cmd = SPI_IOC_FRAME_LEN_DOWNLOAD;

        if (!ioctl(fd, cmd, NULL)) {
            LOG_DEBUG("SPI link configured in %s speed",
                      mode[spi->high_speed != 0]);
            ret = E_ERR_SUCCESS;
        } else {
            LOG_ERROR("failed to configure SPI speed");
        }
        close(fd);
    }

    return ret;
}

static const char *link_get_flash_interface(const link_t *link,
                                            const bus_ev_hdle_t *bus)
{
    const char *path = NULL;

    ASSERT(link != NULL);
    /* bus can be NULL */

    if (E_LINK_USB == link->type) {
        /* if bus is not NULL, we are in discovery mode */
        if (bus)
            path = bus_ev_get_flash_interface(bus);
        else
            path = link->usb.device;
    } else if (E_LINK_HSI == link->type) {
        path = link->hsi.device;
    } else if (E_LINK_UART == link->type) {
        path = link->uart.device;
    } else if (E_LINK_SPI == link->type) {
        path = link->spi.device;
    }

    return path;
}

/**
 * Configures link before a modem reset
 *
 * @param [in] hdle link handle
 * @param [in] delay @TODO: to be removed
 *
 * @return E_ERR_SUCCESS
 * @return E_ERR_FAILED
 */
e_mmgr_errors_t link_on_mdm_reset(const link_hdle_t *hdle, int delay)
{
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);

    return ctrl_on_mdm_reset(link->ctrl, delay);
}

/**
 * Configures link when flashing interface appears
 *
 * @param [in] hdle link handle
 *
 * @return E_ERR_SUCCESS
 * @return E_ERR_FAILED
 */
e_mmgr_errors_t link_on_mdm_flash(const link_hdle_t *hdle)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);

    if (E_LINK_HSI == link->flash_ebl.type) {
        ret = file_write(link->flash_ebl.hsi.ctrl, SYSFS_OPEN_MODE,
                         link->flash_ebl.hsi.cmd,
                         sizeof(link->flash_ebl.hsi.cmd));
    } else if (E_LINK_SPI == link->flash_fw.type) {
        ret = link_set_spi_speed(&link->flash_fw.spi);
    }

    return ret;
}

/**
 * Configures link when flash FW is complete
 *
 * @param [in] hdle link handle
 *
 * @return E_ERR_SUCCESS
 * @return E_ERR_FAILED
 */
e_mmgr_errors_t link_on_mdm_flash_complete(const link_hdle_t *hdle)
{
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);

    if (E_LINK_HSI == link->bb.type) {
        ret = file_write(link->bb.hsi.ctrl, SYSFS_OPEN_MODE,
                         link->bb.hsi.cmd,
                         sizeof(link->bb.hsi.cmd));
    } else if (E_LINK_SPI == link->bb.type) {
        ret = link_set_spi_speed(&link->bb.spi);
    }

    if (link->ssic_hack)
        ctrl_on_mdm_reset(link->ctrl, 15);

    return ret;
}

/**
 * Configures link when modem is UP
 *
 * @param [in] hdle link handle
 *
 * @return E_ERR_SUCCESS
 * @return E_ERR_FAILED
 */
e_mmgr_errors_t link_on_mdm_up(const link_hdle_t *hdle)
{
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);

    return pm_on_mdm_up(link->pm);
}

/**
 * Configures link when core dump interface appears
 *
 * @param [in] hdle link handle
 *
 * @return E_ERR_SUCCESS
 * @return E_ERR_FAILED
 */
e_mmgr_errors_t link_on_cd(const link_hdle_t *hdle)
{
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);

    return pm_on_cd(link->pm);;
}

/**
 * Configures link after core dump retrieval completion
 *
 * @param [in] hdle link handle
 *
 * @return E_ERR_SUCCESS
 * @return E_ERR_FAILED
 */
e_mmgr_errors_t link_on_cd_complete(const link_hdle_t *hdle)
{
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);

    return pm_on_cd_complete(link->pm);
}

/**
 * Configures link when the core dump retrieval has failed
 *
 * @param [in] hdle link handle
 *
 * @return E_ERR_SUCCESS
 * @return E_ERR_FAILED
 */
e_mmgr_errors_t link_on_cd_failure(const link_hdle_t *hdle)
{
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);
    return ctrl_on_cd_ipc_failure(link->ctrl);
}

/**
 * Configures link when modem is down
 *
 * @param [in] hdle link handle
 *
 * @return E_ERR_SUCCESS
 * @return E_ERR_FAILED
 */
e_mmgr_errors_t link_on_mdm_down(const link_hdle_t *hdle)
{
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);

    return ctrl_on_mdm_down(link->ctrl);
}

/**
 * Gets boat loader flashing interface path
 *
 * @param [in] hdle link handle
 *
 * @return path. Must not be freed by caller.
 */
const char *link_get_flash_ebl_interface(const link_hdle_t *hdle)
{
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);

    return link_get_flash_interface(&link->flash_ebl, link->bus_ev);
}

/**
 * Gets firmware flashing interface path
 *
 * @param [in] hdle link handle
 *
 * @return path. Must not be freed by caller.
 */
const char *link_get_flash_fw_interface(const link_hdle_t *hdle)
{
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);

    return link_get_flash_interface(&link->flash_fw, link->bus_ev);
}

/**
 * Gets baseband interface path
 *
 * @param [in] hdle link handle
 *
 * @return path. Must not be freed by caller.
 */
const char *link_get_bb_interface(const link_hdle_t *hdle)
{
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);

    return link_get_flash_interface(&link->bb, NULL);
}

/**
 * Gets boot loader flashing interface type
 *
 * @param [in] hdle link handle
 *
 * @return interface type.
 */
e_link_t link_get_flash_ebl_type(const link_hdle_t *hdle)
{
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);

    return link->flash_ebl.type;
}

/**
 * Gets firmware flashing interface type
 *
 * @param [in] hdle link handle
 *
 * @return interface type.
 */
e_link_t link_get_flash_fw_type(const link_hdle_t *hdle)
{
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);

    return link->flash_fw.type;
}

/**
 * Gets baseband interface type
 *
 * @param [in] hdle link handle
 *
 * @return interface type.
 */
e_link_t link_get_bb_type(const link_hdle_t *hdle)
{
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);

    return link->bb.type;
}

/**
 * Gets core dump interface type
 *
 * @param [in] hdle link handle
 *
 * @return interface type.
 */
e_link_t link_get_cd_type(const link_hdle_t *hdle)
{
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);

    return link->cd_type;
}

/**
 * Gets boot loader interface baudrate
 *
 * @param [in] hdle link handle
 *
 * @return baudrate value if link type is UART. 0 otherwise.
 */
int link_get_ebl_baudrate(const link_hdle_t *hdle)
{
    int baudrate = 0;
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);

    if (E_LINK_UART == link->flash_ebl.type)
        baudrate = link->flash_ebl.uart.baudrate;

    return baudrate;
}

/**
 * Initialize link module
 *
 * @param [in] links links parameters. Provided by TCS
 * @param [in] mcdr core dump configuration. Provided by TCS
 * @param [in] bus_ev bus event handler
 * @param [in] ssic_hack @TODO: remove this
 *
 * @return valid pointer. Must be freed by calling link_dipose.
 */
link_hdle_t *link_init(const mmgr_mdm_link_t *links, const mcdr_info_t *mcdr,
                       const bus_ev_hdle_t *bus_ev, bool ssic_hack)
{
    link_ctx_t *link = calloc(1, sizeof(link_ctx_t));

    ASSERT(link != NULL);
    ASSERT(mcdr != NULL);
    ASSERT(bus_ev != NULL);

    link->flash_ebl = links->flash_ebl;
    link->flash_fw = links->flash_fw;
    link->bb = links->baseband;
    link->bus_ev = bus_ev;
    link->ssic_hack = ssic_hack;
    link->cd_type = mcdr->link.type;

    link->ctrl = ctrl_init(links->baseband.type, &links->ctrl,
                           mcdr->link.type, &mcdr->ctrl);
    link->pm = pm_init(links->baseband.type, &links->power,
                       mcdr->link.type, &mcdr->power);

    ASSERT(link->ctrl != NULL);
    ASSERT(link->pm != NULL);

    return (link_hdle_t *)link;
}

/**
 * Disposes link module
 *
 * @param [in] hdle link handle
 */
void link_dispose(link_hdle_t *hdle)
{
    link_ctx_t *link = (link_ctx_t *)hdle;

    ASSERT(link != NULL);

    pm_dispose(link->pm);
    ctrl_dispose(link->ctrl);
    free(link);
}
