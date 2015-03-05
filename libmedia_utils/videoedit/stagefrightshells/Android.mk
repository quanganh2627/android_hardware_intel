#
# Copyright (C) 2011 The Android Open Source Project
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
#

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    MediaBufferPuller.cpp \
    VideoEditorVideoDecoder.cpp \
    VideoEditorAudioDecoder.cpp \
    VideoEditorMp3Reader.cpp \
    VideoEditor3gpReader.cpp \
    VideoEditorBuffer.cpp \
    VideoEditorVideoEncoder.cpp \
    VideoEditorAudioEncoder.cpp \
    IntelVideoEditorUtils.cpp \
    IntelVideoEditorEncoderSource.cpp \
    IntelVideoEditorAVCEncoder.cpp \
    IntelVideoEditorH263Encoder.cpp

LOCAL_C_INCLUDES += \
    $(call include-path-for, libmediaplayerservice) \
    $(call include-path-for, libstagefright)/.. \
    $(call include-path-for, libstagefright) \
    $(call include-path-for, libstagefright-rtsp) \
    $(call include-path-for, corecg graphics) \
    $(call include-path-for, osal) \
    $(call include-path-for, vss-common) \
    $(call include-path-for, vss-mcs) \
    $(call include-path-for, vss) \
    $(call include-path-for, vss-stagefrightshells) \
    $(call include-path-for, lvpp) \
    $(call include-path-for, frameworks-native)/media/editor \
    $(call include-path-for, frameworks-native)/media/openmax \
    $(TARGET_OUT_HEADERS)/libsharedbuffer \
    $(TARGET_OUT_HEADERS)/libmix_videoencoder \
    $(TARGET_OUT_HEADERS)/libva

LOCAL_SHARED_LIBRARIES :=     \
    libcutils                 \
    libutils                  \
    libmedia                  \
    libbinder                 \
    libstagefright            \
    libstagefright_foundation \
    libstagefright_omx        \
    libgui                    \
    libvideoeditor_osal       \
    libvideoeditorplayer      \
    libsharedbuffer

LOCAL_CFLAGS += -DVIDEOEDITOR_INTEL_NV12_VERSION

# Add specific code for Merrifield
ifneq ($(filter $(TARGET_BOARD_PLATFORM),merrifield moorefield),)
LOCAL_CFLAGS += -DRUN_IN_MERRIFIELD
endif
LOCAL_STATIC_LIBRARIES := \
    libstagefright_color_conversion


LOCAL_MODULE:= libvideoeditor_stagefrightshells_intel

LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)
