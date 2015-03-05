#
# CP DAEMON
#
LOCAL_PATH := $(call my-dir)

#
# cpdd - e911 CP daemon
#
include $(CLEAR_VARS)

ifeq ($(MODEM_INTERFACE),stmd)
LOCAL_CFLAGS := -DSTMD_IMPLEMENTED
else
LOCAL_CFLAGS := -DMODEM_MANAGER
endif

LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES:=          \
    $(call include-path-for, icu4c-common) \
    $(call include-path-for, icu4c-i18n) \
    $(call include-path-for, libxml2) \
    $(TARGET_OUT_HEADERS)/IFX-modem \
    $(TARGET_OUT_HEADERS)/IFX-modem/mmgr_cli/c

LOCAL_SRC_FILES :=  cpdd.c  \
					cpdInit.c  \
					cpdStart.c \
					cpdUtil.c \
					cpdModem.c \
					cpdModemReadWrite.c \
					cpdXmlParser.c \
					cpdXmlUtils.c \
					cpdDebug.c \
					cpdXmlFormatter.c\
					cpdGpsComm.c  \
					cpdSocketServer.c \
					cpdSystemMonitor.c \
					cpdMMgr.c

LOCAL_C_INCLUDES += $(LOCAL_PATH)

LOCAL_STATIC_LIBRARIES := libc libxml2
LOCAL_SHARED_LIBRARIES := libicuuc libcutils

ifneq ($(MODEM_INTERFACE),stmd)
    LOCAL_SHARED_LIBRARIES += libmmgrcli
endif

LOCAL_MODULE := cpdd

include $(BUILD_EXECUTABLE)


#
# CPDD communication library for GPS.
# GPS links this library.
#
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional


LOCAL_SRC_FILES += \
                    $(CPD_PATH)/cpdInit.c  \
                    $(CPD_PATH)/cpdUtil.c \
                    $(CPD_PATH)/cpdDebug.c \
                    $(CPD_PATH)/cpdGpsComm.c  \
                    $(CPD_PATH)/cpdSocketServer.c

LOCAL_C_INCLUDES += $(LOCAL_PATH)

LOCAL_STATIC_LIBRARIES := libc
LOCAL_SHARED_LIBRARIES := libicuuc libcutils

LOCAL_MODULE := libCpd
include $(BUILD_STATIC_LIBRARY)

# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# Unit-test code:
# =======================================================
# the folowing builds build unit-test code, not part of released code, for debugging only!
# =======================================================
#
# cpd - e911 CP deamon simulator
#
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

ifeq ($(MODEM_INTERFACE),stmd)
LOCAL_CFLAGS := -DSTMD_IMPLEMENTED
else
LOCAL_CFLAGS := -DMODEM_MANAGER
endif

ifeq ($(TARGET_BUILD_VARIANT),eng)
COMMON_GLOBAL_CFLAGS += -DCPDD_DEBUG_ENABLED
endif

LOCAL_C_INCLUDES:=          \
    external/icu4c/common   \
    external/icu4c/i18n     \
    external/libxml2/include \
    $(TARGET_OUT_HEADERS)/IFX-modem

LOCAL_SRC_FILES :=  cpd.c  \
    cpdInit.c  \
    cpdStart.c \
    cpdUtil.c \
    cpdModem.c \
    cpdMMgr.c \
    cpdModemReadWrite.c \
    cpdXmlParser.c \
    cpdXmlUtils.c \
    cpdDebug.c \
    cpdXmlFormatter.c\
    cpdGpsComm.c  \
    cpdSocketServer.c \
    cpdSystemMonitor.c

LOCAL_C_INCLUDES += $(LOCAL_PATH)

LOCAL_STATIC_LIBRARIES := libc libxml2
LOCAL_SHARED_LIBRARIES := libicuuc libcutils

ifneq ($(MODEM_INTERFACE),stmd)
    LOCAL_SHARED_LIBRARIES += libmmgrcli
endif

LOCAL_MODULE := cpd

include $(BUILD_EXECUTABLE)
