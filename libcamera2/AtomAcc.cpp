/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (c) 2012-2014 Intel Corporation
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
 */

#define LOG_TAG "Camera_AtomAcc"

#include "AtomAcc.h"
#include "ICameraHwControls.h"
#include <stdio.h>

namespace android {

extern "C" {

static int get_file_size (FILE *file)
{
    int len;

    if (file == NULL) return 0;

    if (fseek(file, 0, SEEK_END)) return 0;

    len = ftell(file);

    if (fseek(file, 0, SEEK_SET)) return 0;

    return len;
}

void *open_firmware(const char *fw_path, unsigned *size)
{
    FILE *file;
    unsigned len;
    void *fw;

    LOG1("@%s", __FUNCTION__);
    if (!fw_path)
        return NULL;

    file = fopen(fw_path, "rb");
    if (!file)
        return NULL;

    len = get_file_size(file);

    if (!len) {
        fclose(file);
        return NULL;
    }

    fw = malloc(len);
    if (!fw) {
        fclose(file);
        return NULL;
    }

    if (fread(fw, 1, len, file) != len) {
        fclose(file);
        free(fw);
        return NULL;
    }

    *size = len;

    fclose(file);

    return fw;
}

int load_firmware(void *isp, void *fw, unsigned size, unsigned *handle)
{
    IHWIspControl *ISP = (IHWIspControl*)isp;
    LOG1("@%s", __FUNCTION__);
    return ISP->loadAccFirmware(fw, size, handle);
}

int load_firmware_ext(void *isp, void *fw, unsigned size, unsigned *handle, int dst)
{
    IHWIspControl *ISP = (IHWIspControl*)isp;
    LOG1("@%s", __FUNCTION__);
    return ISP->loadAccPipeFirmware(fw, size, handle, dst);
}

int load_firmware_pipe(void *isp, void *fw, unsigned size, unsigned *handle)
{
    IHWIspControl *ISP = (IHWIspControl*)isp;
    LOG1("@%s", __FUNCTION__);
    return ISP->loadAccPipeFirmware(fw, size, handle);
}

int unload_firmware(void *isp, unsigned handle)
{
    IHWIspControl *ISP = (IHWIspControl*)isp;
    LOG1("@%s", __FUNCTION__);
    return ISP->unloadAccFirmware(handle);
}

int map_firmware_arg (void *isp, void *val, size_t size, unsigned long *ptr)
{
    IHWIspControl *ISP = (IHWIspControl*)isp;
    LOG1("@%s", __FUNCTION__);
    return ISP->mapFirmwareArgument(val, size, ptr);
}

int unmap_firmware_arg (void *isp, unsigned long val, size_t size)
{
    IHWIspControl *ISP = (IHWIspControl*)isp;
    LOG1("@%s", __FUNCTION__);
    return ISP->unmapFirmwareArgument(val, size);
}

int set_firmware_arg(void *isp, unsigned handle, unsigned num, void *val, size_t size)
{
    IHWIspControl *ISP = (IHWIspControl*)isp;
    LOG1("@%s", __FUNCTION__);
    return ISP->setFirmwareArgument(handle, num, val, size);
}

int set_mapped_arg(void *isp, unsigned handle, unsigned mem, unsigned long val, size_t size)
{
    IHWIspControl *ISP = (IHWIspControl*)isp;
    LOG1("@%s", __FUNCTION__);
    return ISP->setMappedFirmwareArgument(handle, mem, val, size);
}

int start_firmware(void *isp, unsigned handle)
{
    IHWIspControl *ISP = (IHWIspControl*)isp;
    LOG1("@%s", __FUNCTION__);
    return ISP->startFirmware(handle);
}

int wait_for_firmware(void *isp, unsigned handle)
{
    IHWIspControl *ISP = (IHWIspControl*)isp;
    LOG1("@%s", __FUNCTION__);
    return ISP->waitForFirmware(handle);
}

int abort_firmware(void *isp, unsigned handle, unsigned timeout)
{
    IHWIspControl *ISP = (IHWIspControl*)isp;
    LOG1("@%s", __FUNCTION__);
    return ISP->abortFirmware(handle, timeout);
}

int set_stage_state(void *isp, unsigned int handle, bool enable)
{
    IHWIspControl *ISP = (IHWIspControl*)isp;
    LOG1("@%s", __FUNCTION__);
    return ISP->setStageState(handle, enable);
}

int wait_stage_update(void *isp, unsigned int handle)
{
    IHWIspControl *ISP = (IHWIspControl*)isp;
    LOG2("@%s", __FUNCTION__);
    return ISP->waitStageUpdate(handle);
}

} // extern "C"

} // namespace android
