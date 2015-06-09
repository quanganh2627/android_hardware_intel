/*
 * Copyright (c) 2014 Intel Corporation.
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

#define LOG_TAG "Camera_GPUWarper"

//#define NO_WARPING
//#define GPU_WARPING_DEBUG

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
#include "GPUWarperEglUtil.h"
#include "GPUWarper.h"
#include "PlatformData.h" // for HAL pixel format
namespace android {

// Shaders sources
static const char *fragmentShaderSourceStY = "#extension GL_OES_EGL_image_external : enable \n"
        "precision highp float;            \n"
        "varying vec2 v_texCoord;          \n"
        "uniform float mx;                 \n"
        "uniform samplerExternalOES tex_Y; \n"
        " \n"
        "void main() \n"
        "{ \n"
        "   float sfg;  \n"
        "   float xt;   \n"
        "   float fr;\n"
        "   sfg = (gl_FragCoord.x-0.5)/4.0; \n"
        "   xt = mx*(floor(sfg)+0.5);       \n"
        "   fr = fract(sfg);             \n"
        "   if(fr<0.25)                                                  \n"
        "      gl_FragColor.r = texture2D(tex_Y, vec2(xt, v_texCoord.t)).r; \n"
        "   else if(fr<0.5)                                              \n"
        "      gl_FragColor.r = texture2D(tex_Y, vec2(xt, v_texCoord.t)).g; \n"
        "   else if(fr<0.75)                                             \n"
        "      gl_FragColor.r = texture2D(tex_Y, vec2(xt, v_texCoord.t)).b; \n"
        "   else \n"
        "      gl_FragColor.r = texture2D(tex_Y, vec2(xt, v_texCoord.t)).a; \n"
        "} \n";

static const char *fragmentShaderSourceStUV = "#extension GL_OES_EGL_image_external : enable \n"
        "precision highp float;             \n"
        "varying vec2 v_texCoord;           \n"
        "uniform float mx;                  \n"
        "uniform samplerExternalOES tex_UV; \n"
        " \n"
        "void main() \n"
        "{ \n"
        "   float sfg;  \n"
        "   float xt;   \n"
        "   float fr;\n"
        "   sfg = (gl_FragCoord.x-0.5)/2.0; \n"
        "   xt = mx*(floor(sfg)+0.5);       \n"
        "   fr = fract(sfg);             \n"
        "   if(fr<0.5)                                                      \n"
        "      gl_FragColor.rg = texture2D(tex_UV, vec2(xt, v_texCoord.t)).rg; \n"
        "   else \n"
        "      gl_FragColor.rg = texture2D(tex_UV, vec2(xt, v_texCoord.t)).ba; \n"
        "} \n";

static const char *fragmentShaderSourceY = "precision highp float; \n"
        "uniform sampler2D tex_Y; \n"
        "varying vec2 v_texCoord; \n"
        " \n"
        "void main() \n"
        "{ \n"
        "   gl_FragColor.r = texture2D(tex_Y, v_texCoord).r; \n"
        "} \n";

static const char *fragmentShaderSourceUV = "precision highp float; \n"
        "uniform sampler2D tex_UV; \n"
        "varying vec2 v_texCoord; \n"
        " \n"
        "void main() \n"
        "{ \n"
        "   gl_FragColor.rg = texture2D(tex_UV, v_texCoord).rg; \n"
        "} \n";

static const char *fragmentShaderSourceNV12 = "precision highp float; \n"
        "uniform sampler2D tex_Y;  \n"
        "uniform sampler2D tex_UV; \n"
        "uniform float mHeight;    \n"
        "uniform float mx_Y;       \n"
        "uniform float mx_UV;      \n"
        "uniform vec4 bx_Y;        \n"
        "uniform vec2 bx_UV;       \n"
        "varying vec2 v_texCoord;  \n"
        " \n"
        "void main()    \n"
        "{ \n"
        "   vec4 xt_Y;  \n"
        "   vec2 xt_UV; \n"
        "   float yt;   \n"
        "   float w;    \n"
        "   vec4 Y;     \n"
        "   vec4 UV;    \n"
        " \n"
        "   // Y \n"
        "   xt_Y = mx_Y*(gl_FragCoord.x-0.5) + bx_Y;  \n"
        "   yt = v_texCoord.t;  \n"
        "   Y.r = texture2D(tex_Y,vec2(xt_Y.r,yt)).r; \n"
        "   Y.g = texture2D(tex_Y,vec2(xt_Y.g,yt)).r; \n"
        "   Y.b = texture2D(tex_Y,vec2(xt_Y.b,yt)).r; \n"
        "   Y.a = texture2D(tex_Y,vec2(xt_Y.a,yt)).r; \n"
        " \n"
        "   // UV \n"
        "   xt_UV = mx_UV*(gl_FragCoord.x-0.5) + bx_UV;    \n"
        "   yt = (v_texCoord.t - 1.0)*2.0;                 \n"
        "   UV.rg = texture2D(tex_UV,vec2(xt_UV.r,yt)).rg; \n"
        "   UV.ba = texture2D(tex_UV,vec2(xt_UV.g,yt)).rg; \n"
        " \n"
        "   // NV12 \n"
        "   w = clamp( sign(gl_FragCoord.y-mHeight+0.5), 0.0, 1.0 ); \n"
        "   gl_FragColor = mix( Y, UV, w );                          \n"
        "} \n";

static const char *vertexShaderSource = "precision highp float; \n"
        "attribute vec4 a_position;   \n"
        "attribute vec2 a_texCoord;   \n "
        "varying vec2 v_texCoord;     \n"
        "void main()                  \n"
        "{                            \n"
        "   gl_Position = a_position; \n"
        "   v_texCoord = a_texCoord ; \n"
        "}";

GPUWarper::GPUWarper(GLuint width, GLuint height, GLuint meshSize) :
        mInFrame(NULL)
        ,mOutFrame(NULL)
        ,mWidth(width)
        ,mHeight(height)
        ,mStride(width)
        ,mMeshSize(meshSize)
        ,mNGridPointsX(0)
        ,mNGridPointsY(0)
        ,mTileExpansionCoeff(0.0f)
        ,mGraphicBufferOut(NULL)
        ,mGraphicBufferInY(NULL)
        ,mGraphicBufferInUV(NULL)
        ,mDisplay(EGL_NO_DISPLAY)
        ,mContext(EGL_NO_CONTEXT)
        ,mSurface(EGL_NO_SURFACE)
        ,mGlVertices(NULL)
        ,mGlIndices(NULL)
        ,mIsInitialized(false)
{

}

status_t GPUWarper::init(){

    status_t status;

    if (mIsInitialized) {
        ALOGI("GPUWarper already initialized");
        return NO_ERROR;
    }

    status = initGPU();
    if (status != NO_ERROR) {
        ALOGE("Failed to initialize GPU");
        return status;
    }

    status = setupWarper();
    if (status != NO_ERROR) {
        ALOGE("Failed to setup GPUWarper");
        return status;
    }

    mIsInitialized = true;
    return NO_ERROR;
}

GPUWarper::~GPUWarper() {
    clearWarper();
    clearGPU();
}

status_t GPUWarper::setupWarper() {

    status_t status;

    // check if picture height exceeds maximally allowed texture size
    if (mMaxTextureSize < mHeight) {
        ALOGE("Failed to setup GPUWarper: input picture height exceeds parameter GL_MAX_TEXTURE_SIZE of the GPU.");
        return INVALID_OPERATION;
    }

    findMeshParameters();

    status = allocateHostArrays();
    if (status != NO_ERROR) return status;

    status = createTextureObjects();
    if (status != NO_ERROR) return status;

    status = initShaders();
    if (status != NO_ERROR) return status;

    status = createInputGraficBuffers();
    if (status != NO_ERROR) return status;

    status = createOutputGraficBuffer();
    if (status != NO_ERROR) return status;

    return NO_ERROR;
}

status_t GPUWarper::initGPU() {

    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLint majorVersion(0);
    EGLint minorVersion(0);
    EGLint w(0);
    EGLint h(0);

    // init EGL
    mDisplay = eglGetDisplay_EC(EGL_DEFAULT_DISPLAY);
    eglInitialize_EC(mDisplay, &majorVersion, &minorVersion);
    eglBindAPI_EC(EGL_OPENGL_ES_API);

    // get native mSurface
    sp<IBinder> dtoken(SurfaceComposerClient::getBuiltInDisplay(ISurfaceComposer::eDisplayIdMain));
    DisplayInfo dinfo;
    status_t status = SurfaceComposerClient::getDisplayInfo(dtoken, &dinfo);
    if (status != NO_ERROR) {
        ALOGE("SurfaceComposerClient::getDisplayInfo failed\n");
        return status;
    }
    sp<SurfaceComposerClient> session = new SurfaceComposerClient();
    if (session == NULL) {
        ALOGE("Failed to create new SurfaceComposerClient");
        return UNKNOWN_ERROR;
    }
    ALOGI("@createSurface()\n");
    sp<SurfaceControl> control = session->createSurface(String8("ULL GPU warping"), dinfo.w, dinfo.h, PIXEL_FORMAT_RGBA_8888);
    ALOGI("@openGlobalTransaction()\n");
    SurfaceComposerClient::openGlobalTransaction();
    ALOGI("@setLayer()\n");
    control->setLayer(0x40000000);
    ALOGI("@closeGlobalTransaction()\n");
    SurfaceComposerClient::closeGlobalTransaction();
    ALOGI("@getSurface\n");
    sp<Surface> s = control->getSurface();

    // finish EGL and GL init
    const EGLint attribs[] = { EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 0, EGL_CONFORMANT, EGL_OPENGL_ES2_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
    EGLConfig config(0);
    EGLint numConfigs(0);

    eglChooseConfig_EC(mDisplay, attribs, &config, 1, &numConfigs);
    mSurface = eglCreateWindowSurface_EC(mDisplay, config, s.get(), NULL);
    if (mSurface == NULL) {
        ALOGE("eglCreateWindowSurface error");
        return UNKNOWN_ERROR;
    }
    mContext = eglCreateContext_EC(mDisplay, config, NULL, context_attribs);
    eglMakeCurrent_EC(mDisplay, mSurface, mSurface, mContext);
    eglQuerySurface_EC(mDisplay, mSurface, EGL_WIDTH, &w);
    eglQuerySurface_EC(mDisplay, mSurface, EGL_HEIGHT, &h);
    ALOGI("EGL window dimensions: %d x %d\n", w, h);
    gpuw_pzEglUtil_printGLString("Version", GL_VERSION);
    gpuw_pzEglUtil_printGLString("Vendor", GL_VENDOR);
    gpuw_pzEglUtil_printGLString("Renderer", GL_RENDERER);
    gpuw_pzEglUtil_printGLString("Extensions", GL_EXTENSIONS);

    GLint max = 1024;
    glGetIntegerv_EC(GL_MAX_TEXTURE_SIZE, &max);
    ALOGI("GL_MAX_TEXTURE_SIZE: %d\n", max);
    mMaxTextureSize = max;
    mMaxTextureSizeX = (max < 4096) ? max : 4096;

    glPixelStorei_EC(GL_PACK_ALIGNMENT, 1);
    glPixelStorei_EC(GL_UNPACK_ALIGNMENT, 1);

    return NO_ERROR;
}

void GPUWarper::clearGPU() {

    eglMakeCurrent_EC(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(mDisplay, mSurface);
    eglDestroyContext(mDisplay, mContext);
    eglTerminate(mDisplay);

}

void GPUWarper::clearWarper() {

    glDeleteProgram(mGlslProgramStY);
    glDeleteProgram(mGlslProgramStUV);
    glDeleteProgram(mGlslProgramY);
    glDeleteProgram(mGlslProgramUV);
    glDeleteProgram(mGlslProgramNV12);

    glDeleteShader(mVertexShader);
    glDeleteShader(mFragmentShaderStY);
    glDeleteShader(mFragmentShaderStUV);
    glDeleteShader(mFragmentShaderY);
    glDeleteShader(mFragmentShaderUV);
    glDeleteShader(mFragmentShaderNV12);

    if (mInEGLImageY != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(mDisplay, mInEGLImageY);
    }
    if (mInEGLImageUV != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(mDisplay, mInEGLImageUV);
    }
    if (mOutEGLImageNV12 != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(mDisplay, mOutEGLImageNV12);
    }

    if (mInTextureY) {
        glDeleteTextures(1, &mInTextureY);
    }
    if (mInTextureUV) {
        glDeleteTextures(1, &mInTextureUV);
    }
    if (mMidTextureY) {
        glDeleteTextures(1, &mMidTextureY);
    }
    if (mMidTextureUV) {
        glDeleteTextures(1, &mMidTextureUV);
    }
    if (mOutTextureY) {
        glDeleteTextures(1, &mOutTextureY);
    }
    if (mOutTextureUV) {
        glDeleteTextures(1, &mOutTextureUV);
    }
    if (mOutTextureNV12) {
        glDeleteTextures(1, &mOutTextureNV12);
    }

    glDeleteFramebuffers(1, &mMidFbY);
    glDeleteFramebuffers(1, &mMidFbUV);
    glDeleteFramebuffers(1, &mOutFbY);
    glDeleteFramebuffers(1, &mOutFbUV);
    glDeleteFramebuffers(1, &mOutFbNV12);

    deleteHostArrays();

}

void GPUWarper::findMeshParameters() {

    mNTilesX = (mWidth - 1) / (mMaxTextureSizeX) + 1;

    // since it is not allowed to process input pictures with height
    // larger than mMaxTextureSize, mNTilesY will always be 1
    mNTilesY = (mHeight - 1) / (mMaxTextureSize) + 1;

    mTileSizeX = (mWidth - 1) / mNTilesX + 1;
    if (mTileSizeX % 2 != 0)
        mTileSizeX += 1;

    mTileSizeY = (mHeight - 1) / mNTilesY + 1;
    if (mTileSizeY % 2 != 0)
        mTileSizeY += 1;

    mNTilesX = (mWidth - 1) / (mTileSizeX) + 1;
    mNTilesY = (mHeight - 1) / (mTileSizeY) + 1;

    mTileExpansionCoeff = 1.2;
    mInBuffWidth = static_cast<double>(mTileSizeX) * mTileExpansionCoeff;
    GLuint diff = mInBuffWidth - mTileSizeX;
    if (diff % 4 != 0)
        mInBuffWidth = mTileSizeX + (diff / 4 + 1) * 4;
    mInBuffHeight = static_cast<double>(mTileSizeY) * mTileExpansionCoeff;
    diff = mInBuffHeight - mTileSizeY;
    if (diff % 4 != 0)
        mInBuffHeight = mTileSizeY + (diff / 4 + 1) * 4;

    if (mNTilesX == 1)
        mInBuffWidth = mWidth;
    if (mNTilesY == 1)
        mInBuffHeight = mHeight;

    mBorderX = (mInBuffWidth - mTileSizeX) / 2;
    mBorderY = (mInBuffHeight - mTileSizeY) / 2;

    mNGridPointsX = (mTileSizeX - 1) / mMeshSize + 2;
    mNGridPointsY = (mTileSizeY - 1) / mMeshSize + 2;

}

status_t GPUWarper::processFrame() {

    status_t status;

    // iterate through tiles
    for (GLuint j = 0; j < mNTilesY; j++) {
        for (GLuint i = 0; i < mNTilesX; i++) {

            // coordinates of the current tile origin
            // obtained from function fillInputGraphicBuffers
            GLuint startX = 0;
            GLuint startY = 0;

            // transfer data from current tile to input textures
            status = fillInputGraphicBuffers(i, j, startX, startY);
            if (status != NO_ERROR) return status;

            // if current tile is not the first tile in the row
            // transfer data from output GraphicBuffer to previous output tile
            if (i > 0) {
                status = readOutputGraphicBuffer(i - 1, j);
                if (status != NO_ERROR) return status;
            }

            ///////// Y
            RGBATexToREDorRG(mInTextureY, GL_TEXTURE0, mInEGLImageY, mMidFbY, mInBuffWidth, mInBuffHeight, mGlslProgramStY, mVertexPosStY, mVertexTexCoordStY);
            ///////// UV
            RGBATexToREDorRG(mInTextureUV, GL_TEXTURE1, mInEGLImageUV, mMidFbUV, mInBuffWidth / 2, mInBuffHeight / 2, mGlslProgramStUV, mVertexPosStUV, mVertexTexCoordStUV);

            // creates mesh (triangles to be rendered) for current input tile
            meshTileBackward(i, j, startX, startY);

            ///////// Y
            warping(mMidTextureY, GL_TEXTURE2, mOutFbY, mTileSizeX, mTileSizeY, mGlslProgramY, mVertexPosY, mVertexTexCoordY);
            ///////// Y
            warping(mMidTextureUV, GL_TEXTURE3, mOutFbUV, mTileSizeX / 2, mTileSizeY / 2, mGlslProgramUV, mVertexPosUV, mVertexTexCoordUV);

            combYandUVTexsIntoNV12();

            glFinish();

        }

        // transfer data from output GraphicBuffer to the last output tile in the row
        status = readOutputGraphicBuffer(mNTilesX - 1, j);
        if (status != NO_ERROR) return status;

    }

    return NO_ERROR;
}

void GPUWarper::warping(GLuint iTexID, GLenum actTex, GLint fb, GLint w, GLint h, GLint glslProgram, GLint vertex_pos, GLint vertex_texCoord) {

#ifdef NO_WARPING
    mGlVertices[0] = -1.0f;
    mGlVertices[1] = 1.0f;
    mGlVertices[2] = 0.0f; // Position 0
    mGlVertices[3] = 0.0f;
    mGlVertices[4] = 1.0f;// TexCoord 0
    mGlVertices[5] = -1.0f;
    mGlVertices[6] = -1.0f;
    mGlVertices[7] = 0.0f;// Position 1
    mGlVertices[8] = 0.0f;
    mGlVertices[9] = 0.0f;// TexCoord 1
    mGlVertices[10] = 1.0f;
    mGlVertices[11] = -1.0f;
    mGlVertices[12] = 0.0f;// Position 2
    mGlVertices[13] = 1.0f;
    mGlVertices[14] = 0.0f;// TexCoord 2
    mGlVertices[15] = 1.0f;
    mGlVertices[16] = 1.0f;
    mGlVertices[17] = 0.0f;// Position 3
    mGlVertices[18] = 1.0f;
    mGlVertices[19] = 1.0f;// TexCoord 3

    mGlIndices[0] = 0;
    mGlIndices[1] = 1;
    mGlIndices[2] = 2;
    mGlIndices[3] = 0;
    mGlIndices[4] = 2;
    mGlIndices[5] = 3;
#endif

    glBindFramebuffer_EC(GL_FRAMEBUFFER, fb);
    glViewport_EC(0, 0, w, h);
    glClearColor_EC(0.0f, 0.0f, 0.0f, 1.0f);
    glClear_EC(GL_COLOR_BUFFER_BIT);
    glUseProgram_EC(glslProgram);
    glActiveTexture_EC(actTex);
    glBindTexture_EC(GL_TEXTURE_2D, iTexID);
    glEnableVertexAttribArray_EC(vertex_pos);
    glEnableVertexAttribArray_EC(vertex_texCoord);
    glVertexAttribPointer_EC(vertex_pos, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), mGlVertices);
    glVertexAttribPointer_EC(vertex_texCoord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &mGlVertices[3]);

#ifdef NO_WARPING
    glDrawElements_EC(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, mGlIndices);
#else
    glDrawElements_EC(GL_TRIANGLES, (mNGridPointsX - 1) * (mNGridPointsY - 1) * 2 * 3, GL_UNSIGNED_SHORT, mGlIndices);
#endif

    glDisableVertexAttribArray(vertex_pos);
    glDisableVertexAttribArray(vertex_texCoord);
}

void GPUWarper::combYandUVTexsIntoNV12() {

    mGlVertices[0] = -1.0f;
    mGlVertices[1] = 1.0f;
    mGlVertices[2] = 0.0f; // Position 0
    mGlVertices[3] = 0.0f;
    mGlVertices[4] = 1.5f; // TexCoord 0
    mGlVertices[5] = -1.0f;
    mGlVertices[6] = -1.0f;
    mGlVertices[7] = 0.0f; // Position 1
    mGlVertices[8] = 0.0f;
    mGlVertices[9] = 0.0f; // TexCoord 1
    mGlVertices[10] = 1.0f;
    mGlVertices[11] = -1.0f;
    mGlVertices[12] = 0.0f; // Position 2
    mGlVertices[13] = 1.0f;
    mGlVertices[14] = 0.0f; // TexCoord 2
    mGlVertices[15] = 1.0f;
    mGlVertices[16] = 1.0f;
    mGlVertices[17] = 0.0f; // Position 3
    mGlVertices[18] = 1.0f;
    mGlVertices[19] = 1.5f; // TexCoord 3

    mGlIndices[0] = 0;
    mGlIndices[1] = 1;
    mGlIndices[2] = 2;
    mGlIndices[3] = 0;
    mGlIndices[4] = 2;
    mGlIndices[5] = 3;

    glBindFramebuffer_EC(GL_FRAMEBUFFER, mOutFbNV12);
    glViewport_EC(0, 0, mTileSizeX / 4, 3 * mTileSizeY / 2);
    glClearColor_EC(0.0f, 0.0f, 0.0f, 1.0f);
    glClear_EC(GL_COLOR_BUFFER_BIT);
    glUseProgram_EC(mGlslProgramNV12);
    glActiveTexture_EC(GL_TEXTURE4);
    glBindTexture_EC(GL_TEXTURE_2D, mOutTextureY);
    glActiveTexture_EC(GL_TEXTURE5);
    glBindTexture_EC(GL_TEXTURE_2D, mOutTextureUV);
    glEnableVertexAttribArray_EC(mVertexPosNV12);
    glEnableVertexAttribArray_EC(mVertexTexCoordNV12);
    glVertexAttribPointer_EC(mVertexPosNV12, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), mGlVertices);
    glVertexAttribPointer_EC(mVertexTexCoordNV12, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &mGlVertices[3]);

    glDrawElements_EC(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, mGlIndices);

    glDisableVertexAttribArray(mVertexPosNV12);
    glDisableVertexAttribArray(mVertexTexCoordNV12);

}

status_t GPUWarper::createOutputGraficBuffer() {

    mGraphicBufferOut = new GraphicBuffer(mTileSizeX / 4, 3 * mTileSizeY / 2, PIXEL_FORMAT_RGBA_8888,
            GraphicBuffer::USAGE_HW_RENDER | GraphicBuffer::USAGE_SW_READ_OFTEN | GraphicBuffer::USAGE_HW_TEXTURE);
    if (!mGraphicBufferOut) {
        ALOGE("Error: creating output buffer\n");
        return UNKNOWN_ERROR;
    }

    mOutGrBuffStride = mGraphicBufferOut->getStride();

    EGLClientBuffer buffer = mGraphicBufferOut->getNativeBuffer();

    if (!buffer) {
        ALOGE("Error: get native buffer from output buffer\n");
        return UNKNOWN_ERROR;
    }

    EGLint eglImageAttributes[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE };
    mOutEGLImageNV12 = eglCreateImageKHR_EC(mDisplay, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, buffer, eglImageAttributes);

    if (mOutEGLImageNV12 == EGL_NO_IMAGE_KHR) {
        ALOGE("eglCreateImageKHR dest failed (err=0x%x)\n", eglGetError());
        return UNKNOWN_ERROR;
    }

    glBindTexture_EC(GL_TEXTURE_2D, mOutTextureNV12);
    glEGLImageTargetTexture2DOES_EC(GL_TEXTURE_2D, mOutEGLImageNV12);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer_EC(GL_FRAMEBUFFER, mOutFbNV12);
    glFramebufferTexture2D_EC(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mOutTextureNV12, 0);
    glClear_EC(GL_COLOR_BUFFER_BIT);
    GLenum glError = glCheckFramebufferStatus_EC(GL_FRAMEBUFFER);
    if (glError != GL_FRAMEBUFFER_COMPLETE) {
        ALOGE("glCheckFramebufferStatus generated error 0x%x\n", glError);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

void GPUWarper::getProjTexture(GLfloat *in, GLfloat *out) {

    double out0 = (in[0] * mProjective[0][0] + in[1] * mProjective[0][1] + mProjective[0][2])
            / (in[0] * mProjective[2][0] + in[1] * mProjective[2][1] + mProjective[2][2]);
    double out1 = (in[0] * mProjective[1][0] + in[1] * mProjective[1][1] + mProjective[1][2])
            / (in[0] * mProjective[2][0] + in[1] * mProjective[2][1] + mProjective[2][2]);

    // ww have to map [0, 0], [mWidth, mHeight]  to [0, 0] , [1, 1] coordinates
    out[0] = out0 / mWidth;
    out[1] = out1 / mHeight;
}

void GPUWarper::meshTileBackward(GLuint indexX, GLuint indexY, GLuint startX, GLuint startY) {

    float originalTexturePosition[3];

    // points initialization
    double step_x_v = 2. * (static_cast<double>(mMeshSize) / static_cast<double>(mTileSizeX));
    double step_y_v = 2. * (static_cast<double>(mMeshSize) / static_cast<double>(mTileSizeY));
    double step_x_t = static_cast<double>(mMeshSize);
    double step_y_t = static_cast<double>(mMeshSize);

    float x_coord_t_start = indexX * mTileSizeX;

    // if tiles are not equal (last tile in row or column can be smaler)
    // in order to have all tiles equal, origin of the tile is moved to the left and/or up
    // this means that some parts of output picture are calculated more than ones
    // TODO: currently (for tested picture dimenstions) this is not preformance penalty,
    // because all tiles are equal
    // but should be checked with new picture dimesions

    if (indexX == (mNTilesX - 1)) {
        x_coord_t_start = mWidth - mTileSizeX;
    }

    float y_coord_t_start = indexY * mTileSizeY;
    if (indexY == (mNTilesY - 1)) {
        y_coord_t_start = mHeight - mTileSizeY;
    }

    for (int j = 0; j < mNGridPointsY; j++) {
        float y_coord_v = -1.0 + j * step_y_v;
        float y_coord_t = y_coord_t_start + j * step_y_t;
        if (j == (mNGridPointsY - 1)) {
            y_coord_v = 1;
            y_coord_t = y_coord_t_start + mTileSizeY;
        }
        int y_pos = j * mNGridPointsX;
        originalTexturePosition[1] = y_coord_t;
        originalTexturePosition[2] = 1.0f;
        for (int i = 0; i < mNGridPointsX; i++) {
            float x_coord_v = -1.0 + i * step_x_v;
            float x_coord_t = x_coord_t_start + i * step_x_t;
            if (i == (mNGridPointsX - 1)) {
                x_coord_v = 1;
                x_coord_t = x_coord_t_start + mTileSizeX;
            }
            float *first = mGlVertices + (y_pos + i) * 5;
            first[0] = x_coord_v;
            first[1] = y_coord_v;
            first[2] = 0.0f;
            originalTexturePosition[0] = x_coord_t;
            getProjTexture(originalTexturePosition, first + 3);

            // obtained coordinates that are global for the input picture
            // should be changed having in mind that
            // coordinates of the input texture for current tile also go from [0, 0] to [1, 1]
            first[3] = (static_cast<double>(first[3]) - (static_cast<double>(startX) / mWidth)) * static_cast<double>(mWidth) / mInBuffWidth;
            first[4] = (static_cast<double>(first[4]) - (static_cast<double>(startY) / mHeight)) * static_cast<double>(mHeight) / mInBuffHeight;
        }
    }

    // calculating triangles for current tile
    GLushort index1 = 0;
    GLushort index2 = 0;
    GLushort index3 = 0;
    GLushort index4 = 0;
    int triangleIndex = 0;
    for (int j = 0; j < (mNGridPointsY - 1); j++) {
        for (int i = 0; i < (mNGridPointsX - 1); i++) {
            index1 = j * mNGridPointsX + i;
            index2 = index1 + 1;
            index3 = (j + 1) * mNGridPointsX + i;
            index4 = index3 + 1;
            mGlIndices[triangleIndex++] = index1;
            mGlIndices[triangleIndex++] = index4;
            mGlIndices[triangleIndex++] = index2;
            mGlIndices[triangleIndex++] = index1;
            mGlIndices[triangleIndex++] = index4;
            mGlIndices[triangleIndex++] = index3;
        }
    }

}

status_t GPUWarper::readOutputGraphicBuffer(GLuint indexX, GLuint indexY) {

    GLuint x_start = indexX * mTileSizeX;
    if (indexX == (mNTilesX - 1)) {
        x_start = mWidth - mTileSizeX;
    }
    GLuint y_start = indexY * mTileSizeY;
    if (indexY == (mNTilesY - 1)) {
        y_start = mHeight - mTileSizeY;
    }

    GLubyte *y = mOutFrame + y_start * mStride + x_start;
    GLubyte *uv = mOutFrame + mStride * mHeight + (y_start / 2) * mStride + x_start;
    GLubyte *pointerOut = NULL;

    mGraphicBufferOut->lock(GraphicBuffer::USAGE_SW_READ_OFTEN, (void**) &pointerOut);

    if (!pointerOut) {
        ALOGE("Error: getting buffer address from output buffer\n");
        return UNKNOWN_ERROR;
    }

    for (GLuint i = 0; i < mTileSizeY; i++) {
        memcpy(y, pointerOut, mTileSizeX);
        pointerOut += mOutGrBuffStride * 4;
        y += mStride;
    }
    for (GLuint i = 0; i < mTileSizeY / 2; i++) {
        memcpy(uv, pointerOut, mTileSizeX);
        pointerOut += mOutGrBuffStride * 4;
        uv += mStride;
    }

    mGraphicBufferOut->unlock();

    return NO_ERROR;
}

status_t GPUWarper::createTextureObjects() {

    // Input textures
    glGenTextures_EC(1, &mInTextureY);
    glGenTextures_EC(1, &mInTextureUV);

    // Render textures
    glGenTextures_EC(1, &mMidTextureY);
    glGenTextures_EC(1, &mMidTextureUV);
    glGenTextures_EC(1, &mOutTextureY);
    glGenTextures_EC(1, &mOutTextureUV);
    glGenTextures_EC(1, &mOutTextureNV12);

    // FBOs
    glGenFramebuffers_EC(1, &mMidFbY);
    glGenFramebuffers_EC(1, &mMidFbUV);
    glGenFramebuffers_EC(1, &mOutFbY);
    glGenFramebuffers_EC(1, &mOutFbUV);
    glGenFramebuffers_EC(1, &mOutFbNV12);

    // Y mid
    glActiveTexture_EC(GL_TEXTURE2);
    glBindTexture_EC(GL_TEXTURE_2D, mMidTextureY);
    glTexImage2D_EC(GL_TEXTURE_2D, 0, GL_R8_EXT, mInBuffWidth, mInBuffHeight, 0, GL_RED_EXT, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer_EC(GL_FRAMEBUFFER, mMidFbY);
    glFramebufferTexture2D_EC(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mMidTextureY, 0);
    glClearColor_EC(0.0f, 0.0f, 0.0f, 1.0f);
    glClear_EC(GL_COLOR_BUFFER_BIT);

    // UV mid
    glActiveTexture_EC(GL_TEXTURE3);
    glBindTexture_EC(GL_TEXTURE_2D, mMidTextureUV);
    glTexImage2D_EC(GL_TEXTURE_2D, 0, GL_RG8_EXT, mInBuffWidth / 2, mInBuffHeight / 2, 0, GL_RG_EXT, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer_EC(GL_FRAMEBUFFER, mMidFbUV);
    glFramebufferTexture2D_EC(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mMidTextureUV, 0);
    glClearColor_EC(0.0f, 0.0f, 0.0f, 1.0f);
    glClear_EC(GL_COLOR_BUFFER_BIT);

    // Y out
    glActiveTexture_EC(GL_TEXTURE4);
    glBindTexture_EC(GL_TEXTURE_2D, mOutTextureY);
    glTexImage2D_EC(GL_TEXTURE_2D, 0, GL_R8_EXT, mTileSizeX, mTileSizeY, 0, GL_RED_EXT, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer_EC(GL_FRAMEBUFFER, mOutFbY);
    glFramebufferTexture2D_EC(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mOutTextureY, 0);
    glClearColor_EC(0.0f, 0.0f, 0.0f, 1.0f);
    glClear_EC(GL_COLOR_BUFFER_BIT);

    // UV out
    glActiveTexture_EC(GL_TEXTURE5);
    glBindTexture_EC(GL_TEXTURE_2D, mOutTextureUV);
    glTexImage2D_EC(GL_TEXTURE_2D, 0, GL_RG8_EXT, mTileSizeX / 2, mTileSizeY / 2, 0, GL_RG_EXT, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri_EC(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer_EC(GL_FRAMEBUFFER, mOutFbUV);
    glFramebufferTexture2D_EC(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mOutTextureUV, 0);
    glClearColor_EC(0.0f, 0.0f, 0.0f, 1.0f);
    glClear_EC(GL_COLOR_BUFFER_BIT);

    return NO_ERROR;
}

status_t GPUWarper::compileShader(const char **source, GLenum type, GLuint &shader) {

    status_t status = NO_ERROR;

    shader = glCreateShader(type);
    glShaderSource_EC(shader, 1, source, NULL);
    glCompileShader_EC(shader);

    char pInfoLog[MAX_SH_INFO_LOG_SIZE];
    int llen;
    GLint stat(0);
    glGetShaderiv_EC(shader, GL_COMPILE_STATUS, &stat);
    if (!stat) {
        ALOGE("Error: shader did not compile!\n");
        glGetShaderInfoLog_EC(shader, MAX_SH_INFO_LOG_SIZE, &llen, pInfoLog);
        ALOGE("%s\n", pInfoLog);
        status = UNKNOWN_ERROR;
    }

    return status;
}

status_t GPUWarper::createProgram(const char **fragmentShaderSource, GLuint &fragmentShader, GLuint &program) {

    status_t status = NO_ERROR;

    program = glCreateProgram();
    glAttachShader_EC(program, mVertexShader);

    status = compileShader(fragmentShaderSource, GL_FRAGMENT_SHADER, fragmentShader);
    if (status != NO_ERROR) {
        ALOGE("Failed to compile fragment shader\n");
        return status;
    }
    glAttachShader_EC(program, fragmentShader);

    glLinkProgram_EC(program);

    char pInfoLog[MAX_SH_INFO_LOG_SIZE];
    int llen;
    GLint stat(0);
    glGetProgramInfoLog(program, MAX_SH_INFO_LOG_SIZE, &llen, pInfoLog);
    if (llen > 0)
        ALOGE("glLinkProgram log was %s", pInfoLog);

    glGetProgramiv_EC(program, GL_LINK_STATUS, &stat);
    if (!stat) {
        ALOGE("Error linking program:%d\n", stat);
        status = UNKNOWN_ERROR;
    }

    return status;
}

status_t GPUWarper::initShaders() {

    status_t status = NO_ERROR;

    // vertex shader
    status = compileShader(static_cast<const char **>(&vertexShaderSource), GL_VERTEX_SHADER, mVertexShader);
    if (status != NO_ERROR) {
        ALOGE("Failed to compile vertex shader\n");
        return status;
    }

    // Y St program
    status = createProgram(static_cast<const char **>(&fragmentShaderSourceStY), mFragmentShaderStY, mGlslProgramStY);
    if (status != NO_ERROR) {
        ALOGE("Failed to create program mGlslProgramStY\n");
        return status;
    }

    mVertexPosStY = glGetAttribLocation_EC(mGlslProgramStY, "a_position");
    mVertexTexCoordStY = glGetAttribLocation_EC(mGlslProgramStY, "a_texCoord");
    GLint fsYinTextureStY = glGetUniformLocation_EC(mGlslProgramStY, "tex_Y");
    GLint mxStY = glGetUniformLocation_EC(mGlslProgramStY, "mx");

    // UV St program
    status = createProgram(static_cast<const char **>(&fragmentShaderSourceStUV), mFragmentShaderStUV, mGlslProgramStUV);
    if (status != NO_ERROR) {
        ALOGE("Failed to create program mGlslProgramStUV\n");
        return status;
    }

    mVertexPosStUV = glGetAttribLocation_EC(mGlslProgramStUV, "a_position");
    mVertexTexCoordStUV = glGetAttribLocation_EC(mGlslProgramStUV, "a_texCoord");
    GLint fsUVinTextureStUV = glGetUniformLocation_EC(mGlslProgramStUV, "tex_UV");
    GLint mxStUV = glGetUniformLocation_EC(mGlslProgramStUV, "mx");

    // Y warping program
    status = createProgram(static_cast<const char **>(&fragmentShaderSourceY), mFragmentShaderY, mGlslProgramY);
    if (status != NO_ERROR) {
        ALOGE("Failed to create program mGlslProgramY\n");
        return status;
    }

    mVertexPosY = glGetAttribLocation_EC(mGlslProgramY, "a_position");
    mVertexTexCoordY = glGetAttribLocation_EC(mGlslProgramY, "a_texCoord");
    GLint fsYinTextureY = glGetUniformLocation_EC(mGlslProgramY, "tex_Y");

    // UV warping program
    status = createProgram(static_cast<const char **>(&fragmentShaderSourceUV), mFragmentShaderUV, mGlslProgramUV);
    if (status != NO_ERROR) {
        ALOGE("Failed to create program mGlslProgramUV\n");
        return status;
    }

    mVertexPosUV = glGetAttribLocation_EC(mGlslProgramUV, "a_position");
    mVertexTexCoordUV = glGetAttribLocation_EC(mGlslProgramUV, "a_texCoord");
    GLint fsUVinTextureUV = glGetUniformLocation_EC(mGlslProgramUV, "tex_UV");

    // Program used to combine Y and UV textures into NV12 texture
    status = createProgram(static_cast<const char **>(&fragmentShaderSourceNV12), mFragmentShaderNV12, mGlslProgramNV12);
    if (status != NO_ERROR) {
        ALOGE("Failed to create program mGlslProgramNV12\n");
        return status;
    }

    mVertexPosNV12 = glGetAttribLocation_EC(mGlslProgramNV12, "a_position");
    mVertexTexCoordNV12 = glGetAttribLocation_EC(mGlslProgramNV12, "a_texCoord");

    GLint fsNV12inTextureY = glGetUniformLocation_EC(mGlslProgramNV12, "tex_Y");
    GLint fsNV12inTextureUV = glGetUniformLocation_EC(mGlslProgramNV12, "tex_UV");

    GLint h = glGetUniformLocation_EC(mGlslProgramNV12, "mHeight");

    GLint mxY = glGetUniformLocation_EC(mGlslProgramNV12, "mx_Y");
    GLint mxUV = glGetUniformLocation_EC(mGlslProgramNV12, "mx_UV");

    GLint bxY = glGetUniformLocation_EC(mGlslProgramNV12, "bx_Y");
    GLint bxUV = glGetUniformLocation_EC(mGlslProgramNV12, "bx_UV");

    // setup shader variables
    glUseProgram_EC(mGlslProgramStY);
    glUniform1i_EC(fsYinTextureStY, 0);
    GLfloat step = (GLfloat)(1.0 / ((GLfloat) mInBuffWidth / 4.0));

    GLfloat m = step;
    glUniform1f_EC(mxStY, m);

    glUseProgram_EC(mGlslProgramStUV);
    glUniform1i_EC(fsUVinTextureStUV, 1);
    step = (GLfloat)(1.0 / ((GLfloat) mInBuffWidth / 4.0));

    m = step;
    glUniform1f_EC(mxStUV, m);

    glUseProgram_EC(mGlslProgramY);
    glUniform1i_EC(fsYinTextureY, 2);

    glUseProgram_EC(mGlslProgramUV);
    glUniform1i_EC(fsUVinTextureUV, 3);

    glUseProgram_EC(mGlslProgramNV12);
    glUniform1i_EC(fsNV12inTextureY, 4);
    glUniform1i_EC(fsNV12inTextureUV, 5);

    step = (GLfloat)(1.0 / ((GLfloat) mTileSizeX));

    m = 4.0 * step;
    glUniform1f_EC(mxY, m);
    glUniform4f_EC(bxY, 0.5 * step, 1.5 * step, 2.5 * step, 3.5 * step);

    step = (GLfloat)(1.0 / ((GLfloat) mTileSizeX / 2.0));
    m = 2.0 * step;
    glUniform1f_EC(mxUV, m);
    glUniform2f_EC(bxUV, 0.5 * step, 1.5 * step);

    glUniform1f_EC(h, (GLfloat ) mTileSizeY);

    return NO_ERROR;
}

status_t GPUWarper::warpBackFrame(AtomBuffer *frame, double projective[PROJ_MTRX_DIM][PROJ_MTRX_DIM]) {
    status_t status = NO_ERROR;

    if (!mIsInitialized) {
        status = init();
        if(status != NO_ERROR) return status;
    }

    mStride = frame->bpl;
    status = updateFrameDimensions(frame->width, frame->height);
    if (status != NO_ERROR) {
        return status;
    }

    mInFrame = (GLubyte *) frame->dataPtr;
    // the same buffer is used to store warped frame
    mOutFrame = (GLubyte *) frame->dataPtr;

    for (int i = 0; i < PROJ_MTRX_DIM; i++) {
        for (int j = 0; j < PROJ_MTRX_DIM; j++) {
            mProjective[i][j] = projective[i][j];
        }
    }

    status = processFrame();

    return status;
}

status_t GPUWarper::allocateHostArrays() {

    status_t status = NO_ERROR;

    mGlVertices = new GLfloat[mNGridPointsX * mNGridPointsY * 5]; // 3 for vertex coordinates, and 2 for texture cordinates
    mGlIndices = new GLushort[(mNGridPointsX - 1) * (mNGridPointsY - 1) * 2 * 3]; // 2 triangles per quad, and 3 vertices per triangle
    if (mGlVertices == NULL || mGlIndices == NULL) {
        ALOGE("Host memory allocation failed\n");
        delete[] mGlVertices;
        delete[] mGlIndices;
        status = NO_MEMORY;
    }
    return status;
}

void GPUWarper::deleteHostArrays(){
    delete[] mGlVertices;
    delete[] mGlIndices;
}

status_t GPUWarper::createInputGraficBuffers() {

    // Y
    mGraphicBufferInY = new GraphicBuffer(mInBuffWidth / 4, mInBuffHeight, PIXEL_FORMAT_RGBA_8888,
            GraphicBuffer::USAGE_HW_RENDER | GraphicBuffer::USAGE_SW_WRITE_OFTEN | GraphicBuffer::USAGE_HW_TEXTURE);
    if (!mGraphicBufferInY) {
        ALOGE("Error: creating input buffer\n");
        return UNKNOWN_ERROR;
    }

    mInGrBuffStride = mGraphicBufferInY->getStride();

    EGLClientBuffer bufferY = mGraphicBufferInY->getNativeBuffer();

    if (!bufferY) {
        ALOGE("Error: get native buffer from input buffer\n");
        return UNKNOWN_ERROR;
    }

    EGLint eglImageAttributes[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE };
    mInEGLImageY = eglCreateImageKHR_EC(mDisplay, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, bufferY, eglImageAttributes);

    if (mInEGLImageY == EGL_NO_IMAGE_KHR) {
        ALOGE("eglCreateImageKHR source failed (err=0x%x)\n", eglGetError());
        return UNKNOWN_ERROR;
    }

    glActiveTexture_EC(GL_TEXTURE0);
    glBindTexture_EC(GL_TEXTURE_EXTERNAL_OES, mInTextureY);
    glTexParameteri_EC(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri_EC(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri_EC(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri_EC(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // UV
    mGraphicBufferInUV = new GraphicBuffer(mInBuffWidth / 4, mInBuffHeight / 2, PIXEL_FORMAT_RGBA_8888,
            GraphicBuffer::USAGE_HW_RENDER | GraphicBuffer::USAGE_SW_WRITE_OFTEN | GraphicBuffer::USAGE_HW_TEXTURE);
    if (!mGraphicBufferInUV) {
        ALOGE("Error: creating output buffer\n");
        return UNKNOWN_ERROR;
    }

    EGLClientBuffer bufferUV = mGraphicBufferInUV->getNativeBuffer();

    if (!bufferUV) {
        ALOGE("Error: get native buffer from input buffer\n");
        return UNKNOWN_ERROR;
    }

    mInEGLImageUV = eglCreateImageKHR_EC(mDisplay, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, bufferUV, eglImageAttributes);

    if (mInEGLImageUV == EGL_NO_IMAGE_KHR) {
        ALOGE("eglCreateImageKHR source failed (err=0x%x)\n", eglGetError());
        return UNKNOWN_ERROR;
    }

    glActiveTexture_EC(GL_TEXTURE1);
    glBindTexture_EC(GL_TEXTURE_EXTERNAL_OES, mInTextureUV);
    glTexParameteri_EC(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri_EC(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri_EC(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri_EC(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return NO_ERROR;
}

status_t GPUWarper::fillInputGraphicBuffers(GLuint indexX, GLuint indexY, GLuint &startX, GLuint &startY) {

    if (indexX == 0)
        startX = 0;
    else
        startX = indexX * mTileSizeX - mBorderX;
    if ((startX + mInBuffWidth) > mWidth)
        startX = mWidth - mInBuffWidth;

    if (indexY == 0)
        startY = 0;
    else
        startY = indexY * mTileSizeY - mBorderY;
    if ((startY + mInBuffHeight) > mHeight)
        startY = mHeight - mInBuffHeight;

    GLubyte *y = mInFrame + startY * mStride + startX;
    GLubyte *uv = mInFrame + mStride * mHeight + (startY / 2) * mStride + startX;

    GLubyte *pointerIn = NULL;

    mGraphicBufferInY->lock(GraphicBuffer::USAGE_SW_WRITE_OFTEN, (void**) &pointerIn);

    if (!pointerIn) {
        ALOGE("Error: getting buffer address from input buffer\n");
        return UNKNOWN_ERROR;
    }

    for (GLuint i = 0; i < mInBuffHeight; i++) {
        memcpy(pointerIn, y, mInBuffWidth);
        pointerIn += mInGrBuffStride * 4;
        y += mStride;
    }

    mGraphicBufferInY->unlock();

    mGraphicBufferInUV->lock(GraphicBuffer::USAGE_SW_WRITE_OFTEN, (void**) &pointerIn);

    if (!pointerIn) {
        ALOGE("Error: getting buffer address from input buffer\n");
        return UNKNOWN_ERROR;
    }

    for (GLuint i = 0; i < mInBuffHeight / 2; i++) {
        memcpy(pointerIn, uv, mInBuffWidth);
        pointerIn += mInGrBuffStride * 4;
        uv += mStride;
    }

    mGraphicBufferInUV->unlock();

    return NO_ERROR;
}

void GPUWarper::RGBATexToREDorRG(GLuint iTexID, GLenum actTex, EGLImageKHR image, GLint fb, GLint w, GLint h, GLint glslProgram, GLint vertex_pos,
        GLint vertex_texCoord) {

    mGlVertices[0] = -1.0f;
    mGlVertices[1] = 1.0f;
    mGlVertices[2] = 0.0f; // Position 0
    mGlVertices[3] = 0.0f;
    mGlVertices[4] = 1.0f; // TexCoord 0
    mGlVertices[5] = -1.0f;
    mGlVertices[6] = -1.0f;
    mGlVertices[7] = 0.0f; // Position 1
    mGlVertices[8] = 0.0f;
    mGlVertices[9] = 0.0f; // TexCoord 1
    mGlVertices[10] = 1.0f;
    mGlVertices[11] = -1.0f;
    mGlVertices[12] = 0.0f; // Position 2
    mGlVertices[13] = 1.0f;
    mGlVertices[14] = 0.0f; // TexCoord 2
    mGlVertices[15] = 1.0f;
    mGlVertices[16] = 1.0f;
    mGlVertices[17] = 0.0f; // Position 3
    mGlVertices[18] = 1.0f;
    mGlVertices[19] = 1.0f; // TexCoord 3

    mGlIndices[0] = 0;
    mGlIndices[1] = 1;
    mGlIndices[2] = 2;
    mGlIndices[3] = 0;
    mGlIndices[4] = 2;
    mGlIndices[5] = 3;

    glBindFramebuffer_EC(GL_FRAMEBUFFER, fb);
    glBindTexture_EC(GL_TEXTURE_2D, 0);
    glViewport_EC(0, 0, w, h);
    glClearColor_EC(0.0f, 0.0f, 0.0f, 1.0f);
    glClear_EC(GL_COLOR_BUFFER_BIT);
    glUseProgram_EC(glslProgram);
    glActiveTexture_EC(actTex);
    glBindTexture_EC(GL_TEXTURE_EXTERNAL_OES, iTexID);
    glEGLImageTargetTexture2DOES_EC(GL_TEXTURE_EXTERNAL_OES, image);
    glEnableVertexAttribArray_EC(vertex_pos);
    glEnableVertexAttribArray_EC(vertex_texCoord);
    glVertexAttribPointer_EC(vertex_pos, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), mGlVertices);
    glVertexAttribPointer_EC(vertex_texCoord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &mGlVertices[3]);

    glDrawElements_EC(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, mGlIndices);

    glDisableVertexAttribArray(vertex_pos);
    glDisableVertexAttribArray(vertex_texCoord);

}

status_t GPUWarper::updateFrameDimensions(GLuint width, GLuint height) {

    status_t status = NO_ERROR;

    if (width != mWidth || height != mHeight) {

        clearWarper();

        mWidth = width;
        mHeight = height;

        status = setupWarper();

        LOG1("Frame dimensions updated.");
    }

    return status;
}

} // namespace android
