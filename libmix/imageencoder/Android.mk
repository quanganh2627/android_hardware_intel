###############################################
#             libmix_imageencoder             #
###############################################
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= ImageEncoder.cpp

LOCAL_C_INCLUDES += \
    $(TARGET_OUT_HEADERS)/libva

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    liblog \
    libva \
    libva-android \
    libva-tpi

LOCAL_COPY_HEADERS_TO  := libmix_imageencoder

LOCAL_COPY_HEADERS := ImageEncoder.h

LOCAL_MODULE := libmix_imageencoder
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)


###############################################
#   libmix_imageencoder's Test Application    #
###############################################
include $(CLEAR_VARS)
LOCAL_SRC_FILES:= test/main.cpp

LOCAL_C_INCLUDES += \
    $(TARGET_OUT_HEADERS)/libva \
    $(TARGET_OUT_HEADERS)/libmix_imageencoder

ifeq ($(ENABLE_GEN_GRAPHICS),true)
LOCAL_CFLAGS += -DGEN_GFX
endif

LOCAL_SHARED_LIBRARIES := \
    libmix_imageencoder \
    libui \
    libgui

LOCAL_MODULE := libmix_imageencoder_tester
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
