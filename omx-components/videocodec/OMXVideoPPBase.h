/*
* Copyright (c) 2009-2011 Intel Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/


#ifndef OMX_VIDEO_PP_BASE_H_
#define OMX_VIDEO_PP_BASE_H_


#include "OMXComponentCodecBase.h"
#include "PPWorker.h"

using namespace android;

#define Intel_PrivateDataVideoInfo 0x7F000000

typedef struct INTEL_VERSIONTYPE {
    uint32_t nVersionMajor;
    uint32_t nVersionMinor;
} INTEL_VERSIONTYPE;

class VPPWorker;

class OMXVideoPPBase : public OMXComponentCodecBase {
public:
    OMXVideoPPBase();
    virtual ~OMXVideoPPBase();

protected:
    virtual OMX_ERRORTYPE InitInputPort(void);
    virtual OMX_ERRORTYPE InitOutputPort(void);
    virtual OMX_ERRORTYPE InitInputPortFormatSpecific(OMX_PARAM_PORTDEFINITIONTYPE *input)/* = 0*/;
    virtual OMX_ERRORTYPE InitOutputPortFormatSpecific(OMX_PARAM_PORTDEFINITIONTYPE *output);


    virtual OMX_ERRORTYPE BuildHandlerList(void);
    DECLARE_HANDLER(OMXVideoPPBase, NativeBuffer);
    DECLARE_HANDLER(OMXVideoPPBase, FilterTypes);
    DECLARE_HANDLER(OMXVideoPPBase, DenoiseLevel);
    DECLARE_HANDLER(OMXVideoPPBase, DeinterlaceLevel);
    DECLARE_HANDLER(OMXVideoPPBase, ColorBalanceLevel);
    DECLARE_HANDLER(OMXVideoPPBase, SharpnessLevel);
    DECLARE_HANDLER(OMXVideoPPBase, ScaleLevel);
    DECLARE_HANDLER(OMXVideoPPBase, Intel3PLevel);

protected:
    VPPWorker * mVPP;
    bool mErrorReportEnabled;

    virtual OMX_ERRORTYPE ProcessorInit(void);
    virtual OMX_ERRORTYPE ProcessorReset(void);
    virtual OMX_ERRORTYPE ProcessorDeinit(void);
    virtual OMX_ERRORTYPE ProcessorStart(void);
    virtual OMX_ERRORTYPE ProcessorStop(void);
    virtual OMX_ERRORTYPE ProcessorPause(void);
    virtual OMX_ERRORTYPE ProcessorResume(void);
    virtual OMX_ERRORTYPE ProcessorFlush(OMX_U32 portIndex);
    virtual OMX_ERRORTYPE ProcessorProcess(
            OMX_BUFFERHEADERTYPE ***pBuffers,
            buffer_retain_t *retains,
            OMX_U32 numberBuffers);

private:
    //TODO: this should not define here
    static const uint32_t MAX_GRAPHIC_BUFFER_NUM = 64;
    enum {
        // OMX_PARAM_PORTDEFINITIONTYPE
        INPORT_MIN_BUFFER_COUNT = 5,
        INPORT_ACTUAL_BUFFER_COUNT = 5,
        INPORT_BUFFER_SIZE = 1382400,

        // OMX_PARAM_PORTDEFINITIONTYPE
        OUTPORT_MIN_BUFFER_COUNT = 5,
        OUTPORT_ACTUAL_BUFFER_COUNT = 5,
        OUTPORT_BUFFER_SIZE = 1382400,

        OUTPORT_NATIVE_BUFFER_COUNT = 10,
    };
    uint32_t mOMXInBufferHeaderTypePtrNum;
    OMX_BUFFERHEADERTYPE *mOMXInBufferHeaderTypePtrArray[MAX_GRAPHIC_BUFFER_NUM];
    uint32_t mOMXOutBufferHeaderTypePtrNum;
    OMX_BUFFERHEADERTYPE *mOMXOutBufferHeaderTypePtrArray[MAX_GRAPHIC_BUFFER_NUM];

    struct GraphicBufferParam {
        uint32_t graphicBufferStride;
        uint32_t graphicBufferWidth;
        uint32_t graphicBufferHeight;
        uint32_t graphicBufferColorFormat;
    };
    GraphicBufferParam mGraphicBufferParam;

    //VPP param
    OMX_U32 mFilters;
    FilterParam mFilterParam;
};

#endif /* OMX_VIDEO_PP_BASE_H_ */
