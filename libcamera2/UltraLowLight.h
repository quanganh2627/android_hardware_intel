/*
 * Copyright (C) 2013 The Android Open Source Project
 * Copyright (c) 2013 Intel Corporation. All Rights Reserved.
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
#ifndef ANDROID_LIBCAMERA_ULTRALOWLIGHT_H_
#define ANDROID_LIBCAMERA_ULTRALOWLIGHT_H_

#include "PostCaptureThread.h"
#include "Callbacks.h"  // For memory allocation only
#include "PictureThread.h" // For Image metadata declaration
#include "AtomCommon.h"
#include "ia_cp_types.h"
#include "WarperService.h"

namespace android {

#undef STUB_BODY
#undef STUB_BODY_STAT
#undef STUB_BODY_BOOL
#undef STUB_BODY_PTR

#ifdef ENABLE_INTEL_EXTRAS
#define STUB_BODY ;
#define STUB_BODY_STAT ;
#define STUB_BODY_BOOL ;
#define STUB_BODY_PTR ;
#else
#define STUB_BODY {};
#define STUB_BODY_STAT {return NO_ERROR;};
#define STUB_BODY_BOOL {return false;};
#define STUB_BODY_PTR {return NULL;};
#endif

/**
 * \class UltraLowLight
 *
 * Wrapper class for the Morpho Photo Solid  algorithm
 * This class handles the algorithm usage and also the triggering logic.
 *
 */
class UltraLowLight : public IPostCaptureProcessItem
{
public:
    /**
     * \enum
     * User modes for ULL.
     * This controls whether the ULL algorithm is in use or not.
     */
    enum ULLMode {
        ULL_AUTO,   /* ULL active, 3A thresholds will trigger the use of it */
        ULL_ON,     /* ULL active always, forced for all captures */
        ULL_OFF,    /* ULL disabled */
    };

    /**
     *  Algorithm hard coded values
     *  These values have been decided based on the current HAL implementation
     *
     */
    static const char MORPHO_INPUT_FORMAT[];
    static const int  MORPHO_INPUT_FORMAT_V4L2 = V4L2_PIX_FMT_NV12; // Keep these two constants in sync;
    static const int  MAX_INPUT_BUFFERS = 3;

    /**
     *  Activation Threshold
     *  used to trigger ULL based on 3A parameters
     *  There is 1 threshold:
     *  - to activate ULL when scene gets darker (bright threshold)
     *  In cases where scene is too dark and needs flash then flash is used
     *  and ULL is disabled
     *
     *  TODO: They should eventually come from CPF
     **/
    static float ULL_ACTIVATION_GAIN_THRESHOLD;


    /**
     * \enum ULLPreset
     * Different configurations for the ULL algorithm
     */
    enum ULLPreset {
        ULL_PRESET_1 = 0,
        ULL_PRESET_2,
        ULL_PRESET_MAX
    };

public:
    UltraLowLight(Callbacks *callbacks, sp<WarperService> warperService) STUB_BODY
    virtual ~UltraLowLight() STUB_BODY

    void setMode(ULLMode m) STUB_BODY
    bool isActive() STUB_BODY_BOOL
    bool trigger() STUB_BODY_BOOL
    bool isProcessing() STUB_BODY_BOOL

    status_t init(ia_cp_context *iaCpContext, int w, int h, int aPreset, ia_binary_data *aiqb_data) STUB_BODY_STAT
    status_t deinit() STUB_BODY_STAT


    status_t addInputFrame(AtomBuffer *snapshot, AtomBuffer *postview) STUB_BODY_STAT
    status_t addSnapshotMetadata(PictureThread::MetaData &metadata, ia_aiq_exposure_parameters &exposure) STUB_BODY_STAT
    status_t getOuputResult(AtomBuffer *snap, AtomBuffer * pv,
                            PictureThread::MetaData *metadata, int *ULLid) STUB_BODY_STAT
    status_t getInputBuffers(Vector<AtomBuffer> *inputs) STUB_BODY_STAT
    status_t getPostviewBuffers(Vector<AtomBuffer> *postviews) STUB_BODY_STAT
    int getCurrentULLid() { return mULLCounter; };
    int getULLBurstLength() STUB_BODY_STAT

    // implementation of IPostCaptureProcessItem
    status_t process() STUB_BODY_STAT
    status_t cancelProcess() STUB_BODY_STAT

    bool updateTrigger(bool trigger) STUB_BODY_BOOL

    void setZoomFactor(unsigned int zoom) STUB_BODY

    void allocateCopyBuffers(AtomBuffer snapshotDescr, AtomBuffer postviewDesc) STUB_BODY

    AtomBuffer* getSnapshotCopyZoom(AtomBuffer *snapshotBuff) STUB_BODY_PTR

    AtomBuffer* getPostviewCopyZoom(AtomBuffer *postviewBuff) STUB_BODY_PTR

// prevent copy constructor and assignment operator
private:
    UltraLowLight(const UltraLowLight& other);
    UltraLowLight& operator=(const UltraLowLight& other);

private:
    enum State {
        ULL_STATE_NULL,
        ULL_STATE_UNINIT,
        ULL_STATE_INIT,
        ULL_STATE_READY,
        ULL_STATE_PROCESSING,
        ULL_STATE_CANCELING,
        ULL_STATE_DONE
    };

    /**
     * \struct MorphoULLConfig
     *
     * This struct contains the parameters that can be tuned in the algorithm
     * The ULL presets are a a list of elements of this type.
     */
    struct MorphoULLConfig {
        int gain;
        int margin;
        int block_size;
        int obc_level;
        int y_nr_level;
        int c_nr_level;
        int y_nr_type;
        int c_nr_type;
        MorphoULLConfig(){};
        MorphoULLConfig(int gain,int margin, int block_size, int obc_level,
                        int y_nr_level, int c_nr_level, int y_nr_type, int c_nr_type):
                        gain(gain),
                        margin(margin),
                        block_size(block_size),
                        obc_level(obc_level),
                        y_nr_level(y_nr_level),
                        c_nr_level(c_nr_level),
                        y_nr_type(y_nr_type),
                        c_nr_type(c_nr_type)
        {}
    };

private:
    status_t initMorphoLib(int w, int h, int aPreset) STUB_BODY_STAT
    status_t configureMorphoLib(void) STUB_BODY_STAT
    status_t processMorphoULL() STUB_BODY_STAT
    void deinitMorphoLib() STUB_BODY
    void freeWorkingBuffer() STUB_BODY
    void AtomToMorphoBuffer(const   AtomBuffer *atom, void* morpho) STUB_BODY

    status_t processIntelULL() STUB_BODY_STAT
    status_t initIntelULL(ia_cp_context *iaCpContext, int w, int h, ia_binary_data *aiqb_data) STUB_BODY_STAT
    void deinitIntelULL() STUB_BODY
    void AtomToIaFrameBuffer(const AtomBuffer *atom, ia_frame* frame) STUB_BODY

    void setState(enum State aState);
    enum State getState();

    status_t gpuImageRegistration(AtomBuffer *target, AtomBuffer *source, int *imregFallback);

private:
    struct MorphoULL;       /*!> Forward declaration of the opaque struct for Morpho's algo configuration */
    MorphoULL        *mMorphoCtrl;
    ia_cp_ull_cfg *mIntelUllCfg;
    Callbacks   *mCallbacks;
    AtomBuffer  mOutputBuffer;  /*!> Output of the ULL processing. this is actually the first input buffer passed */
    AtomBuffer  mOutputPostView;  /*!> post view image for the first snapshot, used as output one */
    State       mState;
    int mULLCounter;        /*!> Running counter of ULL shots. Used as frame id towards application */
    int mWidth;
    int mHeight;
    int mCurrentPreset;
    MorphoULLConfig mPresets[ULL_PRESET_MAX];

    Vector<AtomBuffer> mInputBuffers;      /*!< snapshots */
    Vector<AtomBuffer> mPostviewBuffs;     /*!< postview buffers */

    PictureThread::MetaData mSnapMetadata;  /*!> metadata of the first snapshot taken */

    /**
     * algorithm external status
     */
    ULLMode        mUserMode; /*!> User selected mode of operation of the ULL feature */
    Mutex          mStateMutex; /*!> Protects the trigger and state variable that are queried by different threads*/
    bool           mTrigger;  /*!> Only valid if in auto mode. It signal that ULL should be used. */
    bool           mUseIntelULL; /*!> Use Intel ULL algorithm instead of Morpho. */

    sp<WarperService> mWarper; /*!> Service used to perform frame warping on GPU */

    unsigned int mZoomFactor; /*!> Zoom factor to be sent to ull_compose. */
    AtomBuffer   mSnapshotCopy;
    AtomBuffer   mPostviewCopy;
    bool         mCopyBuffsAllocated;

    ia_cp_context *mIaCpContext;
    ia_cp_ull *mIaCpUll;
};
}  //namespace android
#endif /* ANDROID_LIBCAMERA_ULTRALOWLIGHT_H_ */
