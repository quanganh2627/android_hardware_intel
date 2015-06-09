/*
 * Copyright (c) 2013 Intel Corporation
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

#ifndef ANDROID_LIBCAMERA_V4L2DEVICE_H_
#define ANDROID_LIBCAMERA_V4L2DEVICE_H_

#include <utils/RefBase.h>
#include <utils/String8.h>
#include <utils/Vector.h>
#include <utils/Mutex.h>
#include <linux/atomisp.h>
#include <linux/videodev2.h>

#ifdef USE_CAMERA_IO_BREAKDOWN
#define pioctl(fd, ctrlId, attr) \
({ \
    int reti; \
    PERFORMANCE_TRACES_IO_BREAKDOWN(#ctrlId); \
    reti = ioctl(fd, ctrlId, attr); \
    reti; \
 })

#define popen(name, attr) \
({ \
    int fd; \
    PERFORMANCE_TRACES_IO_BREAKDOWN("open"); \
    fd = ::open(name, attr); \
    fd; \
})

#define pclose(fd) \
({ \
    int ret; \
    PERFORMANCE_TRACES_IO_BREAKDOWN("Close"); \
    ret = ::close(fd); \
    ret; \
})

#define pxioctl(device, ctrlId, attr) \
({ \
    int reti; \
    PERFORMANCE_TRACES_IO_BREAKDOWN(#ctrlId); \
    reti = device->xioctl(ctrlId, attr); \
    reti; \
})

#define _ppoll(fd, value, timeout) \
({ \
    int reti; \
    PERFORMANCE_TRACES_IO_BREAKDOWN("poll"); \
    reti = ::poll(fd, value, timeout); \
    reti; \
})

#else
#define pioctl(fd, ctrlId, attr) \
    ioctl(fd, ctrlId, attr)

#define popen(name, attr) \
    ::open(name, attr)

#define pclose(fd) \
    ::close(fd)

#define pxioctl(device, ctrlId, attr) \
    device->xioctl(ctrlId, attr)

#define _ppoll(fd, value, timeout) \
    ::poll(fd, value, timeout)

#endif // USE_CAMERA_IO_BREAKDOWN

namespace android {


/**
 * v4l2 buffer descriptor.
 *
 * This information is stored in the pool
 */
struct v4l2_buffer_info {
    void *data;
    size_t length;
    int width;
    int height;
    int format;
    int cache_flags;        /*!< initial flags used when creating buffers */
    struct v4l2_buffer vbuffer;
};


/**
 * A class encapsulating simple V4L2 device operations
 *
 * Base class that contains common V4L2 operations used by video nodes and
 * subdevices. It provides a slightly higher interface than IOCTLS to access
 * the devices. It also stores:
 * - State of the device to protect from invalid usage sequence
 * - Name
 * - File descriptor
 */
class V4L2DeviceBase: public RefBase {
public:
    V4L2DeviceBase(const char *name, int id);
    virtual ~V4L2DeviceBase();

    virtual status_t open();
    virtual status_t close();

    virtual int xioctl(int request, void *arg) const;
    virtual int poll(int timeout);

    virtual status_t setControl (int aControlNum, const int value, const char *name);
    virtual status_t getControl (int aControlNum, int *value);

    virtual int subscribeEvent(int event, int id = 0);
    virtual int unsubscribeEvent(int event, int id = 0);
    virtual int dequeueEvent(struct v4l2_event *event);

    bool isOpen() { return mFd != -1; };

public:
    const int mId;    /*!< Convenient index to identify the device in old AtomISP code
                          (TODO: remove once it is not needed) */

protected:
    String8      mName;     /*!< path to device in file system, ex: /dev/video0 */
    int          mFd;       /*!< file descriptor obtained when device is open */

};


/**
 * A class encapsulating simple V4L2 video device node operations
 *
 * This class extends V4L2DeviceBase and adds extra internal state
 * and more convenience methods to  manage an associated buffer pool
 * with the device.
 * This class introduces new methods specifics to control video device nodes
 *
 */
class V4L2VideoNode: public V4L2DeviceBase {
public:
    enum VideNodeDirection {
        INPUT_VIDEO_NODE,   /*!< input video devices like cameras or capture cards */
        OUTPUT_VIDEO_NODE  /*!< output video devices like displays */
    };

public:
    V4L2VideoNode(const char *name, int id, VideNodeDirection nodeDirection = INPUT_VIDEO_NODE);
    virtual ~V4L2VideoNode();

    virtual status_t open();
    virtual status_t close();

    status_t setInput(int index);
    status_t queryCap(struct v4l2_capability *cap);
    status_t enumerateInputs(struct v4l2_input *anInput);

    // Video node configuration
    int getFramerate(float * framerate, int width, int height, int pix_fmt);
    status_t setParameter (struct v4l2_streamparm *aParam);
    status_t setFormat(AtomBuffer &aFormatDescriptor);
    status_t setFormat(struct v4l2_format &aFormat);
    status_t queryCapturePixelFormats(Vector<v4l2_fmtdesc> &formats);

    // Buffer pool management
    status_t setBufferPool(void **pool, int poolSize, AtomBuffer *aFormatDescriptor, bool cached);
    void destroyBufferPool();
    int createBufferPool(unsigned int buffer_count);
    int activateBufferPool();

    // Buffer flow control
    int stop(bool leaveConfigured = false);
    int start(int buffer_count, int initialSkips);

    int grabFrame(struct v4l2_buffer_info *buf);
    int putFrame(unsigned int index);

    // Convenience accessors
    bool isStarted() const { return mState == DEVICE_STARTED; };
    unsigned int getFrameCount() const { return mFrameCounter; };
    unsigned int getInitialFrameSkips() const { return mInitialSkips; };

private:
    int qbuf(struct v4l2_buffer_info *buf);
    int dqbuf(struct v4l2_buffer_info *buf);
    int newBuffer(int index, struct v4l2_buffer_info &buf);
    int freeBuffer(struct v4l2_buffer_info *buf_info);
    int requestBuffers(uint num_buffers);

private:

    enum VideoNodeState  {
            DEVICE_CLOSED = 0,  /*!< kernel device closed */
            DEVICE_OPEN,        /*!< device node opened */
            DEVICE_CONFIGURED,  /*!< device format set, IOC_S_FMT */
            DEVICE_PREPARED,    /*!< device has allocated memory available (set_buffer_pool)*/
            DEVICE_POPULATED,    /*!< buffers queued, IOC_QBUF */
            DEVICE_STARTED,     /*!< stream started, IOC_STREAMON */
            DEVICE_ERROR        /*!< undefined state */
        };

    VideoNodeState  mState;
    // Device capture configuration
    AtomBuffer mFormatDescriptor;

    unsigned int mFrameCounter;
    unsigned int mInitialSkips;
    unsigned int mDeviceId;

    Vector<struct v4l2_buffer_info> mSetBufferPool; /*!< This is the buffer pool set before the device is prepared*/
    Vector<struct v4l2_buffer_info> mBufferPool;    /*!< This is the active buffer pool */

    VideNodeDirection mDirection;
};

/**
 * A class encapsulating simple V4L2 sub device node operations
 *
 * Sub-devices are control points to the new media controller architecture
 * in V4L2
 *
 */
class V4L2Subdevice: public V4L2DeviceBase {
public:
    V4L2Subdevice(const char *name, int id);
    virtual ~V4L2Subdevice();

};

} /* namespace android */
#endif /* ANDROID_LIBCAMERA_V4L2DEVICE_H_ */
