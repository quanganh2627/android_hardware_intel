#############################################
# MODEM MANAGER java client library
#############################################
LOCAL_PATH := $(call my-dir)

ifneq ($(BOARD_HAVE_RMC_MODEM), true)
################# MAKE_XML ############################
include $(CLEAR_VARS)
LOCAL_MODULE := com.intel.internal.telephony.MmgrClient.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_OWNER := intel
# This will install the file in /system/etc/permissions
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/permissions
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

################# MAKE_JAR ############################

include $(CLEAR_VARS)

LOCAL_MODULE := com.intel.internal.telephony.MmgrClient

LOCAL_MODULE_TAGS := optional

# Only compile source java files in this jar.
LOCAL_SRC_FILES := $(call all-java-files-under, src)

include $(BUILD_STATIC_JAVA_LIBRARY)
endif
