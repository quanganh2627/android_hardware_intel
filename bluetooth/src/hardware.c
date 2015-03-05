/******************************************************************************
 *
 *  Copyright (C) 2013-2014 Intel Mobile Communications GmbH
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/******************************************************************************
 *
 *  Filename:      hardware.c
 *
 *  Description:   Contains controller-specific functions, like
 *                      firmware patch download
 *                      low power mode operations
 *
 ******************************************************************************/

#define LOG_TAG "bt_hwcfg"

#include <utils/Log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <cutils/properties.h>
#include <stdlib.h>
#include "bt_hci_bdroid.h"
#include "bt_vendor.h"
#include "userial.h"
#include "userial_vendor.h"
#include "upio.h"

/******************************************************************************
**  Constants & Macros
******************************************************************************/

#ifndef BTHW_DBG
#define BTHW_DBG FALSE
#endif

#if (BTHW_DBG == TRUE)
#define BTHWDBG(param, ...) {ALOGD(param, ## __VA_ARGS__);}
#else
#define BTHWDBG(param, ...) {}
#endif

#define FW_PATCHFILE_EXTENSION      ".seq"
#define FW_PATCHFILE_EXTENSION_LEN  4

#define HCI_CMD_MAX_LEN             258
#define LINE_LEN_MAX                1024

#define HCI_RESET                               0x0C03
#define HCI_VSC_WRITE_UART_CLOCK_SETTING        0xFC45
#define HCI_VSC_UPDATE_BAUDRATE                 0xFC18
#define HCI_READ_LOCAL_NAME                     0x0C14
#define HCI_VSC_DOWNLOAD_MINIDRV                0xFC2E
#define HCI_VSC_WRITE_BD_ADDR                   0xFC01
#define HCI_VSC_WRITE_SLEEP_MODE                0xFC27
#define HCI_VSC_WRITE_SCO_PCM_INT_PARAM         0xFC1C
#define HCI_VSC_WRITE_PCM_DATA_FORMAT_PARAM     0xFC1E
#define HCI_VSC_WRITE_I2SPCM_INTERFACE_PARAM    0xFC6D
#define HCI_VSC_LAUNCH_RAM                      0xFC4E
#define HCI_READ_LOCAL_BDADDR                   0x1009
#define HCI_INTEL_MANUFACTURE                   0xFC11
#define HCI_INTEL_RDSW_VERSION                  0xFC05
#define HCI_INTEL_MEMWRITE                      0xFC8E
#if (SW_RFKILL_CMD_SUPPORTED == TRUE)
#define HCI_INTEL_SW_RF_KILL                    0xFC3F
#endif
#define HCI_EVT_CMD_STAT_EVT_CODE               0x0F
#define HCI_EVT_CMD_CMPL_EVT_CODE               0x0E

#define HCI_EVT_CMD_STAT_STATUS_RET_BYTE        2
#define HCI_EVT_CMD_CMPL_STATUS_RET_BYTE        5
#define HCI_EVT_CMD_CMPL_LOCAL_NAME_STRING      6
#define HCI_EVT_CMD_CMPL_LOCAL_BDADDR_ARRAY     6
#define HCI_EVT_CMD_CMPL_OPCODE                 3
#define LPM_CMD_PARAM_SIZE                      12
#define UPDATE_BAUDRATE_CMD_PARAM_SIZE          6
#define HCI_CMD_PREAMBLE_SIZE                   3
#define HCD_REC_PAYLOAD_LEN_BYTE                2
#define BD_ADDR_LEN                             6
#define LOCAL_NAME_BUFFER_LEN                   32
#define LOCAL_BDADDR_PATH_BUFFER_LEN            256
#define HCI_INTEL_MANUFACTURE_PARAM_SIZE        2


#define STREAM_TO_UINT16(u16, p) {u16 = ((uint16_t)(*(p)) + (((uint16_t)(*((p) + 1))) << 8)); (p) += 2;}
#define UINT16_TO_STREAM(p, u16) {*(p)++ = (uint8_t)(u16); *(p)++ = (uint8_t)((u16) >> 8);}
#define UINT32_TO_STREAM(p, u32) {*(p)++ = (uint8_t)(u32); *(p)++ = (uint8_t)((u32) >> 8); *(p)++ = (uint8_t)((u32) >> 16); *(p)++ = (uint8_t)((u32) >> 24);}

/******************************************************************************
**  Local type definitions
******************************************************************************/

/* Hardware Configuration State */
enum {
    HW_CFG_START = 1,
    HW_CFG_SET_UART_CLOCK,
    HW_CFG_SET_UART_BAUD_1,
    HW_CFG_READ_LOCAL_NAME,
    HW_CFG_DL_MINIDRIVER,
    HW_CFG_DL_FW_PATCH,
    HW_CFG_SET_UART_BAUD_2,
    HW_CFG_SET_BD_ADDR
#if (USE_CONTROLLER_BDADDR == TRUE)
    , HW_CFG_READ_BD_ADDR
#endif
    , HW_CFG_SUCCESS,
    HW_CFG_FAIL,
    HW_CFG_INTEL_RDSW_VERSION,
    HW_CFG_INTEL_MANUFACTURE_ON,
    HW_CFG_INTEL_MEMWRITE,
    HW_CFG_INTEL_OPEN_PATCHFILE,
    HW_CFG_INTEL_RDSW_VERSION_RECHECK
#if (SW_RFKILL_CMD_SUPPORTED == TRUE)
    , HW_CFG_INTEL_SW_RF_KILL
#endif
};

/* h/w config control block */
typedef struct
{
    uint8_t state;                          /* Hardware configuration state */
    char    local_chip_name[LOCAL_NAME_BUFFER_LEN];
    uint8_t is_patch_enabled;               /* Is patch is enabled? 2: enabled 0:not enabled */
    uint8_t next_state;                     /* next state after manufacture off*/

} bt_hw_cfg_cb_t;

/* low power mode parameters */
typedef struct
{
    uint8_t sleep_mode;                     /* 0(disable),1(UART),9(H5) */
    uint8_t host_stack_idle_threshold;      /* Unit scale 300ms/25ms */
    uint8_t host_controller_idle_threshold; /* Unit scale 300ms/25ms */
    uint8_t bt_wake_polarity;               /* 0=Active Low, 1= Active High */
    uint8_t host_wake_polarity;             /* 0=Active Low, 1= Active High */
    uint8_t allow_host_sleep_during_sco;
    uint8_t combine_sleep_mode_and_lpm;
    uint8_t enable_uart_txd_tri_state;      /* UART_TXD Tri-State */
    uint8_t sleep_guard_time;               /* sleep guard time in 12.5ms */
    uint8_t wakeup_guard_time;              /* wakeup guard time in 12.5ms */
    uint8_t txd_config;                     /* TXD is high in sleep state */
    uint8_t pulsed_host_wake;               /* pulsed host wake if mode = 1 */
} bt_lpm_param_t;

/* Firmware re-launch settlement time */
typedef struct {
    const char *chipset_name;
    const uint32_t delay_time;
} fw_settlement_entry_t;


/******************************************************************************
**  Externs
******************************************************************************/

void hw_config_cback(void *p_evt_buf);
extern uint8_t vnd_local_bd_addr[BD_ADDR_LEN];


/******************************************************************************
**  Static variables
******************************************************************************/

static int fw_patchfile_empty = 0;
static char fw_patchfile_path[PATH_MAX] = FW_PATCHFILE_LOCATION;
static char fw_patchfile_name[NAME_MAX];
#if (VENDOR_LIB_RUNTIME_TUNING_ENABLED == TRUE)
static int fw_patch_settlement_delay = -1;
#endif

static bt_hw_cfg_cb_t hw_cfg_cb;

static bt_lpm_param_t lpm_param =
{
    LPM_SLEEP_MODE,
    LPM_IDLE_THRESHOLD,
    LPM_HC_IDLE_THRESHOLD,
    LPM_BT_WAKE_POLARITY,
    LPM_HOST_WAKE_POLARITY,
    LPM_ALLOW_HOST_SLEEP_DURING_SCO,
    LPM_COMBINE_SLEEP_MODE_AND_LPM,
    LPM_ENABLE_UART_TXD_TRI_STATE,
    0,  /* not applicable */
    0,  /* not applicable */
    0,  /* not applicable */
    LPM_PULSED_HOST_WAKE
};

#if (!defined(SCO_USE_I2S_INTERFACE) || (SCO_USE_I2S_INTERFACE == FALSE))
static uint8_t bt_sco_param[SCO_PCM_PARAM_SIZE] =
{
    SCO_PCM_ROUTING,
    SCO_PCM_IF_CLOCK_RATE,
    SCO_PCM_IF_FRAME_TYPE,
    SCO_PCM_IF_SYNC_MODE,
    SCO_PCM_IF_CLOCK_MODE
};

static uint8_t bt_pcm_data_fmt_param[PCM_DATA_FORMAT_PARAM_SIZE] =
{
    PCM_DATA_FMT_SHIFT_MODE,
    PCM_DATA_FMT_FILL_BITS,
    PCM_DATA_FMT_FILL_METHOD,
    PCM_DATA_FMT_FILL_NUM,
    PCM_DATA_FMT_JUSTIFY_MODE
};
#else
static uint8_t bt_sco_param[SCO_I2SPCM_PARAM_SIZE] =
{
    SCO_I2SPCM_IF_MODE,
    SCO_I2SPCM_IF_ROLE,
    SCO_I2SPCM_IF_SAMPLE_RATE,
    SCO_I2SPCM_IF_CLOCK_RATE
};
#endif

/*
 * The look-up table of recommended firmware settlement delay (milliseconds) on
 * known chipsets.
 */
static const fw_settlement_entry_t fw_settlement_table[] = {
    {"BCM43241", 200},
    {(const char *) NULL, 100}  // Giving the generic fw settlement delay setting.
};

/******************************************************************************
**  Static functions
******************************************************************************/

/******************************************************************************
**  Controller Initialization Static Functions
******************************************************************************/

/*******************************************************************************
**
** Function         hw_strncmp
**
** Description      Used to compare two strings in caseless
**
** Returns          0: match, otherwise: not match
**
*******************************************************************************/
static int hw_strncmp (const char *p_str1, const char *p_str2, const int len)
{
    int i;

    if (!p_str1 || !p_str2)
        return (1);

    for (i = 0; i < len; i++)
    {
        if (toupper(p_str1[i]) != toupper(p_str2[i]))
            return (i+1);
    }

    return 0;
}

/*******************************************************************************
**
** Function         hw_config_findpatch
**
** Description      Search for a proper firmware patch file
**                  The selected firmware patch file name with full path
**                  will be stored in the input string parameter, i.e.
**                  p_chip_id_str, when returns.
**
** Returns          TRUE when found the target patch file, otherwise FALSE
**
*******************************************************************************/
static uint8_t hw_config_findpatch(char *p_chip_id_str)
{
    DIR *dirp;
    struct dirent *dp;
    int filenamelen;
    uint8_t retval = FALSE;

    BTHWDBG("Target name = [%s]", p_chip_id_str);

    if (strlen(fw_patchfile_name)> 0)
    {
        /* If specific filepath and filename have been given in run-time
         * configuration /etc/bluetooth/bt_vendor.conf file, we will use them
         * to concatenate the filename to open rather than searching a file
         * matching to chipset name in the fw_patchfile_path folder.
         */
        snprintf(p_chip_id_str, strlen(fw_patchfile_path), "%s", fw_patchfile_path);
        if (fw_patchfile_path[strlen(fw_patchfile_path)- 1] != '/')
        {
            strncat(p_chip_id_str, "/", 1);
        }
        strncat(p_chip_id_str, fw_patchfile_name, strlen(fw_patchfile_name));

        ALOGI("FW patchfile: %s", p_chip_id_str);
        return TRUE;
    }

    if ((dirp = opendir(fw_patchfile_path)) != NULL)
    {
        /* Fetch next filename in patchfile directory */
        while ((dp = readdir(dirp)) != NULL)
        {
            /* Check if filename starts with chip-id name */
            if ((hw_strncmp(dp->d_name, p_chip_id_str, strlen(p_chip_id_str))
                ) == 0)
            {
                /* Check if it has .seq extenstion */
                filenamelen = strlen(dp->d_name);
                if ((filenamelen >= FW_PATCHFILE_EXTENSION_LEN) &&
                    ((hw_strncmp(
                          &dp->d_name[filenamelen-FW_PATCHFILE_EXTENSION_LEN],
                          FW_PATCHFILE_EXTENSION,
                          FW_PATCHFILE_EXTENSION_LEN)
                     ) == 0))
                {
                    ALOGI("Found patchfile: %s/%s",
                        fw_patchfile_path, dp->d_name);

                    /* Make sure length does not exceed maximum */
                    if ((filenamelen + strlen(fw_patchfile_path)) >
                        (PATH_MAX - 2))
                    {
                        ALOGE("Invalid patchfile name (too long)");
                    }
                    else
                    {
                        memset(p_chip_id_str, 0, NAME_MAX);
                        /* Found patchfile. Store location and name */
                        strncpy(p_chip_id_str, fw_patchfile_path, strlen(fw_patchfile_path));
                        if (fw_patchfile_path[
                            strlen(fw_patchfile_path)- 1
                            ] != '/')
                        {
                            strncat(p_chip_id_str, "/", 1);
                        }
                        strncat(p_chip_id_str, dp->d_name, strlen(dp->d_name));
                        retval = TRUE;
                    }
                    break;
                }
            }
        }

        closedir(dirp);

        if (retval == FALSE)
        {
            ALOGE("Could not find patchfile %s at %s", p_chip_id_str,fw_patchfile_path);
        }
    }
    else
    {
        ALOGE("Could not open %s", fw_patchfile_path);
    }

    return (retval);
}

/*******************************************************************************
**
** Function         char_to_hex
**
** Description      Convert char to hex
**
** Returns          hex value of the character
**
*******************************************************************************/
unsigned char char_to_hex(char c)
{
    volatile uint8_t x;
    char str[2];
    str[0] = c;
    str[1] = '\0';
    x = strtol(str, NULL, 16);


    return x;
}

/*******************************************************************************
**
** Function         form_byte
**
** Description      Convert input to a byte
**
** Returns          formed byte
**
*******************************************************************************/
unsigned char form_byte(char msb, char lsb)
{
    unsigned char byte;
    byte = (char_to_hex(msb)) << 4;
    byte |= char_to_hex(lsb);
    return byte;
}


/*******************************************************************************
**
** Function         form_word
**
** Description      Convert input to a word
**
** Returns          formed word
**
*******************************************************************************/
uint16_t form_word(uint8_t msb, uint8_t lsb)
{
    uint16_t byte;
    byte = msb << 8;
    byte |= lsb;
    return byte;
}

/*******************************************************************************
**
** Function        hw_config_manufacture_mode_off
**
** Description     sends the manufacture mode off command to controller
**
** Returns         None
**
*******************************************************************************/
static uint8_t hw_config_manufacture_mode_off(HC_BT_HDR *p_buf)
{
    uint8_t* p = (uint8_t *) (p_buf + 1);
    ALOGI("HW_CFG_INTEL_MANUFACTURE_OFF");
    UINT16_TO_STREAM(p, HCI_INTEL_MANUFACTURE);
    *p++ = HCI_INTEL_MANUFACTURE_PARAM_SIZE; /* parameter length */
    *p++ = 0x0;
    *p++ = hw_cfg_cb.is_patch_enabled;

    p_buf->len = HCI_CMD_PREAMBLE_SIZE +
    HCI_INTEL_MANUFACTURE_PARAM_SIZE;
    hw_cfg_cb.state = hw_cfg_cb.next_state;

    return bt_vendor_cbacks->xmit_cb(HCI_INTEL_MANUFACTURE,
        p_buf, hw_config_cback);
}


/*******************************************************************************
**
** Function         hw_config_cback
**
** Description      Callback function for controller configuration
**
** Returns          None
**
*******************************************************************************/
void hw_config_cback(void *p_mem)
{
    HC_BT_HDR *p_evt_buf = (HC_BT_HDR *) p_mem;
    uint8_t     *p;
    int16_t     status = -1;
    HC_BT_HDR   *p_buf = NULL;
    uint8_t     is_proceeding = FALSE;
    uint8_t     *evt_buf;
    uint16_t    opcode;
    int         pos;
    static FILE *fp;

    if(*(uint8_t *)(p_evt_buf + 1) == HCI_EVT_CMD_CMPL_EVT_CODE)
        status = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE);

    if(*(uint8_t *)(p_evt_buf + 1) == HCI_EVT_CMD_STAT_EVT_CODE)
        status = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_STAT_STATUS_RET_BYTE);

    evt_buf = (uint8_t *)(p_evt_buf + 1);
    p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE;
    STREAM_TO_UINT16(opcode,p);

    if(status != 0)
        ALOGE("FW Patch download aborted as command 0x%04X failed ", opcode);
    else if (bt_vendor_cbacks)
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + HCI_CMD_MAX_LEN);

    if (p_buf != NULL)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->len = 0;
        p_buf->layer_specific = 0;

        p = (uint8_t *) (p_buf + 1);

        switch (hw_cfg_cb.state)
        {
        case HW_CFG_INTEL_RDSW_VERSION:
#if (BLUETOOTH_HCI_USE_USB == TRUE)
            ALOGI("HW_CFG_INTEL_RDSW_VERSION");
            UINT16_TO_STREAM(p, HCI_INTEL_RDSW_VERSION);
            *p++ = 0;  /* parameter length */

            p_buf->len = HCI_CMD_PREAMBLE_SIZE;

            hw_cfg_cb.state = HW_CFG_INTEL_OPEN_PATCHFILE;

            is_proceeding = bt_vendor_cbacks->xmit_cb(HCI_INTEL_RDSW_VERSION,
                p_buf, hw_config_cback);

            break;
#endif

        case HW_CFG_INTEL_OPEN_PATCHFILE:
            ALOGI("OPEN_PATCHFILE");
            char patchfile[NAME_MAX];
            memset(patchfile, 0, sizeof(patchfile));
            snprintf(patchfile, NAME_MAX, "%02x%02x%02x%02x%02x%02x%02x%02x%02x.seq", evt_buf[6], evt_buf[7],
                evt_buf[8], evt_buf[9], evt_buf[10], evt_buf[11],
                evt_buf[12], evt_buf[13], evt_buf[14]);

            if (hw_config_findpatch(patchfile) == TRUE)
            {
                fp = fopen(patchfile, "r");

                if(fp == NULL) {
                    ALOGE("Can not open patch filename: %s", patchfile);
                    break;
                }
            }
            else
            {
                //Patch file not found, report fw download success
                ALOGD("Firmware is already updated");
                if (bt_vendor_cbacks)
                {
                    if (p_buf != NULL)
                        bt_vendor_cbacks->dealloc(p_buf);

                    bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);
                }
                is_proceeding = TRUE;
                break;
            }

            //continue with manufacture on

        case HW_CFG_INTEL_MANUFACTURE_ON:
            ALOGI("HW_CFG_INTEL_MANUFACTURE_ON");
            UINT16_TO_STREAM(p, HCI_INTEL_MANUFACTURE);
            *p++ = HCI_INTEL_MANUFACTURE_PARAM_SIZE; /* parameter length */
            *p++ = 0x01;
            *p = 0x00;

            p_buf->len = HCI_CMD_PREAMBLE_SIZE +
                HCI_INTEL_MANUFACTURE_PARAM_SIZE;
            hw_cfg_cb.state = HW_CFG_INTEL_MEMWRITE;

            is_proceeding = bt_vendor_cbacks->xmit_cb(HCI_INTEL_MANUFACTURE,
                p_buf, hw_config_cback);

            break;

        case HW_CFG_INTEL_MEMWRITE:
            {
                char line[LINE_LEN_MAX];
                memset(line, 0, sizeof(line));
                ALOGI("HW_CFG_INTEL_MEMWRITE");
                if(!feof(fp)) {
                    ALOGI("file not empty");
                    fgets(line, sizeof(line), fp);

                    while((line[0] == '*') || (line[0] == 0xd ) || (line[0] == 'F') || (line[1] == '2')){
                        if(feof(fp)) {
                            ALOGI("End of file");
                            if(fp != NULL)
                            {
                                fclose(fp);
                                fp = NULL;
                            }

                            if(fw_patchfile_empty != 0)
                            {
                                hw_cfg_cb.is_patch_enabled = 0x2;
                                hw_cfg_cb.next_state = HW_CFG_INTEL_RDSW_VERSION_RECHECK;
                                is_proceeding = hw_config_manufacture_mode_off(p_buf);
                            }
                            else
                            {
                                ALOGE("Patch file is empty");
                                is_proceeding = hw_config_manufacture_mode_off(p_buf);
                            }
                            break;
                        }

                        fgets(line, sizeof(line), fp);
                    }
                    if((line[0] == '0') && (line[1] == '1')){
                        int length = 0;
                        int parameter_length = 0;
                        uint8_t opcodeByte[2];
                        uint16_t opcode = 0;
                        opcodeByte[0] = form_byte(line[3], line[4]);
                        opcodeByte[1] = form_byte(line[5], line[6]);
                        opcode = form_word(opcodeByte[1], opcodeByte[0]);
                        length = strlen(line);

                        UINT16_TO_STREAM(p, opcode);
                        parameter_length = form_byte(line[8], line[9]);
                        *p = parameter_length;  /* parameter length */
                        p++;
                        p_buf->len = HCI_CMD_PREAMBLE_SIZE + parameter_length;
                        ALOGI("Length =%X, %d", parameter_length, length);

                        for(pos=0; pos < (length - 10); pos += 2){
                            *p = form_byte(line[10+pos], line[11+pos]);
                            p++;
                        }

                        fw_patchfile_empty = 1;
                        hw_cfg_cb.state = HW_CFG_INTEL_MEMWRITE;

                        is_proceeding = bt_vendor_cbacks->xmit_cb(opcode,
                            p_buf, hw_config_cback);
                    }
                    break;
                }
                else
                {
                    ALOGE("Patch file is empty");
                    is_proceeding = hw_config_manufacture_mode_off(p_buf);
                }
                break;
            }

        case HW_CFG_INTEL_RDSW_VERSION_RECHECK:
            ALOGI("HW_CFG_INTEL_RDSW_VERSION_RECHECK");
            UINT16_TO_STREAM(p, HCI_INTEL_RDSW_VERSION);
            *p++ = 0;  /* parameter length */

            p_buf->len = HCI_CMD_PREAMBLE_SIZE;
            hw_cfg_cb.state = HW_CFG_SUCCESS;

            is_proceeding = bt_vendor_cbacks->xmit_cb(HCI_INTEL_RDSW_VERSION,
                p_buf, hw_config_cback);

            break;

        case HW_CFG_SUCCESS:
            ALOGI("FIRMWARE INIT SUCCESS...");

            if(hw_cfg_cb.next_state == HW_CFG_INTEL_RDSW_VERSION_RECHECK)
            {
                ALOGI("HW/FW Version : %02x%02x%02x%02x%02x%02x%02x%02x%02x", evt_buf[6], evt_buf[7],
                    evt_buf[8], evt_buf[9], evt_buf[10], evt_buf[11],
                    evt_buf[12], evt_buf[13], evt_buf[14]);
            }

            //Report fw download success
            if (bt_vendor_cbacks)
            {
                if (p_buf != NULL)
                    bt_vendor_cbacks->dealloc(p_buf);

                bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);
            }
            is_proceeding = TRUE;
            break;

        case HW_CFG_FAIL:
            ALOGE("vendor lib fw conf aborted");
            //Report fw download failure
            if (bt_vendor_cbacks)
            {
                if (p_buf != NULL)
                    bt_vendor_cbacks->dealloc(p_buf);

                bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_FAIL);
            }
            is_proceeding = TRUE;
            break;

        default :
            break;

        } // switch(hw_cfg_cb.state)
    } // if (p_buf != NULL)

    /* Free the RX event buffer */
    if (bt_vendor_cbacks)
        bt_vendor_cbacks->dealloc(p_evt_buf);

    if (is_proceeding == FALSE)
    {
        ALOGE("vendor lib fwcfg aborted!!!");
        if (bt_vendor_cbacks)
        {
            if (p_buf != NULL)
                bt_vendor_cbacks->dealloc(p_buf);

            bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_FAIL);
        }

        if (fp != NULL)
        {
            fclose(fp);
            fp = NULL;
        }

        hw_cfg_cb.state = 0;
    }
}

#if (SW_RFKILL_CMD_SUPPORTED == TRUE)
/*******************************************************************************
**
** Function         hw_software_rf_kill_cback
**
** Description      Callback function for SW RF KILL Command
**
** Returns          None
**
*******************************************************************************/
void hw_software_rf_kill_cback(void *p_mem)
{
    HC_BT_HDR *p_evt_buf = (HC_BT_HDR *) p_mem;
    int16_t     status = -1;

    if(*(uint8_t *)(p_evt_buf + 1) == HCI_EVT_CMD_CMPL_EVT_CODE)
        status = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE);

    ALOGI("%s, status = %d",__func__,status);
    if (bt_vendor_cbacks)
        bt_vendor_cbacks->dealloc(p_evt_buf);
}

/*******************************************************************************
**
** Function        hw_config_send_sw_rf_kill_cmd
**
** Description     Send SW RF KILL Command to the Controller
**
** Returns         None
**
*******************************************************************************/
int hw_config_send_sw_rf_kill_cmd(void)
{
    HC_BT_HDR  *p_buf = NULL;
    uint8_t    *p;
    int16_t    result = -1;

    if (bt_vendor_cbacks)
    {
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE +
                                                       HCI_CMD_PREAMBLE_SIZE);
    }

    if (p_buf)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE;

        ALOGI("HW_CFG_INTEL_SW_RF_KILL");
        p = (uint8_t *) (p_buf + 1);
        UINT16_TO_STREAM(p, HCI_INTEL_SW_RF_KILL);
        *p = 0; /* parameter length */

        result = bt_vendor_cbacks->xmit_cb(HCI_INTEL_SW_RF_KILL, p_buf, hw_software_rf_kill_cback);
        if(result < 1)
            result = -1;
    }
    return result;
}
#endif

/******************************************************************************
**   LPM Static Functions
******************************************************************************/

/*******************************************************************************
**
** Function         hw_lpm_ctrl_cback
**
** Description      Callback function for lpm enable/disable rquest
**
** Returns          None
**
*******************************************************************************/
void hw_lpm_ctrl_cback(void *p_mem)
{
    HC_BT_HDR *p_evt_buf = (HC_BT_HDR *) p_mem;
    bt_vendor_op_result_t status = BT_VND_OP_RESULT_FAIL;

    if (*((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE) == 0)
    {
        status = BT_VND_OP_RESULT_SUCCESS;
    }

    if (bt_vendor_cbacks)
    {
        bt_vendor_cbacks->lpm_cb(status);
        bt_vendor_cbacks->dealloc(p_evt_buf);
    }
}


#if (SCO_CFG_INCLUDED == TRUE)
/*****************************************************************************
**   SCO Configuration Static Functions
*****************************************************************************/

/*******************************************************************************
**
** Function         hw_sco_cfg_cback
**
** Description      Callback function for SCO configuration rquest
**
** Returns          None
**
*******************************************************************************/
void hw_sco_cfg_cback(void *p_mem)
{
    HC_BT_HDR *p_evt_buf = (HC_BT_HDR *) p_mem;
    uint8_t     *p;
    uint16_t    opcode;
    HC_BT_HDR  *p_buf=NULL;

    p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE;
    STREAM_TO_UINT16(opcode,p);

    /* Free the RX event buffer */
    if (bt_vendor_cbacks)
        bt_vendor_cbacks->dealloc(p_evt_buf);

#if (!defined(SCO_USE_I2S_INTERFACE) || (SCO_USE_I2S_INTERFACE == FALSE))
    if (opcode == HCI_VSC_WRITE_SCO_PCM_INT_PARAM)
    {
        uint8_t ret = FALSE;

        /* Ask a new buffer to hold WRITE_PCM_DATA_FORMAT_PARAM command */
        if (bt_vendor_cbacks)
            p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE +
                                                HCI_CMD_PREAMBLE_SIZE +
                                                PCM_DATA_FORMAT_PARAM_SIZE);
        if (p_buf)
        {
            p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
            p_buf->offset = 0;
            p_buf->layer_specific = 0;
            p_buf->len = HCI_CMD_PREAMBLE_SIZE + PCM_DATA_FORMAT_PARAM_SIZE;

            p = (uint8_t *) (p_buf + 1);
            UINT16_TO_STREAM(p, HCI_VSC_WRITE_PCM_DATA_FORMAT_PARAM);
            *p++ = PCM_DATA_FORMAT_PARAM_SIZE;
            memcpy(p, &bt_pcm_data_fmt_param, PCM_DATA_FORMAT_PARAM_SIZE);

            if ((ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_WRITE_PCM_DATA_FORMAT_PARAM,
                                           p_buf, hw_sco_cfg_cback)) == FALSE)
            {
                bt_vendor_cbacks->dealloc(p_buf);
            }
            else
                return;
        }
    }
#endif  // !SCO_USE_I2S_INTERFACE

if (bt_vendor_cbacks)
    bt_vendor_cbacks->scocfg_cb(BT_VND_OP_RESULT_SUCCESS);
}
#endif // SCO_CFG_INCLUDED

/*****************************************************************************
**   Hardware Configuration Interface Functions
*****************************************************************************/


/*******************************************************************************
**
** Function        hw_config_start
**
** Description     Kick off controller initialization process
**
** Returns         None
**
*******************************************************************************/
void hw_config_start(void)
{
    HC_BT_HDR  *p_buf = NULL;
    uint8_t     *p;

    hw_cfg_cb.state = 0;
    hw_cfg_cb.is_patch_enabled = 0;         //Patch is not enabled
    hw_cfg_cb.next_state = HW_CFG_SUCCESS;

    /* As a workaround for the controller bug because of which controller is returning zero for number of completed command after sending the first HCI command,
    Start from sending HCI_RESET. this will reset the number of completed command */

    if (bt_vendor_cbacks)
    {
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE +
                                                       HCI_CMD_PREAMBLE_SIZE);
    }

    if (p_buf)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE;

        p = (uint8_t *) (p_buf + 1);
        UINT16_TO_STREAM(p, HCI_RESET);
        *p = 0; /* parameter length */

        hw_cfg_cb.state = HW_CFG_INTEL_RDSW_VERSION;

        bt_vendor_cbacks->xmit_cb(HCI_RESET, p_buf, hw_config_cback);
    }
    else
    {
        if (bt_vendor_cbacks)
        {
            ALOGE("vendor lib fw conf aborted [no buffer]");
            bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_FAIL);
        }
    }
}

/*******************************************************************************
**
** Function        hw_lpm_enable
**
** Description     Enalbe/Disable LPM
**
** Returns         TRUE/FALSE
**
*******************************************************************************/
uint8_t hw_lpm_enable(uint8_t turn_on)
{
    HC_BT_HDR  *p_buf = NULL;
    uint8_t     *p;
    uint8_t     ret = FALSE;

    if (bt_vendor_cbacks)
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE +
                                                       HCI_CMD_PREAMBLE_SIZE +
                                                       LPM_CMD_PARAM_SIZE);

    if (p_buf)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE + LPM_CMD_PARAM_SIZE;

        p = (uint8_t *) (p_buf + 1);
        UINT16_TO_STREAM(p, HCI_VSC_WRITE_SLEEP_MODE);
        *p++ = LPM_CMD_PARAM_SIZE; /* parameter length */

        if (turn_on)
        {
            memcpy(p, &lpm_param, LPM_CMD_PARAM_SIZE);
            upio_set(UPIO_LPM_MODE, UPIO_ASSERT, 0);
        }
        else
        {
            memset(p, 0, LPM_CMD_PARAM_SIZE);
            upio_set(UPIO_LPM_MODE, UPIO_DEASSERT, 0);
        }

        if ((ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_WRITE_SLEEP_MODE, p_buf,
                                        hw_lpm_ctrl_cback)) == FALSE)
        {
            bt_vendor_cbacks->dealloc(p_buf);
        }
    }

    if ((ret == FALSE) && bt_vendor_cbacks)
        bt_vendor_cbacks->lpm_cb(BT_VND_OP_RESULT_FAIL);

    return ret;
}

/*******************************************************************************
**
** Function        hw_lpm_get_idle_timeout
**
** Description     Calculate idle time based on host stack idle threshold
**
** Returns         idle timeout value
**
*******************************************************************************/
uint32_t hw_lpm_get_idle_timeout(void)
{
    uint32_t timeout_ms;

    /* set idle time to be LPM_IDLE_TIMEOUT_MULTIPLE times of
     * host stack idle threshold (in 300ms/25ms)
     */
    timeout_ms = (uint32_t)lpm_param.host_stack_idle_threshold
                            * LPM_IDLE_TIMEOUT_MULTIPLE;

    if (strstr(hw_cfg_cb.local_chip_name, "BCM4325") != NULL)
        timeout_ms *= 25; // 12.5 or 25 ?
    else
        timeout_ms *= 300;

    return timeout_ms;
}

/*******************************************************************************
**
** Function        hw_lpm_set_wake_state
**
** Description     Assert/Deassert BT_WAKE
**
** Returns         None
**
*******************************************************************************/
void hw_lpm_set_wake_state(uint8_t wake_assert)
{
    uint8_t state = (wake_assert) ? UPIO_ASSERT : UPIO_DEASSERT;

    upio_set(UPIO_BT_WAKE, state, lpm_param.bt_wake_polarity);
}

#if (SCO_CFG_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         hw_sco_config
**
** Description      Configure SCO related hardware settings
**
** Returns          None
**
*******************************************************************************/
void hw_sco_config(void)
{
    HC_BT_HDR  *p_buf = NULL;
    uint8_t     *p, ret;

#if (BLUETOOTH_HCI_USE_USB == TRUE)
    /* Nothing specific is required for SCO connection, return SUCCESS */
    if (bt_vendor_cbacks)
        bt_vendor_cbacks->scocfg_cb(BT_VND_OP_RESULT_SUCCESS);
    return;
#endif

#if (!defined(SCO_USE_I2S_INTERFACE) || (SCO_USE_I2S_INTERFACE == FALSE))
    uint16_t cmd_u16 = HCI_CMD_PREAMBLE_SIZE + SCO_PCM_PARAM_SIZE;
#else
    uint16_t cmd_u16 = HCI_CMD_PREAMBLE_SIZE + SCO_I2SPCM_PARAM_SIZE;
#endif

    if (bt_vendor_cbacks)
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE+cmd_u16);

    if (p_buf)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = cmd_u16;

        p = (uint8_t *) (p_buf + 1);
#if (!defined(SCO_USE_I2S_INTERFACE) || (SCO_USE_I2S_INTERFACE == FALSE))
        UINT16_TO_STREAM(p, HCI_VSC_WRITE_SCO_PCM_INT_PARAM);
        *p++ = SCO_PCM_PARAM_SIZE;
        memcpy(p, &bt_sco_param, SCO_PCM_PARAM_SIZE);
        cmd_u16 = HCI_VSC_WRITE_SCO_PCM_INT_PARAM;
        ALOGI("SCO PCM configure {%d, %d, %d, %d, %d}",
           bt_sco_param[0], bt_sco_param[1], bt_sco_param[2], bt_sco_param[3],
           bt_sco_param[4]);

#else
        UINT16_TO_STREAM(p, HCI_VSC_WRITE_I2SPCM_INTERFACE_PARAM);
        *p++ = SCO_I2SPCM_PARAM_SIZE;
        memcpy(p, &bt_sco_param, SCO_I2SPCM_PARAM_SIZE);
        cmd_u16 = HCI_VSC_WRITE_I2SPCM_INTERFACE_PARAM;
        ALOGI("SCO over I2SPCM interface {%d, %d, %d, %d}",
           bt_sco_param[0], bt_sco_param[1], bt_sco_param[2], bt_sco_param[3]);
#endif

        if ((ret=bt_vendor_cbacks->xmit_cb(cmd_u16, p_buf, hw_sco_cfg_cback))
             == FALSE)
        {
            bt_vendor_cbacks->dealloc(p_buf);
        }
        else
            return;
    }

    if (bt_vendor_cbacks)
    {
        ALOGE("vendor lib scocfg aborted");
        bt_vendor_cbacks->scocfg_cb(BT_VND_OP_RESULT_FAIL);
    }
}
#endif  // SCO_CFG_INCLUDED

/*******************************************************************************
**
** Function        hw_set_patch_file_path
**
** Description     Set the location of firmware patch file
**
** Returns         0 : Success
**                 Otherwise : Fail
**
*******************************************************************************/
int hw_set_patch_file_path(char *p_conf_name, char *p_conf_value, int param)
{

    strcpy(fw_patchfile_path, p_conf_value);

    return 0;
}

/*******************************************************************************
**
** Function        hw_set_patch_file_name
**
** Description     Give the specific firmware patch filename
**
** Returns         0 : Success
**                 Otherwise : Fail
**
*******************************************************************************/
int hw_set_patch_file_name(char *p_conf_name, char *p_conf_value, int param)
{

    strcpy(fw_patchfile_name, p_conf_value);

    return 0;
}

#if (VENDOR_LIB_RUNTIME_TUNING_ENABLED == TRUE)
/*******************************************************************************
**
** Function        hw_set_patch_settlement_delay
**
** Description     Give the specific firmware patch settlement time in milliseconds
**
** Returns         0 : Success
**                 Otherwise : Fail
**
*******************************************************************************/
int hw_set_patch_settlement_delay(char *p_conf_name, char *p_conf_value, int param)
{
    fw_patch_settlement_delay = atoi(p_conf_value);

    return 0;
}
#endif  //VENDOR_LIB_RUNTIME_TUNING_ENABLED

/*****************************************************************************
**   Sample Codes Section
*****************************************************************************/

#if (HW_END_WITH_HCI_RESET == TRUE)
/*******************************************************************************
**
** Function         hw_epilog_cback
**
** Description      Callback function for Command Complete Events from HCI
**                  commands sent in epilog process.
**
** Returns          None
**
*******************************************************************************/
void hw_epilog_cback(void *p_mem)
{
    HC_BT_HDR   *p_evt_buf = (HC_BT_HDR *) p_mem;
    uint8_t     *p, status;
    uint16_t    opcode;

    status = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE);
    p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE;
    STREAM_TO_UINT16(opcode,p);

    BTHWDBG("%s Opcode:0x%04X Status: %d", __FUNCTION__, opcode, status);

    if (bt_vendor_cbacks)
    {
        /* Must free the RX event buffer */
        bt_vendor_cbacks->dealloc(p_evt_buf);

        /* Once epilog process is done, must call epilog_cb callback
           to notify caller */
        bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
    }
}

/*******************************************************************************
**
** Function         hw_epilog_process
**
** Description      Sample implementation of epilog process
**
** Returns          None
**
*******************************************************************************/
void hw_epilog_process(void)
{
    HC_BT_HDR  *p_buf = NULL;
    uint8_t     *p;

    BTHWDBG("hw_epilog_process");

    /* Sending a HCI_RESET */
    if (bt_vendor_cbacks)
    {
        /* Must allocate command buffer via HC's alloc API */
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                       HCI_CMD_PREAMBLE_SIZE);
    }

    if (p_buf)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE;

        p = (uint8_t *) (p_buf + 1);
        UINT16_TO_STREAM(p, HCI_RESET);
        *p = 0; /* parameter length */

        /* Send command via HC's xmit_cb API */
        bt_vendor_cbacks->xmit_cb(HCI_RESET, p_buf, hw_epilog_cback);
    }
    else
    {
        if (bt_vendor_cbacks)
        {
            ALOGE("vendor lib epilog process aborted [no buffer]");
            bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_FAIL);
        }
    }
}
#endif // (HW_END_WITH_HCI_RESET == TRUE)
