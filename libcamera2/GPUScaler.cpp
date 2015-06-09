/*
 * Copyright (c) 2013 Intel Corporation.
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

// Need GL and EGL extensions to enable direct render to gralloc'd buffers
#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES

#define LOG_TAG "Camera_GPUScaler"

// If defined enable GL and EGL error logs on LOG2, otherwise GL and EGL error checking are disabled
//#define GPU_SCALER_DEBUG

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sched.h>
#include <sys/resource.h>
#include <string.h>
#include <math.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <utils/Timers.h>
#include "LogHelper.h"
#include <ui/GraphicBuffer.h>
#include <gui/SurfaceComposerClient.h>
#include <ui/DisplayInfo.h>
#include <gui/ISurfaceComposer.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include "GPUScalerEglUtil.h"
#include "GPUScaler.h"
#include "PlatformData.h" // for HAL pixel format

namespace android {

GPUScaler::GPUScaler(int cameraId) :
        mCameraId(cameraId),
        mDisplay(EGL_NO_DISPLAY),
        mContext(EGL_NO_CONTEXT),
        mSurface(EGL_NO_SURFACE),
        mNumFrames(0),
        mInputBufferCounter(0),
        mOutputBufferCounter(0)
{
    LOG2("@%s\n", __FUNCTION__);
#ifdef GPU_SCALER_DEBUG
    LOG2("DEBUG Build %s %s\n",__DATE__,__TIME__);
#else
    LOG2("RELEASE Build %s %s\n", __DATE__, __TIME__);
#endif
    setDefaultConfig();
    logConfig();
    // init EGL and GL
    initGPU();
    // init vertex and fragment shaders
    initShaders();
    // init zoom factor (frag shaders must be initialized first)
    setZoomFactor(mZoomConfig.magFactor);

    // init EGL image resources
    for (int i = 0; i < NUMBER_GFX_BUFFERS; i++) {
        for (int j = 0; j < 3; j++) {
            dstEglImageHandle[i][j] = EGL_NO_IMAGE_KHR;
            srcEglImageHandle[i][j] = EGL_NO_IMAGE_KHR;
            if (j < 2) {
                dstRenderBuffers[i][j] = 0;
                dstFboNames[i][j] = 0;
                srcTexNames[i][j] = 0;
            }
        }
    }
}

GPUScaler::~GPUScaler() {
    LOG2("@%s\n", __FUNCTION__);
    // report frame rate
    if (mZoomConfig.perfMonMode == eOn) {
        struct timeval Tstop;
        gettimeofday(&Tstop, NULL);
        long elapsed = ((Tstop.tv_sec - mTstart.tv_sec) * 1000
                       + (Tstop.tv_usec - mTstart.tv_usec) / 1000);
        if (elapsed >= 1000) {
            float fps = (mNumFrames * 1000.0f) / elapsed;
            LOG2("Frame Rate: %3.1f fps\n", fps);
        } else {
            LOG2("Insufficient run length to compute fps\n");
        }
    }

    // release GL resources
    glDeleteProgram(mP_NV12z);

    // release EGL resources
    for (int i = 0; i < NUMBER_GFX_BUFFERS; i++) {
        for (int j = 0; j < 3; j++) {
            if (dstEglImageHandle[i][j] == EGL_NO_IMAGE_KHR)
                break;
            eglDestroyImageKHR(mDisplay, dstEglImageHandle[i][j]);
        }

        if (dstFboNames[i][0])
            glDeleteFramebuffers(2, dstFboNames[i]);

        if (dstRenderBuffers[i][0])
            glDeleteRenderbuffers(2, dstRenderBuffers[i]);

        for (int k = 0; k < 3; k++) {
            if (srcEglImageHandle[i][k] == EGL_NO_IMAGE_KHR)
                break;
            eglDestroyImageKHR(mDisplay, srcEglImageHandle[i][k]);
        }

        if (srcTexNames[i][0])
            glDeleteTextures(2, srcTexNames[i]);
    }

    eglMakeCurrent_EC(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(mDisplay, mSurface);
    eglDestroyContext(mDisplay, mContext);
    eglTerminate(mDisplay);

    LOG2("@%s done\n", __FUNCTION__);
}

status_t GPUScaler::initGPU() {
    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLint majorVersion(0);
    EGLint minorVersion(0);
    EGLint w(0);
    EGLint h(0);

    LOG2("@%s\n", __FUNCTION__);

    // init EGL
    mDisplay = eglGetDisplay_EC(EGL_DEFAULT_DISPLAY);
    eglInitialize_EC(mDisplay, &majorVersion, &minorVersion);
    LOG2("EGL version %d.%d\n", majorVersion, minorVersion);
    eglBindAPI_EC(EGL_OPENGL_ES_API);

    // get native surface
    sp<IBinder> dtoken(
            SurfaceComposerClient::getBuiltInDisplay(
                    ISurfaceComposer::eDisplayIdMain));
    DisplayInfo dinfo;
    status_t status = SurfaceComposerClient::getDisplayInfo(dtoken, &dinfo);
    if (status) {
        ALOGE("SurfaceComposerClient::getDisplayInfo failed\n");
        return status;
    }
    sp<SurfaceComposerClient> session = new SurfaceComposerClient();
    LOG2("@createSurface()\n");
    sp<SurfaceControl> control = session->createSurface(
            String8("CameraGPUScaler_NV12_GPU"), dinfo.w, dinfo.h,
            PIXEL_FORMAT_RGBA_8888);
    LOG2("@openGlobalTransaction()\n");
    SurfaceComposerClient::openGlobalTransaction();
    LOG2("@setLayer()\n");
    control->setLayer(0x40000000);
    LOG2("@closeGlobalTransaction()\n");
    SurfaceComposerClient::closeGlobalTransaction();
    LOG2("@getSurface\n");
    sp<Surface> s = control->getSurface();

    // finish EGL and GL init
    const EGLint attribs[] = { EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 0, EGL_CONFORMANT,
            EGL_OPENGL_ES2_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE };
    EGLConfig config(0);
    EGLint numConfigs(0);

    eglChooseConfig_EC(mDisplay, attribs, &config, 1, &numConfigs);
    mSurface = eglCreateWindowSurface_EC(mDisplay, config, s.get(), NULL);
    if (mSurface == NULL) {
        ALOGE("eglCreateWindowSurface_EC error");
        return UNKNOWN_ERROR;
    }
    mContext = eglCreateContext_EC(mDisplay, config, NULL, context_attribs);
    eglMakeCurrent_EC(mDisplay, mSurface, mSurface, mContext);
    eglQuerySurface_EC(mDisplay, mSurface, EGL_WIDTH, &w);
    eglQuerySurface_EC(mDisplay, mSurface, EGL_HEIGHT, &h);
    LOG2("EGL window dimensions: %d x %d\n", w, h);
    pzEglUtil_printGLString("Version", GL_VERSION);
    pzEglUtil_printGLString("Vendor", GL_VENDOR);
    pzEglUtil_printGLString("Renderer", GL_RENDERER);
    pzEglUtil_printGLString("Extensions", GL_EXTENSIONS);

    // FIXME:  1-byte align perf penalty
    glPixelStorei_EC(GL_UNPACK_ALIGNMENT, 1);

    return NO_ERROR;
}

int GPUScaler::setDefaultConfig() {
    LOG2("@%s", __FUNCTION__);

    // Set default configuration
    mDefaultZoomConfig.inputMode = eCamera;
    mDefaultZoomConfig.outputMode = eDisplay;
    mDefaultZoomConfig.perfMonMode = eOff;
    mDefaultZoomConfig.zoomMode = eBilinear;
    mDefaultZoomConfig.magMode = eClient;
    mDefaultZoomConfig.magFactor = 1.0;
    mDefaultZoomConfig.magMin = 1.0;
    mDefaultZoomConfig.magMax = 1.0;
    mDefaultZoomConfig.magStep = 1.0;
    mDefaultZoomConfig.magFreq = 1;
    mDefaultZoomConfig.shaderSrcLoc = eInternal;

    // Start with defaults
    mZoomConfig = mDefaultZoomConfig;

    return (0);
}

int GPUScaler::logConfig() {
    LOG2("@%s", __FUNCTION__);
    if (mZoomConfig.inputMode == eCamera) {
        LOG2("Input: camera\n");
    }
    if (mZoomConfig.outputMode == eDisplay) {
        LOG2("Output: display\n");
    }
    if (mZoomConfig.perfMonMode == eOn) {
        LOG2("Perfmon mode: on\n");
    } else {
        LOG2("Perfmon mode: off\n");
    }
    if (mZoomConfig.zoomMode == eBilinear) {
        LOG2("Zoom mode: bilinear\n");
    } else {
        LOG2("Zoom mode: bicubic\n");
    }
    switch (mZoomConfig.magMode) {
    case eClient:
        LOG2("Mag Fact: client API, init mag factor: %2.1f X\n", mZoomConfig.magFactor);
        break;
    case eFixed:
        LOG2("Mag Fact: constant: %2.1f\n", mZoomConfig.magFactor);
        break;
    case eLoop:
        LOG2("Mag Fact: demo loop: min/max/step/freq: %2.1f/%2.1f/%2.1f/%d\n",
             mZoomConfig.magMin, mZoomConfig.magMax, mZoomConfig.magStep, mZoomConfig.magFreq);
        break;
    }
    if (mZoomConfig.shaderSrcLoc == eInternal) {
        LOG2("Shader Sources: embedded\n");
    }
    return (0);
}

int GPUScaler::addOutputBuffer(buffer_handle_t *pBufHandle, int width, int height, int bpl, int format)
{
    mOutputBufferCounter++;
    LOG2("@%s output buffer count %d\n", __FUNCTION__, mOutputBufferCounter);
    int bufferId = -1;
    for (int i = 0; i < NUMBER_GFX_BUFFERS; i++) {
        if (dstEglImageHandle[i][0] == 0) {
            bufferId = i;
            break;
        }
    }

    if (bufferId == -1) {
        LOG2("Error: maximum output buffer count exceeded\n");
        return (-1);
    } else {
        // note - GraphicBuffer destructor is private, and eglCreateImageKHR will
        // increase the strong ref count. eglDestroyImageKHR will respectively decrease
        // the ref count, resulting in destruction.
        mGraphicBuffer[bufferId] = new GraphicBuffer(width, height,
                getGFXHALPixelFormatFromV4L2Format(PlatformData::getPreviewPixelFormat(mCameraId)),
                GraphicBuffer::USAGE_HW_RENDER | GraphicBuffer::USAGE_SW_READ_OFTEN | GraphicBuffer::USAGE_HW_TEXTURE,
                bytesToPixels(V4L2_PIX_FMT_NV12, bpl), (native_handle_t *)*pBufHandle, 0);
        mEglClientBuffer[bufferId] = (mGraphicBuffer[bufferId])->getNativeBuffer();
        EGLint eglImageAttribsY[] =
        {
                EGL_NATIVE_BUFFER_MULTIPLANE_SEPARATE_IMG,      EGL_TRUE,
                EGL_NATIVE_BUFFER_PLANE_OFFSET_IMG,                     0,
                EGL_NONE,                                                                       EGL_NONE,
        };
        EGLint eglImageAttribsUV[] =
        {
                EGL_NATIVE_BUFFER_MULTIPLANE_SEPARATE_IMG,      EGL_TRUE,
                EGL_NATIVE_BUFFER_PLANE_OFFSET_IMG,                     1,
                EGL_NONE,                                                                       EGL_NONE,
        };
        dstEglImageHandle[bufferId][0] = eglCreateImageKHR_EC(eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT,
                EGL_NATIVE_BUFFER_ANDROID, mEglClientBuffer[bufferId], 0);
        dstEglImageHandle[bufferId][1] = eglCreateImageKHR_EC(eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT,
                EGL_NATIVE_BUFFER_ANDROID, mEglClientBuffer[bufferId], eglImageAttribsY);
        dstEglImageHandle[bufferId][2] = eglCreateImageKHR_EC(eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT,
                EGL_NATIVE_BUFFER_ANDROID, mEglClientBuffer[bufferId], eglImageAttribsUV);
        if (dstEglImageHandle[1] == EGL_NO_IMAGE_KHR || dstEglImageHandle[2] == EGL_NO_IMAGE_KHR)
            ALOGE("eglCreateImageKHR dest failed (err=0x%x)\n", eglGetError());

        glGenRenderbuffers_EC(2, dstRenderBuffers[bufferId]);
        for (int i = 0; i < (MAX_EGL_IMAGE_HANDLE - 1); i++)
        {
            glBindRenderbuffer_EC(GL_RENDERBUFFER, dstRenderBuffers[bufferId][i]);
            glEGLImageTargetRenderbufferStorageOES_EC(GL_RENDERBUFFER, dstEglImageHandle[bufferId][i+1]);
        }
        glGenFramebuffers_EC(2, dstFboNames[bufferId]);
        for (int i = 0; i < (MAX_EGL_IMAGE_HANDLE - 1); i++)
        {
            glBindFramebuffer_EC(GL_FRAMEBUFFER, dstFboNames[bufferId][i]);
            glFramebufferRenderbuffer_EC(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, dstRenderBuffers[bufferId][i]);
            GLenum glError = glCheckFramebufferStatus_EC(GL_FRAMEBUFFER);
            if (glError != GL_FRAMEBUFFER_COMPLETE)
            {
                ALOGE("glCheckFramebufferStatus generated error 0x%x\n", glError);
                return (-1);
            }
            glClear_EC(GL_COLOR_BUFFER_BIT);
        }
        glBindFramebuffer_EC(GL_FRAMEBUFFER, 0);
        return bufferId;
    }
}

void GPUScaler::removeInputBuffer(int bufferId)
{
    mInputBufferCounter--;
    LOG1("@%s input buffer count %d\n", __FUNCTION__, mInputBufferCounter);
    for (int k = 0; k < 3; k++) {
        if (srcEglImageHandle[bufferId][k] == EGL_NO_IMAGE_KHR)
            break;
        eglDestroyImageKHR(mDisplay, srcEglImageHandle[bufferId][k]);
        srcEglImageHandle[bufferId][k] = 0;
    }

    glDeleteTextures(2, srcTexNames[bufferId]);
    srcTexNames[bufferId][0] = srcTexNames[bufferId][1] = 0;
}

void GPUScaler::removeOutputBuffer(int bufferId)
{
    mOutputBufferCounter--;
    LOG1("@%s output buffer count %d\n", __FUNCTION__, mOutputBufferCounter);
    for (int j = 0; j < 3; j++) {
        if (dstEglImageHandle[bufferId][j] == EGL_NO_IMAGE_KHR)
            break;
        eglDestroyImageKHR(mDisplay, dstEglImageHandle[bufferId][j]);
        dstEglImageHandle[bufferId][j] = 0;
    }

    glDeleteFramebuffers(2, dstFboNames[bufferId]);
    dstFboNames[bufferId][0] = dstFboNames[bufferId][1] = 0;
    glDeleteRenderbuffers(2, dstRenderBuffers[bufferId]);
    dstRenderBuffers[bufferId][0] = dstRenderBuffers[bufferId][1] = 0;
}

int GPUScaler::addInputBuffer(buffer_handle_t *pBufHandle, int width, int height, int bpl, int format)
{
    mInputBufferCounter++;
    LOG1("@%s input buffer count %d\n", __FUNCTION__, mInputBufferCounter);

    int bufferId = -1;

    for (int i = 0; i < NUMBER_GFX_BUFFERS; i++) {
        if (srcEglImageHandle[i][0] == 0) {
            bufferId = i;
            break;
        }
    }

    if (bufferId == -1) {
        LOG1("Error: maximum input buffer count exceeded\n");
        return (-1);
    }

    // note - GraphicBuffer destructor is private, and eglCreateImageKHR will
    // increase the strong ref count. eglDestroyImageKHR will respectively decrease
    // the ref count, resulting in destruction.
    GraphicBuffer *graphicBuffer = new GraphicBuffer(width, height,
            getGFXHALPixelFormatFromV4L2Format(PlatformData::getPreviewPixelFormat(mCameraId)),
            GraphicBuffer::USAGE_HW_RENDER | GraphicBuffer::USAGE_SW_WRITE_OFTEN | GraphicBuffer::USAGE_HW_TEXTURE,
            bytesToPixels(V4L2_PIX_FMT_NV12, bpl), (native_handle_t *)*pBufHandle, 0);

    if (graphicBuffer == NULL) {
        ALOGE("Error: out of memory\n");
        return (-1);
    }
    ANativeWindowBuffer *nativeWindow = graphicBuffer->getNativeBuffer();

    EGLint eglImageAttribsY[] =
    {
            EGL_NATIVE_BUFFER_MULTIPLANE_SEPARATE_IMG,      EGL_TRUE,
            EGL_NATIVE_BUFFER_PLANE_OFFSET_IMG,                     0,
            EGL_NONE,                                                                       EGL_NONE,
    };
    EGLint eglImageAttribsUV[] =
    {
            EGL_NATIVE_BUFFER_MULTIPLANE_SEPARATE_IMG,      EGL_TRUE,
            EGL_NATIVE_BUFFER_PLANE_OFFSET_IMG,                     1,
            EGL_NONE,                                                                       EGL_NONE,
    };
    srcEglImageHandle[bufferId][0] = eglCreateImageKHR_EC(eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT,
            EGL_NATIVE_BUFFER_ANDROID, nativeWindow, 0);
    srcEglImageHandle[bufferId][1] = eglCreateImageKHR_EC(eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT,
            EGL_NATIVE_BUFFER_ANDROID, nativeWindow, eglImageAttribsY);
    srcEglImageHandle[bufferId][2] = eglCreateImageKHR_EC(eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT,
            EGL_NATIVE_BUFFER_ANDROID, nativeWindow, eglImageAttribsUV);
    if (srcEglImageHandle[bufferId][1] == EGL_NO_IMAGE_KHR || srcEglImageHandle[bufferId][2] == EGL_NO_IMAGE_KHR)
        ALOGE("eglCreateImageKHR source failed (err=0x%x)\n", eglGetError());

    glGenTextures_EC((MAX_EGL_IMAGE_HANDLE - 1), srcTexNames[bufferId]);
    for (int i = 0; i < (MAX_EGL_IMAGE_HANDLE - 1); i++)
    {
        glBindTexture_EC(GL_TEXTURE_EXTERNAL_OES, srcTexNames[bufferId][i]);
        glTexParameteri_EC(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri_EC(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri_EC(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri_EC(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        //FIXME: Use static bind texture and image will caused some unexpect result.
        //glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, srcEglImageHandle[i + 1]);
    }

    return bufferId;
}

int GPUScaler::initShaders(void) {
    LOG2("@%s\n", __FUNCTION__);

    GLuint fragShader[3] = {0,0,0}, vertShader(0);
    GLint stat(0);
    // Vertex shader quad vertices and texture coordinates
    mVertices[0] = -1.0f;
    mVertices[1] = 1.0f;
    mVertices[2] = 0.0f; // Position 0
    mVertices[3] = 0.0f;
    mVertices[4] = 1.0f; // TexCoord 0
    mVertices[5] = -1.0f;
    mVertices[6] = -1.0f;
    mVertices[7] = 0.0f; // Position 1
    mVertices[8] = 0.0f;
    mVertices[9] = 0.0f; // TexCoord 1
    mVertices[10] = 1.0f;
    mVertices[11] = -1.0f;
    mVertices[12] = 0.0f; // Position 2
    mVertices[13] = 1.0f;
    mVertices[14] = 0.0f; // TexCoord 2
    mVertices[15] = 1.0f;
    mVertices[16] = 1.0f;
    mVertices[17] = 0.0f; // Position 3
    mVertices[18] = 1.0f;
    mVertices[19] = 1.0f; // TexCoord 3

    static const char *vertShaderText = "attribute vec4 a_position;   \n"
            "attribute vec2 a_texCoord;   \n"
            "precision highp float;       \n"
            "varying vec2 v_texCoord;     \n"
            "uniform float mx;            \n"
            "uniform float my;            \n"
            "void main()                  \n"
            "{                            \n"
            "   gl_Position = vec4(a_position.x * mx, a_position.y * my, 0.0, 1.0); \n"
            "   v_texCoord = a_texCoord ;   \n"
            "}                             \n";

    char pInfoLog[8192];
    int llen;
    LOG2("Initializing V0\n");
    vertShader = glCreateShader_EC(GL_VERTEX_SHADER);
    glShaderSource_EC(vertShader, 1, (const char **) &vertShaderText, NULL);
    glCompileShader_EC(vertShader);
    glGetShaderiv_EC(vertShader, GL_COMPILE_STATUS, &stat);
    if (!stat) {
        ALOGE("Error: vertex shader did not compile!\n");
        glGetShaderInfoLog_EC(vertShader, 8192, &llen, pInfoLog);
        ALOGE("%s\n", pInfoLog);
        return (1);
    }
    static const char *fragShaderText = " #extension GL_OES_EGL_image_external : enable \n"
            "precision highp float;	                                \n"
            "varying highp vec2 v_texCoord;   				\n"
            "uniform samplerExternalOES yuvTexture;			\n"
            "void main()							\n"
            "{								\n"
            "	gl_FragColor = texture2D(yuvTexture, v_texCoord);	\n"
            "}								\n";
    LOG2("Initializing S0\n");
    fragShader[2] = glCreateShader_EC(GL_FRAGMENT_SHADER);
    glShaderSource_EC(fragShader[2], 1, (const char **) &fragShaderText, NULL);
    glCompileShader_EC(fragShader[2]);
    glGetShaderiv_EC(fragShader[2], GL_COMPILE_STATUS, &stat);
    if (!stat) {
        ALOGE("Error: fragment shader did not compile!\n");
        glGetShaderInfoLog_EC(fragShader[2], 8192, &llen, pInfoLog);
        ALOGE("%s\n", pInfoLog);
        return (1);
    }
    mP_NV12z = glCreateProgram_EC();
    glAttachShader_EC(mP_NV12z, fragShader[2])
    glAttachShader_EC(mP_NV12z, vertShader);
    glLinkProgram_EC(mP_NV12z);
    glGetProgramInfoLog(mP_NV12z, 8192, &llen, pInfoLog);
    if(llen > 0)
        ALOGE("glLinkProgram log #2 was %s", pInfoLog);

    glGetProgramiv_EC(mP_NV12z, GL_LINK_STATUS, &stat);
    if (!stat) {
        ALOGE("Error linking P2:%d\n", stat);
        return (1);
    }
    mH_vertex_pos[2] = glGetAttribLocation_EC( mP_NV12z, "a_position");
    mH_vertex_texCoord[2] = glGetAttribLocation_EC( mP_NV12z, "a_texCoord");
    mH_mx[0] = glGetUniformLocation_EC(mP_NV12z, "mx");
    mH_my[0] = glGetUniformLocation_EC(mP_NV12z, "my");

    return (0);
}

void GPUScaler::setZoomFactor(float factor) {
    LOG2("set zoom factor: %f", factor);
    glUseProgram_EC(mP_NV12z);
    glUniform1f_EC(mH_mx[0], factor);
    glUniform1f_EC(mH_my[0], factor);
}

void GPUScaler::zslApplyZoom(int iBufferId, int oBufferId)
{
    LOG2("zslApplyZoom, inputBuffer ID:%d, ouputBuffer ID:%d", iBufferId, oBufferId);
    GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
    int outputWidth = ((ANativeWindowBuffer* )mEglClientBuffer[oBufferId])->width;
    int outputHeight = ((ANativeWindowBuffer* )mEglClientBuffer[oBufferId])->height;

    /* Render Y in->out ******************************************************/
    glBindFramebuffer_EC(GL_FRAMEBUFFER, dstFboNames[oBufferId][0]);
    glViewport_EC(0, 0, outputWidth, outputHeight);
    glClearColor_EC(0.0f, 0.0f, 0.0f, 1.0f);
    glClear_EC(GL_COLOR_BUFFER_BIT);
    glUseProgram_EC(mP_NV12z);
    glEnableVertexAttribArray_EC(mH_vertex_pos[2]);
    glEnableVertexAttribArray_EC(mH_vertex_texCoord[2]);
    glVertexAttribPointer_EC(mH_vertex_pos[2], 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), mVertices);
    glBindTexture_EC(GL_TEXTURE_EXTERNAL_OES, srcTexNames[iBufferId][0]);
    glEGLImageTargetTexture2DOES_EC(GL_TEXTURE_EXTERNAL_OES, srcEglImageHandle[iBufferId][1]);
    glVertexAttribPointer_EC(mH_vertex_texCoord[2], 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &mVertices[3]);
    glDrawElements_EC(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
    glDisableVertexAttribArray(mH_vertex_pos[2]);
    glDisableVertexAttribArray(mH_vertex_texCoord[2]);

    /* Render RG in->out *****************************************************/
    glBindFramebuffer_EC(GL_FRAMEBUFFER, dstFboNames[oBufferId][1]);
    glViewport_EC(0, 0, outputWidth >> 1, outputHeight>> 1);
    glClearColor_EC(0.5f, 0.5f, 0.0f, 1.0f);
    glClear_EC(GL_COLOR_BUFFER_BIT);
    glUseProgram_EC(mP_NV12z);
    glEnableVertexAttribArray_EC(mH_vertex_pos[2]);
    glEnableVertexAttribArray_EC(mH_vertex_texCoord[2]);
    glVertexAttribPointer_EC(mH_vertex_pos[2], 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), mVertices);
    glBindTexture_EC(GL_TEXTURE_EXTERNAL_OES, srcTexNames[iBufferId][1]);
    glEGLImageTargetTexture2DOES_EC(GL_TEXTURE_EXTERNAL_OES, srcEglImageHandle[iBufferId][2]);
    glVertexAttribPointer_EC(mH_vertex_texCoord[2], 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &mVertices[3]);
    glDrawElements_EC(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

    //FIXME: If not rebind fb0 or finish here, there will be ghost result left in screen.
    glBindFramebuffer_EC(GL_FRAMEBUFFER, 0);
    //FIXME: Use glFlush will cause gpu hung.
    //glFlush_EC();
    glDisableVertexAttribArray(mH_vertex_pos[2]);
    glDisableVertexAttribArray(mH_vertex_texCoord[2]);
}

int GPUScaler::processFrame(int inputBufferId, int outputBufferId)
{
    LOG2("@%s\n",__FUNCTION__);

    // Start frame rate monitor
    if (mNumFrames == 0 && mZoomConfig.perfMonMode == eOn) {
        gettimeofday(&mTstart, NULL);
    }

    if ((mNumFrames % 30 == 0) && (mZoomConfig.perfMonMode == eOn))
        LOG2("Frame %d %2.1f\n", mNumFrames, mZoomConfig.magFactor);

    // Apply zoom magnification
    zslApplyZoom(inputBufferId, outputBufferId);
    mNumFrames++;
    return 0;
}

} // namespace android

