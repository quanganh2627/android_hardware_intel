/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ANDROID_LIBCAMERA_SENSOR_CLASS
#define ANDROID_LIBCAMERA_SENSOR_CLASS

#include <utils/RefBase.h>

#include "ICameraHwControls.h"
#include "PlatformData.h"
#include "AtomIspObserverManager.h" // IObserverSubject
#include "AtomDelayFilter.h"
#include "AtomFifo.h"

namespace android {

class V4L2DeviceBase;
class V4L2VideoNode;
class V4L2Subdevice;

class SensorHW
    :public IHWSensorControl //!< Provides sensor control interface
    ,private IObserverSubject //!< Provides frame synchronization source
    ,private IAtomIspObserver //!< for temporary workaround, see AtomISP::start()
    ,public RefBase
{

public:
    SensorHW(int cameraId);
    virtual ~SensorHW();
    virtual void reset(int cameraId);

    status_t selectActiveSensor(sp<V4L2VideoNode> &device);
    status_t prepare(bool preQueuedExposure);
    status_t start();
    status_t stop();
    IObserverSubject* getFrameSyncSource() { return (IObserverSubject*) this; };
    nsecs_t getFrameTimestamp(nsecs_t event_ts);
    status_t getIspDevicePath(char *fd, int size);
    status_t setIspSubDevId(int id);

    /* IHWSensorControl overloads, */
    virtual const char * getSensorName(void);
    virtual int getCurrentCameraId(void);
    virtual void getMotorData(sensorPrivateData *sensor_data);
    virtual void getSensorData(sensorPrivateData *sensor_data);
    virtual int getModeInfo(struct atomisp_sensor_mode_data *mode_data);
    virtual int setExposureTime(int time);
    virtual int getExposureTime(int *exposure_time);
    virtual int getAperture(int *aperture);
    virtual int getFNumber(unsigned short  *fnum_num, unsigned short *fnum_denom);
    virtual int setExposureMode(v4l2_exposure_auto_type type);
    virtual int getExposureMode(v4l2_exposure_auto_type * type);
    virtual int setExposureBias(int bias);
    virtual int getExposureBias(int * bias);
    virtual int setSceneMode(v4l2_scene_mode mode);
    virtual int getSceneMode(v4l2_scene_mode * mode);
    virtual int setWhiteBalance(v4l2_auto_n_preset_white_balance mode);
    virtual int getWhiteBalance(v4l2_auto_n_preset_white_balance * mode);
    virtual int setIso(int iso);
    virtual int getIso(int * iso);
    virtual int setIsoMode(int mode);
    virtual int setAeMeteringMode(v4l2_exposure_metering mode);
    virtual int getAeMeteringMode(v4l2_exposure_metering * mode);
    virtual int setAeFlickerMode(v4l2_power_line_frequency mode);
    virtual int setAfMode(int mode);
    virtual int getAfMode(int *mode);
    virtual int setAfEnabled(bool enable);
    virtual int setAfWindows(const CameraWindow *windows, int numWindows);
    virtual int set3ALock(int aaaLock);
    virtual int get3ALock(int * aaaLock);
    virtual int setAeFlashMode(int mode);
    virtual int getAeFlashMode(int *mode);
    virtual int getRawFormat();

    virtual unsigned int getExposureDelay();
    virtual int setExposure(struct atomisp_exposure *exposure);
    virtual int setExposureGroup(struct atomisp_exposure exposures[], int depth);

    virtual float getFramerate() const;
    virtual status_t setFramerate(int fps);

    virtual void getFrameSizes(Vector<v4l2_subdev_frame_size_enum> &sizes);

    virtual status_t waitForFrameSync();

    /* IAtomIspObserver overloads */
    virtual bool atomIspNotify(Message *msg, const ObserverState state);

private:
    virtual const char* getName() { return "FrameSyncSource"; }
    virtual status_t observe(IAtomIspObserver::Message *msg);

private:
    static const int MAX_SENSOR_NAME_LENGTH = 32;

    struct cameraInfo {
        uint32_t index;      //!< V4L2 index
        char name[MAX_SENSOR_NAME_LENGTH];
        atomisp_camera_port ispPort;
    };

    size_t enumerateInputs(Vector<struct cameraInfo> &);
    status_t sensorStoreRawFormat(Vector<v4l2_fmtdesc> &formats);

    // Helper methods for Media Controller usage
    // TODO: generalize into Media device class
    status_t findConnectedEntity(sp<V4L2DeviceBase> &mediaCtl,
        const struct media_entity_desc &mediaEntityDescSrc,
        struct media_entity_desc &mediaEntityDescDst, int &padIndex);
    status_t findConnectedEntityByName(sp<V4L2DeviceBase> &mediaCtl,
        struct media_entity_desc mediaEntityDescSrc,
        struct media_entity_desc &mediaEntityDescDst,
        int &padIndex, char const *name, int nameLen, int depth);
    status_t findMediaEntityByName(sp<V4L2DeviceBase> &mediaCtl,
            char const* entityName, struct media_entity_desc &mediaEntityDesc);
    status_t findMediaEntityById(sp<V4L2DeviceBase> &mediaCtl, int index,
        struct media_entity_desc &mediaEntityDesc);
    status_t openSubdevice(sp<V4L2DeviceBase> &subdev, int major, int minor);
    status_t openSubdevices();
    void getPadFormat(sp<V4L2DeviceBase> &subdev, int padIndex, int &width, int &height);

    // Exposure synchronization
    struct exposure_history_item {
        bool    received;
        bool    applied;
        nsecs_t frame_ts;
        struct atomisp_exposure exposure;
    };
    status_t initializeExposureFilter();
    int frameSyncProc(nsecs_t timestamp);
    inline void processGainDelay(struct atomisp_exposure *);
    int setSensorExposure(struct atomisp_exposure *exposure);
    unsigned int vbiIntervalForItem(unsigned int index);
    unsigned int frameIntervalForItem(unsigned int index);
    unsigned int cumulateFrameIntervals(unsigned int index, unsigned int frames);
    struct exposure_history_item* produceExposureHistory(struct atomisp_exposure *exposure, nsecs_t frame_ts);
    void processExposureHistory(nsecs_t timestamp);
    void updateExposureEstimate(nsecs_t timestamp);
    struct exposure_history_item* getPrevAppliedItem(int &id);
    void resetEstimates(struct exposure_history_item *activeItem);

// protected member variables, accessible by subclasses
// TODO: Rename to p* instead of m*
protected:
    sp<V4L2DeviceBase> mSensorSubdevice;
    sp<V4L2DeviceBase> mIspSubdevice;
    sp<V4L2VideoNode> mDevice;
    sp<V4L2Subdevice> mSyncEventDevice;
    SensorType        mSensorType;
    struct cameraInfo mCameraInput;
    int mCameraId;
    bool mStarted; //<! state used to indentify controls in streamon state
    char mIspSubDevName[MAX_SENSOR_NAME_LENGTH];

// private member variables:
private:
    // ModeData stored
    struct atomisp_sensor_mode_data mInitialModeData;
    bool mInitialModeDataValid;

    int mRawBayerFormat;
    int mOutputWidth;
    int mOutputHeight;

    // Common frame synchronization
    Mutex mFrameSyncMutex;
    Condition mFrameSyncCondition;
    enum FrameSyncSource {
        FRAME_SYNC_NA,
        FRAME_SYNC_SOF = V4L2_EVENT_FRAME_SYNC,
        FRAME_SYNC_EOF = V4L2_EVENT_FRAME_END
    } mFrameSyncSource;
    int mCssVersion;

    // Exposure synchronization
    unsigned int mActiveItemIndex;
    bool mDirectExposureIo;     /* set exposure directly in setExposure() caller context*/
    bool mPostponePrequeued;    /* do not discard if more than one exposure settings applied per frame */
    unsigned int mExposureLag;  /* delay of exposure applying based on configuration */
    unsigned int mLatestExpId;  /* the latest exposure id from SOF or EOF event */
    AtomDelayFilter <unsigned int>   *mGainDelayFilter;
    AtomFifo <struct exposure_history_item> *mExposureHistory;
    struct atomisp_exposure          mCurrentExposure;
    int mGroupId;
}; // class SensorHW

}; // namespace android

#endif
