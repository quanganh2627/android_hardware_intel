/*
 * Copyright (c) 2008-2012, Intel Corporation. All rights reserved.
 *
 * Redistribution.
 * Redistribution and use in binary form, without modification, are
 * permitted provided that the following conditions are met:
 *  * Redistributions must reproduce the above copyright notice and
 * the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its
 * suppliers may be used to endorse or promote products derived from
 * this software without specific  prior written permission.
 *  * No reverse engineering, decompilation, or disassembly of this
 * software is permitted.
 *
 * Limited patent license.
 * Intel Corporation grants a world-wide, royalty-free, non-exclusive
 * license under patents it now or hereafter owns or controls to make,
 * have made, use, import, offer to sell and sell ("Utilize") this
 * software, but solely to the extent that any such patent is necessary
 * to Utilize the software alone, or in combination with an operating
 * system licensed under an approved Open Source license as listed by
 * the Open Source Initiative at http://opensource.org/licenses.
 * The patent license shall not apply to any other combinations which
 * include this software. No hardware per se is licensed hereunder.
 *
 * DISCLAIMER.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#include <IntelHWCUEventObserver.h>
#include <IntelHWComposerCfg.h>
#include <cutils/log.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <linux/netlink.h>

IntelHWCUEventObserver::IntelHWCUEventObserver()
    : mReadyToRun(false)
{
}

IntelHWCUEventObserver::~IntelHWCUEventObserver()
{

}

bool IntelHWCUEventObserver::startObserver()
{
    // create new pthread
    int err = pthread_create(&mThread, NULL,
                             IntelHWCUEventObserver::threadLoop,
                             (void *)this);
    if (err) {
        ALOGE("%s: failed to start observer, err %d\n", __func__, err);
        return false;
    }

    mReadyToRun = true;

    ALOGD("%s: observer started\n", __func__);
    return true;
}

bool IntelHWCUEventObserver::stopObserver()
{
    mReadyToRun = false;

    int err = pthread_join(mThread, NULL);
    if (err) {
        ALOGE("%s: failed to stop observer, err %d\n", __func__, err);
        return false;
    }
    ALOGD("%s: observer stopped\n", __func__);
    return true;
}

void *IntelHWCUEventObserver::threadLoop(void *data)
{
    char ueventMessage[UEVENT_MSG_LEN];
    int fd = -1;
    struct sockaddr_nl addr;
    int sz = 64*1024;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid =  pthread_self() | getpid();
    addr.nl_groups = 0xffffffff;

    fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if(fd < 0) {
        ALOGD("%s: failed to open uevent socket\n", __func__);
        return 0;
    }

    setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));

    if(bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(fd);
        return 0;
    }

    memset(ueventMessage, 0, UEVENT_MSG_LEN);
    IntelHWCUEventObserver *observer =
        static_cast<IntelHWCUEventObserver*>(data);

    do {
        struct pollfd fds;
        int nr;

        fds.fd = fd;
        fds.events = POLLIN;
        fds.revents = 0;
        nr = poll(&fds, 1, -1);

        if(nr > 0 && fds.revents == POLLIN) {
            int count = recv(fd, ueventMessage, UEVENT_MSG_LEN - 2, 0);
            if (count > 0)
                observer->onUEvent(0, ueventMessage, UEVENT_MSG_LEN - 2);
        }
    } while (observer->isReadyToRun());

thread_exit:
    ALOGD("%s: observer exited\n", __func__);
    return NULL;
}

bool IntelHWCUEventObserver::onUEvent(int msgType, void* msg, int msgLen)
{
    return true;
}
