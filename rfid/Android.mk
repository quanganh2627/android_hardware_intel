LOCAL_PATH:= $(call my-dir)

########################################################################
### monzaxservice lib
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	rfidmonzaxservice/IMonzax.cpp \
	rfidmonzaxservice/MonzaxService.cpp

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils \
	libbinder

LOCAL_MODULE:= libmonzax_service
include $(BUILD_SHARED_LIBRARY)

########################################################################
### monzaxservice deamon
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= rfidmonzaxservice/main.cpp
LOCAL_SHARED_LIBRARIES := libmonzax_service libbinder libcutils libutils

LOCAL_MODULE := rfid_monzaxd
include $(BUILD_EXECUTABLE)

