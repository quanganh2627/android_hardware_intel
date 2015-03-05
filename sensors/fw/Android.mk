
ifeq ($(USE_GENERAL_SENSOR_DRIVER),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := sensor_parser.c
LOCAL_C_INCLUDES +=  $(COMMON_INCLUDES) \
                $(call include-path-for, icu4c-common) \
                $(call include-path-for, libxml2)

LOCAL_CFLAGS += -g -Wall
LOCAL_STATIC_LIBRARIES := libxml2
LOCAL_SHARED_LIBRARIES := libicuuc-host
LOCAL_MODULE := sensor_parser
include $(BUILD_HOST_EXECUTABLE)

#XML driver config for each platform
SENSOR_DRIVER_XML := $(DEVICE_CONF_PATH)/sensors/sensor_driver_config.xml

include $(CLEAR_VARS)
LOCAL_MODULE := sensor_config.bin
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/firmware/
include $(BUILD_SYSTEM)/base_rules.mk
$(LOCAL_BUILT_MODULE): sensor_parser
	@echo "Generating Sensor Driver Firmware image..."
	$(hide)mkdir -p $(dir $@)
	$(hide)sensor_parser -p -x $(SENSOR_DRIVER_XML) -f $@

endif # USE_GENERAL_SENSOR_DRIVER
