/*
 * Copyright (C) 2008 The Android Open Source Project
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
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <string.h>

#define LOG_TAG "NetlinkListener"
#include <cutils/log.h>
#include <cutils/uevent.h>

#include <sysutils/NetlinkEvent.h>

NetlinkListener::NetlinkListener(int socket, int format) :
                            SocketListener(socket, false), mFormat(format) {
}

bool NetlinkListener::onDataAvailable(SocketClient *cli)
{
    int socket = cli->getSocket();
    ssize_t count;

    count = TEMP_FAILURE_RETRY(uevent_kernel_multicast_recv(socket, mBuffer, sizeof(mBuffer)));
    if (count < 0) {
        SLOGE("recvmsg failed (%s)", strerror(errno));
        return false;
    }

    NetlinkEvent *evt = new NetlinkEvent();
    if (!evt->decode(mBuffer, count, mFormat)) {
        SLOGE("Error decoding NetlinkEvent");
    } else {
        onEvent(evt);
    }

    delete evt;
    return true;
}
