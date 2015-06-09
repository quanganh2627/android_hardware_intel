/*
 * Copyright (C) 2011 The Android Open Source Project
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
#define LOG_TAG "Camera_HAL"

#include "ControlThread.h"
#include "LogHelper.h"
#include "CameraConf.h"
#include "PerformanceTraces.h"
#include <utils/Log.h>
#include <utils/threads.h>
#include "PlatformData.h"

using namespace android;

#define MAX_HAL_INSTANCES 2

///////////////////////////////////////////////////////////////////////////////
//                              DATA TYPES
///////////////////////////////////////////////////////////////////////////////

struct atom_camera {
    int camera_id;
    sp<ControlThread> control_thread;
    bool is_used;
};


///////////////////////////////////////////////////////////////////////////////
//                              HAL MODULE PROTOTYPES
///////////////////////////////////////////////////////////////////////////////


static int ATOM_OpenCameraHardware(const hw_module_t* module,
                                   const char* name,
                                   hw_device_t** device);
static int ATOM_CloseCameraHardware(hw_device_t* device);
static int ATOM_GetNumberOfCameras(void);
static int ATOM_GetCameraInfo(int camera_id,
                              struct camera_info *info);


///////////////////////////////////////////////////////////////////////////////
//                              MODULE DATA
///////////////////////////////////////////////////////////////////////////////


static atom_camera atom_cam[MAX_HAL_INSTANCES] = {{-1, NULL, false}, {-1, NULL, false}};
static int atom_instances = 0;
static Mutex atom_instance_lock; // for locking atom_instances only

static struct hw_module_methods_t atom_module_methods = {
    open: ATOM_OpenCameraHardware
};

camera_module_t HAL_MODULE_INFO_SYM = {
    common: {
         tag: HARDWARE_MODULE_TAG,
         version_major: 1,
         version_minor: 0,
         id: CAMERA_HARDWARE_MODULE_ID,
         name: "Intel CameraHardware Module",
         author: "Intel",
         methods: &atom_module_methods,
         dso: NULL,
         reserved: {0},
    },
    get_number_of_cameras: ATOM_GetNumberOfCameras,
    get_camera_info: ATOM_GetCameraInfo,
    set_callbacks: NULL, /* FYI: CAMERA_MODULE_API_VERSION_1_0, CAMERA_MODULE_API_VERSION_2_0: Not provided by HAL module. Framework may not call this function.not used with HAL v1 */
    get_vendor_tag_ops: NULL,
    open_legacy: NULL,
    reserved: {0}
};

///////////////////////////////////////////////////////////////////////////////
//                              LOCAL FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

int openCameraHardware(int cameraId)
{
    atom_cam[cameraId].camera_id = cameraId;
    atom_cam[cameraId].control_thread = new ControlThread(cameraId);
    if (atom_cam[cameraId].control_thread == NULL) {
        ALOGE("Memory allocation error!");
        return NO_MEMORY;
    }
    int status = atom_cam[cameraId].control_thread->init();
    if (status != NO_ERROR) {
        ALOGE("Error initializing ControlThread");
        atom_cam[cameraId].control_thread.clear();
        return status;
    }
    atom_cam[cameraId].control_thread->run("CamHAL_CTRL");
    atom_cam[cameraId].is_used = true;
    return status;
}

///////////////////////////////////////////////////////////////////////////////
//                              HAL OPERATION FUNCTIONS
///////////////////////////////////////////////////////////////////////////////


static int atom_set_preview_window(struct camera_device * device,
        struct preview_stream_ops *window)
{
    ALOGD("%s preview_window = %p", __FUNCTION__, window);
    if(!device)
        return -EINVAL;
    atom_camera *cam = (atom_camera *)(device->priv);
    return cam->control_thread->setPreviewWindow(window);
}

static void atom_set_callbacks(struct camera_device * device,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return;
    atom_camera *cam = (atom_camera *)(device->priv);
    cam->control_thread->setCallbacks(notify_cb, data_cb, data_cb_timestamp, get_memory, user);
}

static void atom_enable_msg_type(struct camera_device * device, int32_t msg_type)
{
    LOG1("%s msg_type=0x%08x", __FUNCTION__, msg_type);
    if(!device)
        return;
    atom_camera *cam = (atom_camera *)(device->priv);
    cam->control_thread->enableMsgType(msg_type);
}

static void atom_disable_msg_type(struct camera_device * device, int32_t msg_type)
{
    LOG1("%s msg_type=0x%08x", __FUNCTION__, msg_type);
    if(!device)
        return;
    atom_camera *cam = (atom_camera *)(device->priv);
    cam->control_thread->disableMsgType(msg_type);
}

static int atom_msg_type_enabled(struct camera_device * device, int32_t msg_type)
{
    LOG1("%s msg_type=0x%08x", __FUNCTION__, msg_type);
    if(!device)
        return 0;
    atom_camera *cam = (atom_camera *)(device->priv);
    return cam->control_thread->msgTypeEnabled(msg_type);
}

static int atom_start_preview(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    atom_camera *cam = (atom_camera *)(device->priv);
    return cam->control_thread->startPreview();
}

static void atom_stop_preview(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return;
    atom_camera *cam = (atom_camera *)(device->priv);
    PerformanceTraces::SwitchCameras::start(cam->camera_id);
    cam->control_thread->stopPreview();
}

static int atom_preview_enabled(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    atom_camera *cam = (atom_camera *)(device->priv);
    return cam->control_thread->previewEnabled();
}

static int atom_store_meta_data_in_buffers(struct camera_device * device, int32_t enabled)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    atom_camera *cam = (atom_camera *)(device->priv);
    return cam->control_thread->storeMetaDataInBuffers(enabled);
}

static int atom_start_recording(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    atom_camera *cam = (atom_camera *)(device->priv);
    return cam->control_thread->startRecording();
}

static void atom_stop_recording(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return;
    atom_camera *cam = (atom_camera *)(device->priv);
    cam->control_thread->stopRecording();
}

static int atom_recording_enabled(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    atom_camera *cam = (atom_camera *)(device->priv);
    return cam->control_thread->recordingEnabled();
}

static void atom_release_recording_frame(struct camera_device * device,
               const void *opaque)
{
    LOGV("%s", __FUNCTION__);
    if(!device)
        return;
    atom_camera *cam = (atom_camera *)(device->priv);
    cam->control_thread->releaseRecordingFrame(const_cast<void *>(opaque));
}

static int atom_auto_focus(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    atom_camera *cam = (atom_camera *)(device->priv);
    return cam->control_thread->autoFocus();
}

static int atom_cancel_auto_focus(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    atom_camera *cam = (atom_camera *)(device->priv);
    return cam->control_thread->cancelAutoFocus();
}

static int atom_take_picture(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    PerformanceTraces::HDRShot2Preview::start();
    atom_camera *cam = (atom_camera *)(device->priv);
    return cam->control_thread->takePicture();
}

static int atom_cancel_picture(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    atom_camera *cam = (atom_camera *)(device->priv);
    return cam->control_thread->cancelPicture();
}

static int atom_set_parameters(struct camera_device * device, const char *params)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    atom_camera *cam = (atom_camera *)(device->priv);
    return cam->control_thread->setParameters(params);
}

static char *atom_get_parameters(struct camera_device * device)
{
    char* params = NULL;
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return NULL;
    atom_camera *cam = (atom_camera *)(device->priv);
    params = cam->control_thread->getParameters();
    return params;
}

static void atom_put_parameters(struct camera_device *device, char *parms)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return;
    atom_camera *cam = (atom_camera *)(device->priv);
    cam->control_thread->putParameters(parms);
}

static int atom_send_command(struct camera_device * device,
            int32_t cmd, int32_t arg1, int32_t arg2)
{
    ALOGD("%s", __FUNCTION__);
    if (!device)
        return -EINVAL;
    atom_camera *cam = (atom_camera *)(device->priv);
    if (cam)
        cam->control_thread->sendCommand(cmd, arg1, arg2);
    return 0;
}

static void atom_release(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    atom_camera *cam = (atom_camera *)(device->priv);
    if (cam)
        cam->control_thread->atomRelease();
}

static int atom_dump(struct camera_device * device, int fd)
{
    ALOGD("%s", __FUNCTION__);
    // TODO: implement
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
//                              HAL OPERATIONS TABLE
///////////////////////////////////////////////////////////////////////////////


//
// For camera_device_ops_t interface documentation refer to: hardware/camera.h
//
static camera_device_ops_t atom_ops = {
    set_preview_window:         atom_set_preview_window,
    set_callbacks:              atom_set_callbacks,
    enable_msg_type:            atom_enable_msg_type,
    disable_msg_type:           atom_disable_msg_type,
    msg_type_enabled:           atom_msg_type_enabled,
    start_preview:              atom_start_preview,
    stop_preview:               atom_stop_preview,
    preview_enabled:            atom_preview_enabled,
    store_meta_data_in_buffers: atom_store_meta_data_in_buffers,
    start_recording:            atom_start_recording,
    stop_recording:             atom_stop_recording,
    recording_enabled:          atom_recording_enabled,
    release_recording_frame:    atom_release_recording_frame,
    auto_focus:                 atom_auto_focus,
    cancel_auto_focus:          atom_cancel_auto_focus,
    take_picture:               atom_take_picture,
    cancel_picture:             atom_cancel_picture,
    set_parameters:             atom_set_parameters,
    get_parameters:             atom_get_parameters,
    put_parameters:             atom_put_parameters,
    send_command:               atom_send_command,
    release:                    atom_release,
    dump:                       atom_dump,
};


///////////////////////////////////////////////////////////////////////////////
//                              HAL MODULE FUNCTIONS
///////////////////////////////////////////////////////////////////////////////


static int ATOM_OpenCameraHardware(const hw_module_t* module, const char* name,
                hw_device_t** device)
{
    ALOGD("%s", __FUNCTION__);

    // debug/trace setup (done as early as possible, can be called
    // without taking the instance lock
    LogHelper::setDebugLevel();

    PERFORMANCE_TRACES_LAUNCH_START();

    Mutex::Autolock _l(atom_instance_lock);

    camera_device_t *camera_dev;
    if ((!PlatformData::supportDualVideo() && atom_instances == 1) || atom_instances > MAX_HAL_INSTANCES-1) {
        ALOGE("error:only support maximum  %d instances for front/primary sensor", atom_instances);
        // for camera cts2 test, can't open 2 cameras simultaneously.
        return -EUSERS;
    }

    int cameraId = atoi(name);
    if(cameraId < 0 || cameraId > 1 || atom_cam[cameraId].is_used)
        return -EINVAL;
    atom_cam[cameraId].camera_id = cameraId;
    CpfStore cpf(cameraId);
    PlatformData::AiqConfig = cpf.AiqConfig;
    PlatformData::HalConfig = cpf.HalConfig;

    int status = openCameraHardware(cameraId);
    if (status != NO_ERROR) {
        ALOGE("Error initializing ControlThread");
        return status;
    }

    camera_dev = (camera_device_t*)malloc(sizeof(*camera_dev));
    if (camera_dev == NULL) {
        ALOGE("Memory allocation error!");
        return NO_MEMORY;
    }
    memset(camera_dev, 0, sizeof(*camera_dev));
    camera_dev->common.tag = HARDWARE_DEVICE_TAG;
    camera_dev->common.version = 0;
    camera_dev->common.module = (hw_module_t *)(module);
    camera_dev->common.close = ATOM_CloseCameraHardware;
    camera_dev->ops = &atom_ops;
    camera_dev->priv = &atom_cam[cameraId];

    *device = &camera_dev->common;

    atom_instances++;
    PERFORMANCE_TRACES_BREAKDOWN_STEP("Open_HAL_Done");
    return 0;
}

static int ATOM_CloseCameraHardware(hw_device_t* device)
{
    ALOGD("%s", __FUNCTION__);

    Mutex::Autolock _l(atom_instance_lock);

    if (!device)
        return -EINVAL;

    camera_device_t *camera_dev = (camera_device_t *)device;
    atom_camera *cam = (atom_camera *)(camera_dev->priv);
    cam->control_thread->requestExitAndWait();
    cam->control_thread->deinit();
    cam->control_thread.clear();
    cam->is_used = false;

    free(camera_dev);

    PERFORMANCE_TRACES_BREAKDOWN_STEP("Close_HAL_Done");
    atom_instances--;
    return 0;
}

static int ATOM_GetNumberOfCameras(void)
{
    ALOGD("%s", __FUNCTION__);
    int nodes = PlatformData::numberOfCameras();
    if (nodes > MAX_CAMERAS)
        nodes = MAX_CAMERAS;

    return nodes;
}

static int ATOM_GetCameraInfo(int camera_id, struct camera_info *info)
{
    ALOGD("%s", __FUNCTION__);
    if (camera_id >= PlatformData::numberOfCameras())
        return BAD_VALUE;

    info->facing = PlatformData::cameraFacing(camera_id);
    info->orientation = PlatformData::cameraOrientation(camera_id);

    LOG1("@%s: %d: facing %s, orientation %d",
         __FUNCTION__,
         camera_id,
         ((info->facing == CAMERA_FACING_BACK) ?
          "back" : "front/other"),
         info->orientation);

    return NO_ERROR;
}
