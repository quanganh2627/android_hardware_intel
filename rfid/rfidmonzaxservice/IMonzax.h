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

#ifndef RFID_ITEST_H
#define RFID_ITEST_H

#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <binder/BinderService.h>
#include <utils/String8.h>

namespace android {

class IMonzax: public IInterface
{
protected:
    static sp<IMonzax> sMonzaxManager;

public:
    DECLARE_META_INTERFACE(Monzax);
    static sp<IMonzax> getMonzaxSerivce();
    virtual int file_open(char *name) = 0;
    virtual int file_close(int fd) = 0;
    virtual int file_seek(int fd, int offset, int whence) = 0;
    virtual int file_read(int fd, char *readBuf, int length) = 0;
    virtual int file_write(int fd, char *writeBuf, int length) = 0;
};

/* BnMonzax onTransact */
class BnMonzax: public BnInterface<IMonzax>
{
public:
    virtual status_t onTransact(
        uint32_t code,
        const Parcel& data,
        Parcel* reply,
        uint32_t flags = 0
    );
};

}

#endif  // _FILE_H
