#
# Copyright (C) 2008 The Android Open Source Project
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

LOCAL_PATH := hardware/amlogic/libril/config

PRODUCT_COPY_FILES := \
		$(LOCAL_PATH)/mcli-gsm:system/etc/ppp/peers/mcli-gsm \
		$(LOCAL_PATH)/mcli-cdma:system/etc/ppp/peers/mcli-cdma \
		$(filter-out %:system/etc/apns-conf.xml,$(PRODUCT_COPY_FILES)) \
		$(LOCAL_PATH)/apns-conf.xml:system/etc/apns-conf.xml \
		$(LOCAL_PATH)/voicemail-conf.xml:system/etc/voicemail-conf.xml
