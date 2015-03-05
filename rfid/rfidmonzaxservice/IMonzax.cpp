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

#define LOG_TAG "IMonzax"

#include <stdint.h>
#include <sys/types.h>
#include <binder/Parcel.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <private/android_filesystem_config.h>

#include "IMonzax.h"

namespace android {

enum
{
    FILE_OPEN = IBinder::FIRST_CALL_TRANSACTION,
    FILE_CLOSE,
    FILE_SEEK,
    FILE_READ,
    FILE_WRITE,
};

sp<IMonzax> IMonzax::sMonzaxManager;
sp<IMonzax> IMonzax::getMonzaxSerivce()
{
    sp<IBinder> binder;

    ALOGI("getMonzaxService called");

    if(sMonzaxManager != NULL)
        return sMonzaxManager;

    sp<IServiceManager> sm = defaultServiceManager();
    do {
        binder = sm->getService(String16("rfid_monzax.service"));
        if (binder == 0) {
            ALOGW("MonzaxService not published, waiting...");
            usleep(500000); // 0.5 s
        }
    } while(binder == 0);

    if(sMonzaxManager == NULL){
        sMonzaxManager = interface_cast<IMonzax>(binder);
    }

    return sMonzaxManager;
}

//proxy implement
class BpMonzax : public BpInterface<IMonzax>
{
public:
    BpMonzax(const sp<IBinder>& impl)
        : BpInterface<IMonzax>(impl)
    {
    }
    virtual int file_open(char * name)
    {
        Parcel data, reply;

        data.writeInterfaceToken(IMonzax::getInterfaceDescriptor());
        ALOGI("Bp transact file_open: %s", name);
        data.writeCString(name);
        remote()->transact(FILE_OPEN, data, &reply);
        return reply.readInt32();
    }
    virtual int file_close(int fd)
    {
        Parcel data, reply;

        data.writeInterfaceToken(IMonzax::getInterfaceDescriptor());
        ALOGI("Bp transact file_close: %d", fd);
        data.writeInt32(fd);
        remote()->transact(FILE_CLOSE, data, &reply);
        return reply.readInt32();
    }
    virtual int file_seek(int fd, int offset, int whence)
    {
        Parcel data, reply;

        data.writeInterfaceToken(IMonzax::getInterfaceDescriptor());
        data.writeInt32(fd);
        data.writeInt32(offset);
        data.writeInt32(whence);
        remote()->transact(FILE_SEEK, data, &reply);
        return reply.readInt32();
    }
    virtual int file_read(int fd, char *readBuf,int length)
    {
        Parcel data, reply;

        data.writeInterfaceToken(IMonzax::getInterfaceDescriptor());
        data.writeInt32(fd);
        data.writeInt32(length);
        remote()->transact(FILE_READ, data, &reply);

        int ret = reply.readInt32();
        if (ret > 0) {
            const char *result = (const char *)reply.readInplace(ret);
            if (result)
                memcpy((char *)readBuf, result, ret);
            else {
                ALOGE("Bp transact File_Read length is 0, failed.\n");
                memset(readBuf, 0, length);
                ret = 0;
            }
        } else {
            memset(readBuf, 0, length);
        }
        return ret;
    }
    virtual int file_write(int fd, char *writeBuf, int length)
    {
        Parcel data, reply;

        data.writeInterfaceToken(IMonzax::getInterfaceDescriptor());
        data.writeInt32(fd);
        data.writeInt32(length);
        data.write((const char *)writeBuf, length);
        remote()->transact(FILE_WRITE, data, &reply);
        return reply.readInt32();
    }
};

IMPLEMENT_META_INTERFACE(Monzax, "intel.rfid.IMonzax");

status_t BnMonzax::onTransact(
        uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{

    status_t status = BBinder::onTransact(code, data, reply, flags);
    if (status != UNKNOWN_TRANSACTION) {
        return status;
    }
    if (! reply) {
        ALOGE("Monzax::onTransact(): null reply parcel received.");
        return BAD_VALUE;
    }

    IPCThreadState *ipc = IPCThreadState::self();
    const int pid = ipc->getCallingPid();
    const int uid = ipc->getCallingUid();

    // dispatch to the appropriate method based on the transaction code.
    switch(code){
        case FILE_OPEN:
            {
                CHECK_INTERFACE(IMonzax, data, reply);
                ALOGI("BnMonzax: onTransact: FILE_OPEN"
                    "client to access MonzaxService from uid=%d pid=%d code: %d",
                    uid, pid, code);
                char *pName = (char *) data.readCString();
                int ret = file_open(pName);
                reply->writeInt32(ret);
                return NO_ERROR;
            }
            break;
        case FILE_CLOSE:
            {
                ALOGI("BnMonzax: onTransact: FILE_CLOSE"
                    "client to access MonzaxService from uid=%d pid=%d code: %d",
                    uid, pid, code);
                CHECK_INTERFACE(IMonzax, data, reply);
                int fd = data.readInt32();
                int ret = file_close(fd);
                reply->writeInt32(ret);
                return NO_ERROR;
            }
            break;
        case FILE_SEEK:
            {
                CHECK_INTERFACE(IMonzax, data, reply);
                int ret;

                int fd = data.readInt32();
                int offset = data.readInt32();
                int whence = data.readInt32();
                ret = file_seek(fd, offset, whence);
                reply->writeInt32(ret);
                return NO_ERROR;
            }
            break;
        case FILE_READ:
            {
                CHECK_INTERFACE(IMonzax, data, reply);
                char *pReadBuf;
                int ret;

                int fd = data.readInt32();
                int length = data.readInt32();
                pReadBuf = (char*) malloc(sizeof(char) * length);
                if (pReadBuf == NULL) {
                    ret = -1;
                    reply->writeInt32(ret);
                    ALOGE("onTransact File_Read malloc result failed.\n");
                    goto err_read;
                }
                ret = file_read(fd, pReadBuf, length);
                reply->writeInt32(ret);
                if(ret >0){
                    reply->write((char *) pReadBuf, ret);
                }
                free(pReadBuf);
                pReadBuf = NULL;
                return NO_ERROR;
err_read:
                ALOGE("onTransact File_Read ,ret =%d out\n",  (int)ret);
                return ret;
            }
            break;
        case FILE_WRITE:
            {
                CHECK_INTERFACE(IMonzax, data, reply);
                int ret;
                int fd = data.readInt32();
                int length = data.readInt32();
                char *pWriteBuf = (char*) malloc(sizeof(char) * length);
                if (pWriteBuf == NULL) {
                    ret = -1;
                    reply->writeInt32(ret);
                    ALOGE("onTransact File_Write malloc result failed.\n");
                    goto err_write;
                }
                if(length > 0){
                    const char *input = (const char *) data.readInplace(length);
                    if (input)
                        memcpy(pWriteBuf, input, length);
                    else {
                        ALOGE("onTransact File_Write length is 0, failed.\n");
                        memset(pWriteBuf, 0, length);
                        length = 0;
                    }
                } else
                    memset(pWriteBuf, 0, length);

                ret = file_write(fd, (char *)pWriteBuf, length);
                reply->writeInt32(ret);
                free(pWriteBuf);
                pWriteBuf = NULL;
                return NO_ERROR;
err_write:
                ALOGE("onTransact File_Write ,ret =%d out\n",  (int)ret);
                return ret;
            }
            break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

}//namespace
