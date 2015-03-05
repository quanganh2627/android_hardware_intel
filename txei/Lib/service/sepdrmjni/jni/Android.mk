LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ANDROID_PRODUCT_OUT := /home/nshuster/android/WW42_420/android-main/out/target/product/ctp_nomodem
TARGET_OUT_HEADERS := $(ANDROID_PRODUCT_OUT)/obj/include
LOCAL_LIB_DIR := $(ANDROID_PRODUCT_OUT)/system/lib

LOCAL_SHARED_LIBRARIES  := libsepdrm liblog

LOCAL_LDFLAGS += -L$(ANDROID_PRODUCT_OUT)/system/lib/

LOCAL_LDLIBS += -llog -lsepdrm

LOCAL_C_INCLUDES :=				\
	$(TARGET_OUT_HEADERS)/libsepdrm

LOCAL_MODULE    := sepdrmJNI
LOCAL_SRC_FILES := sepdrmJNI.cpp

include $(BUILD_SHARED_LIBRARY)
