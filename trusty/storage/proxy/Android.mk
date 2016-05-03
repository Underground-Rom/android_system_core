#
# Copyright (C) 2016 The Android Open-Source Project
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

LOCAL_MODULE := storageproxyd

LOCAL_SRC_FILES := \
	ipc.c \
	rpmb.c \
	storage.c \
	proxy.c

LOCAL_CLFAGS = -Wall -Werror

LOCAL_SHARED_LIBRARIES := \
	liblog \

LOCAL_STATIC_LIBRARIES := \
	libtrustystorageinterface \
	libtrusty

include $(BUILD_EXECUTABLE)

