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
LOCAL_PATH := $(my-dir)

ifneq ($(TARGET_SIMULATOR),true)
  include $(call first-makefiles-under,$(LOCAL_PATH))
else
  include $(addprefix $(LOCAL_PATH)/,$(addsuffix /Android.mk, \
	      adb \
	      libcutils \
	      liblog \
	      libnetutils \
	      libpixelflinger \
	      libzipfile \
	   ))
endif
