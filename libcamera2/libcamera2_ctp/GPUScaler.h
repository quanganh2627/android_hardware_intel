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

#ifndef ANDROID_LIBCAMERA_GPUSCALER_H
#define ANDROID_LIBCAMERA_GPUSCALER_H

#include <gui/Surface.h>
#include <GLES/gl.h>
#include <GLES2/gl2.h>

#ifndef EGL_IMG_image_plane_attribs
#define EGL_IMG_image_plane_attribs 1
#define EGL_NATIVE_BUFFER_MULTIPLANE_SEPARATE_IMG 0x3105
#define EGL_NATIVE_BUFFER_PLANE_OFFSET_IMG        0x3106
#endif

namespace android {

#define        MAX_ZOOM_FACTOR                       500.0
#define        MAX_CFG_STRLEN                        512

#define        NUMBER_GFX_BUFFERS      20
#define        MAX_EGL_IMAGE_HANDLE                3                                     // use for r/rg exetension

class GPUScaler {

// constructor destructor
public:
    GPUScaler();
    ~GPUScaler();

// public methods
public:

    void setZoomFactor(float zf);
    int processFrame(int inputBufferId, int outputBufferId);
    int addOutputBuffer(buffer_handle_t *pBufHandle, int width, int height, int bpl);
    int addInputBuffer(buffer_handle_t *pBufHandle, int width, int height, int bpl);
    void removeInputBuffer(int bufferId);
    void removeOutputBuffer(int bufferId);
// private types
private:
    typedef struct {
        int inputMode;
        int outputMode;
        int perfMonMode;
        int zoomMode;
        int magMode;
        float magFactor;
        float magMin, magMax, magStep;
        int magFreq;
        int shaderSrcLoc;
    } zoomConfig_t;

    enum zoomConfigVals {
        eCamera = 0,
        eDisplay,
        eFile,
        eOn,
        eOff,
        eBilinear,
        eBicubic,
        eClient,
        eFixed,
        eLoop,
        eInternal,
        eExternal
    };

// private methods
private:
    status_t initGPU();
    int initShaders();
    int initTexture();
    int setDefaultConfig();
    int logConfig();
    void zslApplyZoom(int iBufferId, int oBufferId);

// private data
private:
    EGLDisplay mDisplay; // EGL display connection
    EGLContext mContext; // EGL context
    EGLSurface mSurface; // EGL surface
    GLuint mT_Y, mT_CbCr; // textures for zoom input: Y, CbCr
    GLuint srcTexNames[NUMBER_GFX_BUFFERS][MAX_EGL_IMAGE_HANDLE - 1]; // Use for ZSL source textures
    GLuint dstRenderBuffers[NUMBER_GFX_BUFFERS][MAX_EGL_IMAGE_HANDLE - 1];
    GLuint dstFboNames[NUMBER_GFX_BUFFERS][MAX_EGL_IMAGE_HANDLE - 1];
    GLuint mT_Yz, mT_CbCrz, mT_NV12z[NUMBER_GFX_BUFFERS]; // textures for zoom output: Y, CbCr, and NV12 out
    GLuint mF_Yz, mF_CbCrz, mF_NV12z[NUMBER_GFX_BUFFERS]; // FBOs for zoom shader outputs: Y, CbCr, and NV12 out
    GLuint mP_Yz, mP_CbCrz, mP_NV12z; // shader programs: Y zoom, CbCr zoom, NV12 output pack
    GLint mH_Y, mH_CbCr, mH_Yz, mH_CbCrz; // uniform handles for frag shader input textures
    GLint mH_mx[4], mH_my[4], mH_bx[4], mH_by[4], mH_ho; // uniform handles for frag shader zoom parameters: mx/bx, my/by = texture coordinate transform coefficients
    GLint mH_vertex_pos[3]; // uniform handles for vertex shader position coordinates (V0, V1, V2)
    GLint mH_vertex_texCoord[3]; // uniform handles for vertex shader texture coordinates (V0, V1, V2)
    int mNumFrames; // frame counter for processFrame
    GLfloat mVertices[20]; // vertices and texture coordinates for vertex shader
    struct timeval mTstart; // fps timer
    zoomConfig_t mZoomConfig, mDefaultZoomConfig; // zoom configuration
    int mInputBufferCounter;
    int mOutputBufferCounter;
    EGLImageKHR mEglImageHandle[NUMBER_GFX_BUFFERS]; // EGL images for direct render to gralloc buffer
    EGLClientBuffer mEglClientBuffer[NUMBER_GFX_BUFFERS]; // EGL client buffers for direct render to gralloc buffer
    EGLImageKHR dstEglImageHandle[NUMBER_GFX_BUFFERS][MAX_EGL_IMAGE_HANDLE];
    EGLImageKHR srcEglImageHandle[NUMBER_GFX_BUFFERS][MAX_EGL_IMAGE_HANDLE];
    GraphicBuffer *mGraphicBuffer[NUMBER_GFX_BUFFERS]; // internal gralloc buffers for direct render example
};
// class GPUScaler

};
// namespace android

#endif // ANDROID_LIBCAMERA_GPUSCALER_H
