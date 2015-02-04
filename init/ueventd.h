/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _INIT_UEVENTD_H_
#define _INIT_UEVENTD_H_

#include <cutils/list.h>
#include <sys/types.h>

enum devname_src_t {
    DEVNAME_UNKNOWN = 0,
    DEVNAME_UEVENT_DEVNAME,
    DEVNAME_UEVENT_DEVPATH,
};

struct ueventd_subsystem {
    struct listnode slist;

    const char *name;
    const char *dirname;
    devname_src_t devname_src;
};

int ueventd_main(int argc, char **argv);

#endif
