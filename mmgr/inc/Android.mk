#############################################
# MMGR interface file copy
#############################################

# @TODO: remove this file once all clients are updated
# to use the new header export mechanism

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_COPY_HEADERS := \
    mmgr_cli.h \
    mmgr_fw_cli.h \
    mmgr.h \
    modem_update.h

LOCAL_COPY_HEADERS_TO := IFX-modem
include $(BUILD_COPY_HEADERS)

