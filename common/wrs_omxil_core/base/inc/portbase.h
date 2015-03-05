/*
 * portbase.h, port base class
 *
 * Copyright (c) 2009-2010 Wind River Systems, Inc.
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

#ifndef __PORTBASE_H
#define __PORTBASE_H

#include <pthread.h>

#include <OMX_Core.h>
#include <OMX_Component.h>
#ifdef ANDROID
#include <utils/RefBase.h>
#endif
#include <list.h>
#include <queue.h>

class PortBase
{
public:
    /*
     * constructor & destructor
     */
    PortBase();
    PortBase(const OMX_PARAM_PORTDEFINITIONTYPE *portdefinition);
    virtual ~PortBase();

    /* end of constructor & destructor */

    /*
     * accessor
     */
    /* owner */
    void SetOwner(OMX_COMPONENTTYPE *handle);
    OMX_COMPONENTTYPE *GetOwner(void);

    /* for ReturnThisBuffer() */
    OMX_ERRORTYPE SetCallbacks(OMX_HANDLETYPE hComponent,
                               OMX_CALLBACKTYPE *pCallbacks,
                               OMX_PTR pAppData);
    /* end of accessor */

    /*
     * component methods & helpers
     */
    /* Get/SetParameter */
    OMX_ERRORTYPE SetPortDefinition(const OMX_PARAM_PORTDEFINITIONTYPE *p,
                                    bool overwrite_readonly);
    const OMX_PARAM_PORTDEFINITIONTYPE *GetPortDefinition(void);

    /* Use/Allocate/FreeBuffer */
    OMX_ERRORTYPE UseBuffer(OMX_BUFFERHEADERTYPE **ppBufferHdr,
                            OMX_U32 nPortIndex,
                            OMX_PTR pAppPrivate,
                            OMX_U32 nSizeBytes,
                            OMX_U8 *pBuffer);
    OMX_ERRORTYPE AllocateBuffer(OMX_BUFFERHEADERTYPE **ppBuffer,
                                 OMX_U32 nPortIndex,
                                 OMX_PTR pAppPrivate,
                                 OMX_U32 nSizeBytes);
    OMX_ERRORTYPE FreeBuffer(OMX_U32 nPortIndex,
                             OMX_BUFFERHEADERTYPE *pBuffer);

    /*
     * called in ComponentBase::TransStateToLoaded(OMX_StateIdle) or
     * in ComponentBase::TransStateToIdle(OMX_StateLoaded)
     * wokeup by Use/Allocate/FreeBuffer
     */
    void WaitPortBufferCompletion(void);

    /* Empty/FillThisBuffer */
    OMX_ERRORTYPE PushThisBuffer(OMX_BUFFERHEADERTYPE *pBuffer);
    OMX_BUFFERHEADERTYPE *PopBuffer(void);
    OMX_U32 BufferQueueLength(void);
    OMX_ERRORTYPE RemoveThisBuffer(OMX_BUFFERHEADERTYPE *pBuffer);

    /* Empty/FillBufferDone */
    OMX_ERRORTYPE ReturnThisBuffer(OMX_BUFFERHEADERTYPE *pBuffer);

    /* retain buffer */
    OMX_ERRORTYPE RetainThisBuffer(OMX_BUFFERHEADERTYPE *pBuffer,
                                   bool accumulate);
    /*
     * components have responsibilty of calling this function to return all
     * accumulated buffers to omx-il clients.
     */
    void ReturnAllRetainedBuffers(void);

    /* flush all buffers not under processing */
    OMX_ERRORTYPE FlushPort(void);

    bool IsEnabled(void);

    OMX_DIRTYPE GetPortDirection(void);
    OMX_U32 GetPortBufferCount(void);

    OMX_ERRORTYPE PushMark(OMX_MARKTYPE *mark);
    OMX_MARKTYPE *PopMark(void);

    /* SendCommand(OMX_CommandPortDisable/Enable) */
    OMX_ERRORTYPE TransState(OMX_U8 state);

    /* EventHandler(OMX_EventPortSettingChanged) */
    OMX_ERRORTYPE ReportPortSettingsChanged(void);

    /* EventHandler(OMX_IndexConfigCommonOutputCrop) */
    OMX_ERRORTYPE ReportConfigOutputCrop(void);

    /* get frame size */
    OMX_U32 getFrameBufSize(OMX_COLOR_FORMATTYPE colorFormat, OMX_U32 width, OMX_U32 height);

    /* end of component methods & helpers */

    /* TransState, state */
    static const OMX_U8 OMX_PortDisabled = 0;
    static const OMX_U8 OMX_PortEnabled = 1;

private:
    /* common routines for constructor */
    void __PortBase(void);

    /*
     * component methods & helpers
     */
    OMX_STATETYPE GetOwnerState(void);

    /* end of component methods & helpers */

    /* buffer headers */
    struct list *buffer_hdrs;
    OMX_U32 nr_buffer_hdrs;
    bool buffer_hdrs_completion; /* Use/Allocate/FreeBuffer completion flag */
    pthread_mutex_t hdrs_lock;
    pthread_cond_t hdrs_wait;

    struct queue bufferq;
    pthread_mutex_t bufferq_lock;

    /* retained buffers (only accumulated buffer) */
    struct queue retainedbufferq;
    pthread_mutex_t retainedbufferq_lock;

    struct queue markq;
    pthread_mutex_t markq_lock;

    /* state */
    OMX_U8 state;
    pthread_mutex_t state_lock;

    /* parameter */
    OMX_PARAM_PORTDEFINITIONTYPE portdefinition;
    /* room for portdefinition.format.*.cMIMEType */
    char definition_format_mimetype[OMX_MAX_STRINGNAME_SIZE];

    OMX_AUDIO_PARAM_PORTFORMATTYPE audioparam;

    /* owner handle */
    OMX_COMPONENTTYPE *owner;

    /* omx standard callbacks */
    OMX_PTR appdata;
    OMX_CALLBACKTYPE callbacks;

    /* wrs component handle */
    class ComponentBase *cbase;
};

/* end of PortBase */

#endif /* __PORTBASE_H */
