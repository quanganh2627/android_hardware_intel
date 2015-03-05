LOCAL_PATH := $(call my-dir)

ifneq ($(TARGET_DEVICE),sim)
ifneq (,$(findstring $(REF_DEVICE_NAME), mfld_cdk))
# HAL module implemenation, not prelinked and stored in
# hw/<SENSORS_HARDWARE_MODULE_ID>.<ro.hardware>.so
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_SRC_FILES := sensors_gaid.c \
                   sensors_gaid_accel.c \
		   sensors_gaid_compass.c \
		   sensors_gaid_light.c \
		   sensors_gaid_proximity.c \
		   sensors_gaid_orientation.c
LOCAL_MODULE := sensors.$(TARGET_DEVICE)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
endif
endif
