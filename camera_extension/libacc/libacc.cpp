#define LOG_TAG "Camera_Acc"

#include "libacc.h"
#include <camera/Camera.h>
#include <camera/CameraParameters.h>

#include <utils/threads.h>

//Intel camera extensions
#include <intel_camera_extensions.h>

// should be in sync with AccManager.cpp
const unsigned int MAX_NUMBER_ARGUMENT_BUFFERS = 50;
enum {
    STANDALONE_START = 1,
    STANDALONE_WAIT = 2,
    STANDALONE_ABORT = 3,
    STANDALONE_UNLOAD = 4
};

#define LOG1 if(0) ALOGD
#define LOG2 if(0) ALOGD

CameraAcc::CameraAcc(sp<Camera> cam)   :
    Thread(true) // callbacks may call into java
    ,mMessageQueue("libAcc", (int) MESSAGE_ID_MAX)
    ,mThreadRunning(false)
    ,mCamera(cam)
    ,mStandaloneMode(false)
    ,mCallback(NULL)
{
    LOG1("@%s", __FUNCTION__);

    if (cam == NULL)
    {
        ALOGE("Could not connect to camera service!");
        return;
    }

    // set up list of argument buffers
    mArgumentBuffers.setCapacity(MAX_NUMBER_ARGUMENT_BUFFERS);

    // start Thread
    this->run("CameraAcc");
}

CameraAcc::~CameraAcc()
{
    LOG1("@%s", __FUNCTION__);

    requestExitAndWait();
}

status_t CameraAcc::acc_read_fw(const char* filename, fw_info &fw)
{
    LOG1("@%s", __FUNCTION__);

    FILE* file;
    if (!filename)
        return UNKNOWN_ERROR;

    LOG1("filename=%s", filename);
    fw.size = 0;
    fw.data = NULL;

    file = fopen(filename, "rb");
    if (!file)
        return UNKNOWN_ERROR;

    fseek(file, 0, SEEK_END);
    fw.size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (fw.size == 0) {
        fclose(file);
        return UNKNOWN_ERROR;
    }

    fw.data = host_alloc(fw.size);
    if (fw.data == NULL) {
        fclose(file);
        return UNKNOWN_ERROR;
    }

    if (fread(fw.data, 1, fw.size, file) != fw.size) {
        fclose(file);
        host_free(fw.data);
        return UNKNOWN_ERROR;
    }

    fclose(file);

    return NO_ERROR;
}

status_t CameraAcc::acc_upload_fw_standalone(fw_info &fw)
{
    LOG1("@%s", __FUNCTION__);

    return acc_upload_fw(fw);
}

status_t CameraAcc::acc_upload_fw_extension(fw_info &fw)
{
    LOG1("@%s", __FUNCTION__);

    mCamera->sendCommand(CAMERA_CMD_ENABLE_ISP_EXTENSION, 0, 0);

    return acc_upload_fw(fw);
}

status_t CameraAcc::acc_upload_fw(fw_info &fw)
{
    LOG1("@%s", __FUNCTION__);

    int idx = -1;
    for (unsigned int i = 0; i < mArgumentBuffers.size(); i++) {
        if (mArgumentBuffers[i].mem->base() == fw.data) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        ALOGE("Firmware data not in buffer allocated by us!");
        return UNKNOWN_ERROR;
    }

    return mCamera->sendCommand(CAMERA_CMD_ACC_LOAD, idx, 0);
}

status_t CameraAcc::acc_start_standalone()
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;

    status = mCamera->sendCommand(CAMERA_CMD_ACC_CONFIGURE_ISP_STANDALONE, STANDALONE_START, 0);

    if (status == NO_ERROR)
        mStandaloneMode = true;

    return status;
}

status_t CameraAcc::acc_wait_standalone()
{
    LOG1("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_WAIT_STANDALONE;
    return mMessageQueue.send(&msg, MESSAGE_ID_WAIT_STANDALONE);
}

status_t CameraAcc::handleMessageWaitStandalone()
{
    LOG1("@%s", __FUNCTION__);

    return mCamera->sendCommand(CAMERA_CMD_ACC_CONFIGURE_ISP_STANDALONE, STANDALONE_WAIT, 0);
}

status_t CameraAcc::acc_abort_standalone()
{
    LOG1("@%s", __FUNCTION__);

    return mCamera->sendCommand(CAMERA_CMD_ACC_CONFIGURE_ISP_STANDALONE, STANDALONE_ABORT, 0);
}

status_t CameraAcc::acc_unload_standalone()
{
    LOG1("@%s", __FUNCTION__);

    if (mStandaloneMode == false) {
        ALOGW("Not in standalone mode!");
        return NO_ERROR;
    }

    return mCamera->sendCommand(CAMERA_CMD_ACC_CONFIGURE_ISP_STANDALONE, STANDALONE_UNLOAD, 0);
}

void* CameraAcc::host_alloc(int size)
{
    LOG1("@%s", __FUNCTION__);

    if (mArgumentBuffers.size() >= MAX_NUMBER_ARGUMENT_BUFFERS) {
        ALOGE("Cannot allocate more buffers!");
        return NULL;
    }

    Message msg;
    msg.id = MESSAGE_ID_HOST_ALLOC;
    msg.data.alloc.size = size;
    mMessageQueue.send(&msg, MESSAGE_ID_HOST_ALLOC);

    // postData has been called

    int idx = mArgumentBuffers.size()-1;
    LOG2("host_alloc: Returning buffer pointer %d", idx);
    return mArgumentBuffers[idx].mem->base();
}

status_t CameraAcc::handleMessageHostAlloc(const MessageAlloc& msg)
{
    LOG1("@%s", __FUNCTION__);

    return mCamera->sendCommand(CAMERA_CMD_ACC_ALLOC, msg.size, 0);
}

status_t CameraAcc::host_free(host_ptr ptr)
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;

    int idx = -1;
    for (unsigned int i = 0; i < mArgumentBuffers.size(); i++) {
        if (mArgumentBuffers[i].mem->base() == ptr) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        ALOGE("This buffer has not been allocated by us!");
        return UNKNOWN_ERROR;
    }

    status = mCamera->sendCommand(CAMERA_CMD_ACC_FREE, idx, 0);

    if (status == NO_ERROR)
        mArgumentBuffers.removeAt(idx);

    return status;
}

status_t CameraAcc::acc_map(host_ptr in, isp_ptr &out)
{
    LOG1("@%s", __FUNCTION__);

    int idx = -1;
    for (unsigned int i = 0; i < mArgumentBuffers.size(); i++) {
        if (mArgumentBuffers[i].mem->base() == in) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        ALOGE("This buffer has not been allocated by us!");
        return UNKNOWN_ERROR;
    }

    Message msg;
    msg.id = MESSAGE_ID_MAP;
    msg.data.map.idx = idx;
    mMessageQueue.send(&msg, MESSAGE_ID_MAP);

    LOG2("acc_map: Returning buffer pointer %d", idx);
    out = mArgumentBuffers[idx].ptr;

    return NO_ERROR;
}

status_t CameraAcc::handleMessageMap(const MessageMap& msg)
{
    LOG1("@%s", __FUNCTION__);

    return mCamera->sendCommand(CAMERA_CMD_ACC_MAP, msg.idx, 0);
}

status_t CameraAcc::acc_sendarg(isp_ptr arg)
{
    LOG1("@%s", __FUNCTION__);

    int idx = -1;
    for (unsigned int i = 0; i < mArgumentBuffers.size(); i++) {
        if (mArgumentBuffers[i].ptr == arg) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        ALOGE("This buffer has not been mapped!");
        return UNKNOWN_ERROR;
    }

    mCamera->sendCommand(CAMERA_CMD_ACC_SEND_ARG, idx, 0);

    return NO_ERROR;
}

status_t CameraAcc::acc_unmap(isp_ptr p)
{
    LOG1("@%s", __FUNCTION__);

    int idx = -1;
    for (unsigned int i = 0; i < mArgumentBuffers.size(); i++) {
        if (mArgumentBuffers[i].ptr == p) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        ALOGW("This buffer has not been mapped!");
        return NO_ERROR;
    }

    mCamera->sendCommand(CAMERA_CMD_ACC_UNMAP, idx, 0);

    mArgumentBuffers.editItemAt(idx).ptr = NULL;

    return NO_ERROR;
}

status_t CameraAcc::register_callback(preview_callback cb)
{
    LOG1("@%s", __FUNCTION__);

    if (cb == NULL)
        return UNKNOWN_ERROR;

    mCallback = cb;

    return NO_ERROR;
}

bool CameraAcc::threadLoop()
{
    LOG2("@%s", __FUNCTION__);

    mThreadRunning = true;
    while(mThreadRunning)
        waitForAndExecuteMessage();

    return false;
}

status_t CameraAcc::waitForAndExecuteMessage()
{
    LOG2("@%s", __FUNCTION__);

    status_t status = NO_ERROR;
    Message msg;
    mMessageQueue.receive(&msg);

    switch (msg.id) {
        case MESSAGE_ID_EXIT:
            status = handleExit();
            break;
        case MESSAGE_ID_HOST_ALLOC:
            status = handleMessageHostAlloc(msg.data.alloc);
            break;
        case MESSAGE_ID_MAP:
            status = handleMessageMap(msg.data.map);
            break;
        case MESSAGE_ID_WAIT_STANDALONE:
            status = handleMessageWaitStandalone();
            break;
        default:
            status = INVALID_OPERATION;
            break;
    }
    if (status != NO_ERROR) {
        ALOGE("operation failed, ID = %d, status = %d", msg.id, status);
    }
    return status;
}

status_t CameraAcc::requestExitAndWait()
{
    LOG1("@%s", __FUNCTION__);

    Message msg;
    msg.id = MESSAGE_ID_EXIT;
    // tell thread to exit
    // send message asynchronously
    mMessageQueue.send(&msg);

    // propagate call to base class
    return Thread::requestExitAndWait();
}

status_t CameraAcc::handleExit()
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;

    mThreadRunning = false;
    return status;
}

// ########## Callbacks ##########

/**
 * This Callback is used by HAL to return the mapped pointer
 *
 * After we stored the Pointer, we can unblock the calling function acc_map
 **/
void CameraAcc::notifyPointer(int32_t data, int32_t idx)
{
    LOG1("@%s, data=%x, idx=%d", __FUNCTION__, data, idx);

    mArgumentBuffers.editItemAt(idx).ptr = (void*)data;

    // Unblock acc_map()
    mMessageQueue.reply(MESSAGE_ID_MAP, NO_ERROR);
}

/**
 * This Callback is used by HAL for notification when ISP has finished
 *
 * We can unblock the calling function acc_wait_standalone
 **/
void CameraAcc::notifyFinished()
{
    LOG1("@%s", __FUNCTION__);

    // Unblock acc_wait_standalone()
    mMessageQueue.reply(MESSAGE_ID_WAIT_STANDALONE, NO_ERROR);
}

/**
 * This Callback is used by HAL to return the pointer to the allocated buffer
 *
 * After we stored the Pointer, we can unblock the calling function host_alloc
 **/
void CameraAcc::postArgumentBuffer(sp<IMemoryHeap> heap, uint8_t *heapBase, size_t size, ssize_t offset)
{
    LOG1("@%s, heapBase=%p, size=%d", __FUNCTION__, heapBase, size);

    ArgumentBuffer tmp;
    tmp.mem = heap;
    tmp.ptr = NULL;
    int idx = mArgumentBuffers.add(tmp);

    // unblock host_alloc()
    mMessageQueue.reply(MESSAGE_ID_HOST_ALLOC, NO_ERROR);
}

/**
 * This Callback is used by HAL to return the pointer to a preview frame.
 *
 * After we have created the Frame struct, we can call the user-defined callback
 **/
void CameraAcc::postPreviewBuffer(sp<IMemoryHeap> heap, uint8_t *heapBase, size_t size, ssize_t offset)
{
    LOG2("@%s, heapBase=%p, size=%d", __FUNCTION__, heapBase, size);

    int frameCounter = mFrameMetadata->frameCounter;

    if (mCallback != NULL)
    {
        // Create Frame struct
        Frame f;
        f.img_data = heap->base();
        f.id = mFrameMetadata->id;
        f.frameCounter = mFrameMetadata->frameCounter;
        f.width = mFrameMetadata->width;
        f.height = mFrameMetadata->height;
        f.format = mFrameMetadata->format;
        f.stride = mFrameMetadata->stride;
        f.size = mFrameMetadata->size;

        // Call callback function
        mCallback(&f);
    }

    // Return buffer
    status_t status = mCamera->sendCommand(CAMERA_CMD_ACC_RETURN_BUFFER, frameCounter, 0);

    if (status != NO_ERROR)
        ALOGE("Could not return buffer");
}

void CameraAcc::postMetadataBuffer(sp<IMemoryHeap> heap, uint8_t *heapBase, size_t size, ssize_t offset)
{
    LOG2("@%s, heapBase=%p, size=%d", __FUNCTION__, heapBase, size);

    mFrameMetadata = (Frame*) heapBase;
    mFrameMetadataBuffer = heap;
}

// ########## Debugging ##########

bool CameraAcc::dumpImage2File(const void* data, const unsigned int width_padded, unsigned int width,
                          unsigned int height, const char* name)
{
    ALOGD("@%s", __FUNCTION__);
    char filename[80];
    static unsigned int count = 0;
    size_t bytes = 0;
    FILE *fp;
    char rawdpp[100];
    int ret;
    int size;

    if ((NULL == data) || (0 == width_padded) || (0 == width) || (0 == height) || (NULL == name) || (width_padded < width))
        return false;

    size = width_padded * height * 1.5;

    snprintf(rawdpp, sizeof(rawdpp), "/mnt/cameradump/");

    snprintf(filename, sizeof(filename), "dump_%d_%d_%03u_%s", width_padded,
             height, count, name);
    strncat(rawdpp, filename, strlen(filename));

    ALOGD("Will write image to %s", rawdpp);

    fp = fopen (rawdpp, "w+");
    if (fp == NULL) {
        ALOGE("open file %s failed %s", rawdpp, strerror(errno));
        return false;
    }

    bytes = fwrite(data, size, 1, fp);
    if (bytes < (size_t)size)
        ALOGW("Write less raw bytes to %s: %d, %d", filename, size, bytes);

    count++;

    fclose (fp);

    return true;
}
