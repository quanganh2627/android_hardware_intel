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
#include "CameraDriver.h"
#include <utils/Log.h>
#include <utils/threads.h>

using namespace android;


///////////////////////////////////////////////////////////////////////////////
//                              DATA TYPES
///////////////////////////////////////////////////////////////////////////////


struct camera_hal {
    int camera_id;
    sp<ControlThread> control_thread;
};


///////////////////////////////////////////////////////////////////////////////
//                              HAL MODULE PROTOTYPES
///////////////////////////////////////////////////////////////////////////////


static int CAMERA_OpenCameraHardware(const hw_module_t* module,
                                     const char* name,
                                     hw_device_t** device);
static int CAMERA_CloseCameraHardware(hw_device_t* device);
static int CAMERA_GetNumberOfCameras(void);
static int CAMERA_GetCameraInfo(int camera_id,
                                struct camera_info *info);


///////////////////////////////////////////////////////////////////////////////
//                              MODULE DATA
///////////////////////////////////////////////////////////////////////////////


static camera_hal camera_instance;
static int num_camera_instances = 0;
static Mutex camera_instance_lock; // for locking num_camera_instances only

static struct hw_module_methods_t camera_module_methods = {
    open: CAMERA_OpenCameraHardware
};

camera_module_t HAL_MODULE_INFO_SYM = {
    common: {
         tag: HARDWARE_MODULE_TAG,
         version_major: 1,
         version_minor: 0,
         id: CAMERA_HARDWARE_MODULE_ID,
         name: "Intel CameraHardware Module",
         author: "Intel",
         methods: &camera_module_methods,
         dso: NULL,
         reserved: {0},
    },
    get_number_of_cameras: CAMERA_GetNumberOfCameras,
    get_camera_info: CAMERA_GetCameraInfo,
};


///////////////////////////////////////////////////////////////////////////////
//                              HAL OPERATION FUNCTIONS
///////////////////////////////////////////////////////////////////////////////


static int camera_set_preview_window(struct camera_device * device,
        struct preview_stream_ops *window)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    camera_hal *cam = (camera_hal *)(device->priv);
    return cam->control_thread->setPreviewWindow(window);
}

static void camera_set_callbacks(struct camera_device * device,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return;
    camera_hal *cam = (camera_hal *)(device->priv);
    cam->control_thread->setCallbacks(notify_cb, data_cb, data_cb_timestamp, get_memory, user);
}

static void camera_enable_msg_type(struct camera_device * device, int32_t msg_type)
{
    ALOGD("%s msg_type=0x%08x", __FUNCTION__, msg_type);
    if(!device)
        return;
    camera_hal *cam = (camera_hal *)(device->priv);
    cam->control_thread->enableMsgType(msg_type);
}

static void camera_disable_msg_type(struct camera_device * device, int32_t msg_type)
{
    ALOGD("%s msg_type=0x%08x", __FUNCTION__, msg_type);
    if(!device)
        return;
    camera_hal *cam = (camera_hal *)(device->priv);
    cam->control_thread->disableMsgType(msg_type);
}

static int camera_msg_type_enabled(struct camera_device * device, int32_t msg_type)
{
    ALOGD("%s msg_type=0x%08x", __FUNCTION__, msg_type);
    if(!device)
        return 0;
    camera_hal *cam = (camera_hal *)(device->priv);
    return cam->control_thread->msgTypeEnabled(msg_type);
}

static int camera_start_preview(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    camera_hal *cam = (camera_hal *)(device->priv);
    return cam->control_thread->startPreview();
}

static void camera_stop_preview(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return;
    camera_hal *cam = (camera_hal *)(device->priv);
    cam->control_thread->stopPreview();
}

static int camera_preview_enabled(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    camera_hal *cam = (camera_hal *)(device->priv);
    return cam->control_thread->previewEnabled();
}
static int camera_store_meta_data_in_buffers(struct camera_device * device, int enable)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    camera_hal *cam = (camera_hal *)(device->priv);
    return cam->control_thread->storeMetaDataInVideoBuffers(enable);
}
static int camera_start_recording(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    camera_hal *cam = (camera_hal *)(device->priv);
    return cam->control_thread->startRecording();
}

static void camera_stop_recording(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return;
    camera_hal *cam = (camera_hal *)(device->priv);
    cam->control_thread->stopRecording();
}

static int camera_recording_enabled(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    camera_hal *cam = (camera_hal *)(device->priv);
    return cam->control_thread->recordingEnabled();
}

static void camera_release_recording_frame(struct camera_device * device,
               const void *opaque)
{
    ALOGV("%s", __FUNCTION__);
    if(!device)
        return;
    camera_hal *cam = (camera_hal *)(device->priv);
    cam->control_thread->releaseRecordingFrame(const_cast<void *>(opaque));
}

static int camera_auto_focus(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    camera_hal *cam = (camera_hal *)(device->priv);
    return cam->control_thread->autoFocus();
}

static int camera_cancel_auto_focus(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    camera_hal *cam = (camera_hal *)(device->priv);
    return cam->control_thread->cancelAutoFocus();
}

static int camera_take_picture(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    camera_hal *cam = (camera_hal *)(device->priv);
    return cam->control_thread->takePicture();
}

static int camera_cancel_picture(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    camera_hal *cam = (camera_hal *)(device->priv);
    return cam->control_thread->cancelPicture();
}

static int camera_set_parameters(struct camera_device * device, const char *params)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return -EINVAL;
    camera_hal *cam = (camera_hal *)(device->priv);
    return cam->control_thread->setParameters(params);
}

static char *camera_get_parameters(struct camera_device * device)
{
    char* params = NULL;
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return NULL;
    camera_hal *cam = (camera_hal *)(device->priv);
    params = cam->control_thread->getParameters();
    return params;
}

static void camera_put_parameters(struct camera_device *device, char *parms)
{
    ALOGD("%s", __FUNCTION__);
    if(!device)
        return;
    camera_hal *cam = (camera_hal *)(device->priv);
    cam->control_thread->putParameters(parms);
}

static int camera_send_command(struct camera_device * device,
            int32_t cmd, int32_t arg1, int32_t arg2)
{
    int return_val = -EINVAL;
    ALOGD("%s", __FUNCTION__);
    if (!device)
        return -EINVAL;
    camera_hal *cam = (camera_hal *)(device->priv);
    if (cam)
        return_val = cam->control_thread->sendCommand(cmd, arg1, arg2);
    return return_val;
}

static void camera_release(struct camera_device * device)
{
    ALOGD("%s", __FUNCTION__);
    // TODO: implement
}

static int camera_dump(struct camera_device * device, int fd)
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
static camera_device_ops_t camera_ops = {
    set_preview_window:         camera_set_preview_window,
    set_callbacks:              camera_set_callbacks,
    enable_msg_type:            camera_enable_msg_type,
    disable_msg_type:           camera_disable_msg_type,
    msg_type_enabled:           camera_msg_type_enabled,
    start_preview:              camera_start_preview,
    stop_preview:               camera_stop_preview,
    preview_enabled:            camera_preview_enabled,
    store_meta_data_in_buffers: camera_store_meta_data_in_buffers,
    start_recording:            camera_start_recording,
    stop_recording:             camera_stop_recording,
    recording_enabled:          camera_recording_enabled,
    release_recording_frame:    camera_release_recording_frame,
    auto_focus:                 camera_auto_focus,
    cancel_auto_focus:          camera_cancel_auto_focus,
    take_picture:               camera_take_picture,
    cancel_picture:             camera_cancel_picture,
    set_parameters:             camera_set_parameters,
    get_parameters:             camera_get_parameters,
    put_parameters:             camera_put_parameters,
    send_command:               camera_send_command,
    release:                    camera_release,
    dump:                       camera_dump,
};


///////////////////////////////////////////////////////////////////////////////
//                              HAL MODULE FUNCTIONS
///////////////////////////////////////////////////////////////////////////////


static int CAMERA_OpenCameraHardware(const hw_module_t* module, const char* name,
                hw_device_t** device)
{
    ALOGD("%s", __FUNCTION__);

    Mutex::Autolock _l(camera_instance_lock);

    camera_device_t *camera_dev;

    if (num_camera_instances > 0) {
        ALOGE("error: we only support a single instance");
        return -EINVAL;
    }

    camera_instance.camera_id = atoi(name);
    camera_instance.control_thread = new ControlThread(camera_instance.camera_id);
    if (camera_instance.control_thread == NULL) {
        ALOGE("Memory allocation error!");
        return NO_MEMORY;
    }
    camera_instance.control_thread->run();

    camera_dev = (camera_device_t*)malloc(sizeof(*camera_dev));
    if(camera_dev == NULL)
    {
        ALOGE("Memory allocation error!");
        return NO_MEMORY;
    }
    memset(camera_dev, 0, sizeof(*camera_dev));
    camera_dev->common.tag = HARDWARE_DEVICE_TAG;
    camera_dev->common.version = 0;
    camera_dev->common.module = (hw_module_t *)(module);
    camera_dev->common.close = CAMERA_CloseCameraHardware;
    camera_dev->ops = &camera_ops;
    camera_dev->priv = &camera_instance;

    *device = &camera_dev->common;

    num_camera_instances++;
    return 0;
}

static int CAMERA_CloseCameraHardware(hw_device_t* device)
{
    ALOGD("%s", __FUNCTION__);

    Mutex::Autolock _l(camera_instance_lock);

    if (!device)
        return -EINVAL;

    camera_device_t *camera_dev = (camera_device_t *)device;
    camera_hal *cam = (camera_hal *)(camera_dev->priv);
    cam->control_thread->requestExitAndWait();
    cam->control_thread.clear();

    free(camera_dev);

    num_camera_instances--;
    return 0;
}

static int CAMERA_GetNumberOfCameras(void)
{
    ALOGD("%s", __FUNCTION__);
    return CameraDriver::getNumberOfCameras();
}

static int CAMERA_GetCameraInfo(int camera_id, struct camera_info *info)
{
    ALOGD("%s", __FUNCTION__);
    return CameraDriver::getCameraInfo(camera_id, info);
}
