# Copyright (C) 2009 The Android Open Source Project
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

ifeq ($(BOARD_USES_G_SENSOR_LSM303DLH),true)

LOCAL_PATH := $(call my-dir)

ifneq ($(TARGET_PRODUCT),sim)
# HAL module implemenation, not prelinked and stored in
# hw/<SENSORS_HARDWARE_MODULE_ID>.<ro.hardware>.so
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libcutils
#LOCAL_SRC_FILES := sensors_lsm303dlh.c a.c b.c c.c
LOCAL_SRC_FILES := sensors_lsm303dlh.c
LOCAL_MODULE := sensors.amlogic  
LOCAL_MODULE_TAGS := user
LOCAL_LDFLAGS = $(LOCAL_PATH)/liblsm303dlh.a
include $(BUILD_SHARED_LIBRARY)
endif

endif