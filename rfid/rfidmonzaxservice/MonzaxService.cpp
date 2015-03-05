/*
 *  Copyright (C) 2013 Intel Corporation
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

#define LOG_TAG "MonzaxService"

#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <linux/fs.h>
#include <binder/BinderService.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>

#include "MonzaxService.h"

namespace android {

MonzaxService::MonzaxService()
{
    ALOGI("rfid_monzax.service started (pid=%d)", getpid());
}

MonzaxService::~MonzaxService()
{
}

int MonzaxService::file_open(char *name)
{
    Mutex::Autolock _l(mExecuteLock);
    IPCThreadState* ipcState = IPCThreadState::self();
    int ret = open(name, O_RDWR);
    if (ret < 0)
        ALOGE("service: file_open failure: name: %s, %s",
                name, strerror(errno));
    return ret;
}
int MonzaxService::file_close(int fd)
{
    Mutex::Autolock _l(mExecuteLock);
    IPCThreadState* ipcState = IPCThreadState::self();
    int ret = close(fd);
    if (ret < 0)
        ALOGE("service: file_close failure: %s", strerror(errno));
    return ret;
}
int MonzaxService::file_seek(int fd, int offset, int whence)
{
    Mutex::Autolock _l(mExecuteLock);
    IPCThreadState* ipcState = IPCThreadState::self();
    int ret = lseek(fd, offset, whence);
    if (ret < 0)
        ALOGE("service: file_seek failure: %s", strerror(errno));
    return ret;
}
int MonzaxService::file_read(int fd, char *readBuf, int length)
{
    Mutex::Autolock _l(mExecuteLock);
    IPCThreadState* ipcState = IPCThreadState::self();
    int ret = read(fd, readBuf, length);
    if (ret < 0)
        ALOGE("service: file_read failure: %s", strerror(errno));
    return ret;
}
int MonzaxService::file_write(int fd, char *writeBuf, int length)
{
    Mutex::Autolock _l(mExecuteLock);
    IPCThreadState* ipcState = IPCThreadState::self();
    int ret = write(fd, writeBuf, length);
    if (ret < 0)
        ALOGE("service: file_write failure: %s", strerror(errno));
    return ret;
}

}//namespace
