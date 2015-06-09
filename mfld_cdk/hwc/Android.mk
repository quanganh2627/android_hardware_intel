# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)


ifeq ($(INTEL_HWC),true)

# HAL module implemenation, not prelinked and stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)

ifeq ($(INTEL_WIDI), true)
LOCAL_COPY_HEADERS_TO := hwc
LOCAL_COPY_HEADERS := \
    IntelBufferManager.h \
    IntelDisplayPlaneManager.h \
    IntelHWCUEventObserver.h \
    IntelHWComposer.h \
    IntelHWComposerDrm.h \
    IntelHWComposerDump.h \
    IntelHWComposerLayer.h \
    IntelOverlayContext.h \
    IntelOverlayHW.h \
    IntelOverlayPlane.h \
    IntelOverlayUtil.h \
    IntelWsbm.h \
    IntelWsbmWrapper.h \
    IntelUtility.h \
    RotationBufferProvider.h
ifeq ($(TARGET_HAS_MULTIPLE_DISPLAY),true)
LOCAL_COPY_HEADERS += IntelExternalDisplayMonitor.h
endif
include $(BUILD_COPY_HEADERS)
endif

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libEGL libcutils libdrm libpvr2d \
                          libwsbm libsrv_um libui libutils libbinder\
                          libhardware libva libva-tpi libva-android
LOCAL_SRC_FILES := IntelHWComposerModule.cpp \
                   IntelHWComposer.cpp \
                   IntelDisplayDevice.cpp \
                   IntelMIPIDisplayDevice.cpp \
                   IntelHDMIDisplayDevice.cpp \
                   IntelHWComposerLayer.cpp \
                   IntelHWComposerDump.cpp \
                   IntelBufferManager.cpp \
                   IntelDisplayPlaneManager.cpp \
                   IntelHWComposerDrm.cpp \
                   IntelOverlayPlane.cpp \
                   IntelSpritePlane.cpp \
                   MedfieldSpritePlane.cpp \
                   IntelWsbm.cpp \
                   IntelWsbmWrapper.c \
                   IntelHWCUEventObserver.cpp \
                   IntelVsyncEventHandler.cpp \
                   IntelFakeVsyncEvent.cpp \
                   IntelUtility.cpp \
                   RotationBufferProvider.cpp
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM)
LOCAL_CFLAGS:= -DLOG_TAG=\"hwcomposer\" -DLINUX

ifeq ($(TARGET_SUPPORT_HWC_SYS_LAYER), true)
LOCAL_CFLAGS += -DTARGET_SUPPORT_HWC_SYS_LAYER -DINTEL_RGB_OVERLAY
LOCAL_SRC_FILES += IntelHWCWrapper.cpp
endif

LOCAL_C_INCLUDES := $(addprefix $(LOCAL_PATH)/../../, $(SGX_INCLUDES)) \
            $(call include-path-for, frameworks-native)/media/openmax \
            $(TARGET_OUT_HEADERS)/khronos/openmax \
            $(call include-path-for, frameworks-native)/ui \
            $(call include-path-for, libhardware_legacy)/hardware_legacy \
            $(TARGET_BOARD_KERNEL_HEADERS) \
            $(TARGET_OUT_HEADERS)/pvr/hal \
            $(TARGET_OUT_HEADERS)/pvr/pvr2d \
            $(TARGET_OUT_HEADERS)/pvr/include4 \
            $(TARGET_OUT_HEADERS)/drm \
            $(TARGET_OUT_HEADERS)/libdrm \
            $(TARGET_OUT_HEADERS)/libdrm/shared-core \
            $(TARGET_OUT_HEADERS)/libwsbm/wsbm \
            $(TARGET_OUT_HEADERS)/libttm \
            $(TARGET_OUT_HEADERS)/widi \
            $(TARGET_OUT_HEADERS)/libva \
            $(TARGET_OUT_HEADERS)/libwsbm
ifeq ($(TARGET_HAS_MULTIPLE_DISPLAY),true)
    LOCAL_CFLAGS += -DTARGET_HAS_MULTIPLE_DISPLAY
    LOCAL_SHARED_LIBRARIES += libmultidisplay
    LOCAL_SRC_FILES += IntelExternalDisplayMonitor.cpp
endif

ifeq ($(INTEL_WIDI), true)
    LOCAL_SHARED_LIBRARIES += libhwcwidi
    LOCAL_CFLAGS += -DINTEL_WIDI
    LOCAL_SRC_FILES += WidiDisplayDevice.cpp
endif

ifeq ($(BOARD_OVERLAY_USE_SECONDARY_GAMMA),true)
    LOCAL_CFLAGS += -DOVERLAY_USE_SECONDARY_GAMMA
endif

include $(BUILD_SHARED_LIBRARY)

endif
