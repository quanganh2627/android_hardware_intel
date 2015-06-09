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

#ifndef ANDROID_LIBCAMERA_GPUSCALER_EGL_UTIL_H
#define ANDROID_LIBCAMERA_GPUSCALER_EGL_UTIL_H

namespace android
{

const char *strerror(EGLint err) {
    switch (err) {
        case EGL_SUCCESS:           return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:   return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:        return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:         return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:     return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONFIG:        return "EGL_BAD_CONFIG";
        case EGL_BAD_CONTEXT:       return "EGL_BAD_CONTEXT";
        case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:       return "EGL_BAD_DISPLAY";
        case EGL_BAD_MATCH:         return "EGL_BAD_MATCH";
        case EGL_BAD_NATIVE_PIXMAP: return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW: return "EGL_BAD_NATIVE_WINDOW";
        case EGL_BAD_PARAMETER:     return "EGL_BAD_PARAMETER";
        case EGL_BAD_SURFACE:       return "EGL_BAD_SURFACE";
        case EGL_CONTEXT_LOST:      return "EGL_CONTEXT_LOST";
        default: return "UNKNOWN";
    }
}

void pzEglUtil_printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    LOG2("GL %s = %s\n", name, v);
}

void pzEglUtil_printEGLConfiguration(EGLDisplay dpy, EGLConfig config) {

#define X(VAL) {VAL, #VAL}
    struct {EGLint attribute; const char* name;} names[] = {
    X(EGL_BUFFER_SIZE),
    X(EGL_ALPHA_SIZE),
    X(EGL_BLUE_SIZE),
    X(EGL_GREEN_SIZE),
    X(EGL_RED_SIZE),
    X(EGL_DEPTH_SIZE),
    X(EGL_STENCIL_SIZE),
    X(EGL_CONFIG_CAVEAT),
    X(EGL_CONFIG_ID),
    X(EGL_LEVEL),
    X(EGL_MAX_PBUFFER_HEIGHT),
    X(EGL_MAX_PBUFFER_PIXELS),
    X(EGL_MAX_PBUFFER_WIDTH),
    X(EGL_NATIVE_RENDERABLE),
    X(EGL_NATIVE_VISUAL_ID),
    X(EGL_NATIVE_VISUAL_TYPE),
    X(EGL_SAMPLES),
    X(EGL_SAMPLE_BUFFERS),
    X(EGL_SURFACE_TYPE),
    X(EGL_TRANSPARENT_TYPE),
    X(EGL_TRANSPARENT_RED_VALUE),
    X(EGL_TRANSPARENT_GREEN_VALUE),
    X(EGL_TRANSPARENT_BLUE_VALUE),
    X(EGL_BIND_TO_TEXTURE_RGB),
    X(EGL_BIND_TO_TEXTURE_RGBA),
    X(EGL_MIN_SWAP_INTERVAL),
    X(EGL_MAX_SWAP_INTERVAL),
    X(EGL_LUMINANCE_SIZE),
    X(EGL_ALPHA_MASK_SIZE),
    X(EGL_COLOR_BUFFER_TYPE),
    X(EGL_RENDERABLE_TYPE),
    X(EGL_CONFORMANT),
   };
#undef X

    for (size_t j = 0; j < sizeof(names) / sizeof(names[0]); j++) {
        EGLint value = -1;
        EGLint returnVal = eglGetConfigAttrib(dpy, config, names[j].attribute, &value);
        EGLint error = eglGetError();
        if (returnVal && error == EGL_SUCCESS) {
            LOG2(" %s: ", names[j].name);
            LOG2("%d (0x%x)", value, value);
        }
    }
}

void pzEglUtil_checkEglError(const char* op, EGLBoolean returnVal = EGL_TRUE) {
    if (returnVal != EGL_TRUE) {
        ALOGE("%s() returned %d\n", op, returnVal);
    }
    for (EGLint error = eglGetError(); error != EGL_SUCCESS; error =
            eglGetError()) {
        ALOGE("after %s() eglError %s (0x%x)\n", op, strerror(error), error);
    }
}

void pzEglUtil_checkGlError(const char* op) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        ALOGE("%s() glError (0x%x)\n", op, error);
    }
}

// Enable error checking for egl/gl functions with "_EC" postfix for GPU_SCALER_DEBUG mode
// Disable otherwise with empty string for error check function call
#ifdef GPU_SCALER_DEBUG
#define checkGlError(x)        pzEglUtil_checkGlError(x);
#define checkEglError(x)    pzEglUtil_checkEglError(x);
#else
#define checkGlError(x)
#define checkEglError(x)
#endif
#define eglGetDisplay_EC(...)                    eglGetDisplay(__VA_ARGS__);                 checkEglError("eglGetDisplay")
#define eglInitialize_EC(...)                    eglInitialize(__VA_ARGS__);                 checkEglError("eglInitialize")
#define eglBindAPI_EC(...)                       eglBindAPI(__VA_ARGS__);                    checkEglError("eglBindAPI")
#define eglChooseConfig_EC(...)                  eglChooseConfig(__VA_ARGS__);               checkEglError("eglChooseConfig")
#define eglCreateWindowSurface_EC(...)           eglCreateWindowSurface(__VA_ARGS__);        checkEglError("eglCreateWindowSurface")
#define eglCreateContext_EC(...)                 eglCreateContext(__VA_ARGS__);              checkEglError("eglCreateContext")
#define eglMakeCurrent_EC(...)                   eglMakeCurrent(__VA_ARGS__);                checkEglError("eglMakeCurrent")
#define eglQuerySurface_EC(...)                  eglQuerySurface(__VA_ARGS__);               checkEglError("eglQuerySurface")
#define eglCreateImageKHR_EC(...)                eglCreateImageKHR(__VA_ARGS__)              checkEglError("eglCreateImageKHR")
#define glPixelStorei_EC(...)                    glPixelStorei(__VA_ARGS__);                 checkGlError("glPixelStorei")
#define glGenTextures_EC(...)                    glGenTextures(__VA_ARGS__);                 checkGlError("glGenTextures")
#define glActiveTexture_EC(...)                  glActiveTexture(__VA_ARGS__);               checkGlError("glActiveTexture")
#define glBindTexture_EC(...)                    glBindTexture(__VA_ARGS__);                 checkGlError("glBindTexture")
#define glTexImage2D_EC(...)                     glTexImage2D(__VA_ARGS__);                  checkGlError("glTexImage2D")
#define glTexParameteri_EC(...)                  glTexParameteri(__VA_ARGS__);               checkGlError("glTexParameteri")
#define glBindFramebuffer_EC(...)                glBindFramebuffer(__VA_ARGS__);             checkGlError("glBindFramebuffer")
#define glFramebufferTexture2D_EC(...)           glFramebufferTexture2D(__VA_ARGS__);        checkGlError("glFramebufferTexture2D")
#define glClear_EC(...)                          glClear(__VA_ARGS__);                       checkGlError("glClear")
#define glGenFramebuffers_EC(...)                glGenFramebuffers(__VA_ARGS__);             checkGlError("glGenFramebuffers")
#define glCreateShader_EC(...)                   glCreateShader(__VA_ARGS__);                checkGlError("glCreateShader")
#define glShaderSource_EC(...)                   glShaderSource(__VA_ARGS__);                checkGlError("glShaderSource")
#define glCompileShader_EC(...)                  glCompileShader(__VA_ARGS__);               checkGlError("glCompileShader")
#define glGetShaderiv_EC(...)                    glGetShaderiv(__VA_ARGS__);                 checkGlError("glGetShaderiv")
#define glGetShaderInfoLog_EC(...)               glGetShaderInfoLog(__VA_ARGS__);            checkGlError("glGetShaderInfoLog")
#define glCreateShader_EC(...)                   glCreateShader(__VA_ARGS__);                checkGlError("glCreateShader")
#define glAttachShader_EC(...)                   glAttachShader(__VA_ARGS__);                checkGlError("glAttachShader")
#define glLinkProgram_EC(...)                    glLinkProgram(__VA_ARGS__);                 checkGlError("glLinkProgram")
#define glGetProgramiv_EC(...)                   glGetProgramiv(__VA_ARGS__);                checkGlError("glGetProgramiv")
#define glCreateProgram_EC(...)                  glCreateProgram(__VA_ARGS__);               checkGlError("glCreateProgram")
#define glGetAttribLocation_EC(...)              glGetAttribLocation(__VA_ARGS__);           checkGlError("glGetAttribLocation");
#define glGetUniformLocation_EC(...)             glGetUniformLocation(__VA_ARGS__);          checkGlError("glGetUniformLocation");
#define glUseProgram_EC(...)                     glUseProgram(__VA_ARGS__);                  checkGlError("glUseProgram");
#define glVertexAttribPointer_EC(...)            glVertexAttribPointer(__VA_ARGS__);         checkGlError("glVertexAttribPointer");
#define glEnableVertexAttribArray_EC(...)        glEnableVertexAttribArray(__VA_ARGS__);     checkGlError("glEnableVertexAttribArray");
#define glUniform1i_EC(...)                      glUniform1i(__VA_ARGS__);                   checkGlError("glUniform1i");
#define glUniform1f_EC(...)                      glUniform1f(__VA_ARGS__);                   checkGlError("glUniform1f");
#define glUniform2f_EC(...)                      glUniform2f(__VA_ARGS__);                   checkGlError("glUniform2f");
#define glUniform4f_EC(...)                      glUniform4f(__VA_ARGS__);                   checkGlError("glUniform4f");
#define glReadPixels_EC(...)                     glReadPixels(__VA_ARGS__);                  checkGlError("glReadPixels");
#define glDrawElements_EC(...)                   glDrawElements(__VA_ARGS__);                checkGlError("glDrawElements");
#define glViewport_EC(...)                       glViewport(__VA_ARGS__);                    checkGlError("glViewport");
#define glFlush_EC(...)                          glFlush(__VA_ARGS__);                       checkGlError("glFlush");
#define glTexSubImage2D_EC(...)                  glTexSubImage2D(__VA_ARGS__);               checkGlError("glTexSubImage2D");
#define glEGLImageTargetTexture2DOES_EC(...)     glEGLImageTargetTexture2DOES(__VA_ARGS__);  checkGlError("glEGLImageTargetTexture2DOES");
#define glGenRenderbuffers_EC(...)		 glGenRenderbuffers(__VA_ARGS__);            checkGlError("glGenRenderbuffers");
#define glBindRenderbuffer_EC(...)               glBindRenderbuffer(__VA_ARGS__);            checkGlError("glBindRenderbuffer");
#define glEGLImageTargetRenderbufferStorageOES_EC(...) glEGLImageTargetRenderbufferStorageOES(__VA_ARGS__); checkGlError("glEGLImageTargetRenderbufferStorageOES");
#define glFramebufferRenderbuffer_EC(...)	 glFramebufferRenderbuffer(__VA_ARGS__);     checkGlError("glFramebufferRenderbuffer");
#define glCheckFramebufferStatus_EC(...)         glCheckFramebufferStatus(__VA_ARGS__);      checkGlError("glCheckFramebufferStatus");
#define glClearColor_EC(...)			 glClearColor(__VA_ARGS__);		     checkGlError("glClearColor");
} // namespace android

#endif // ANDROID_LIBCAMERA_GPUSCALER_EGL_UTIL_H
