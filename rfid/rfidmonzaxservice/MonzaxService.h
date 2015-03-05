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

#ifndef _RFID_SERVICE_H
#define _RFID_SERVICE_H

#include <stdint.h>
#include <binder/BinderService.h>
#include "IMonzax.h"

namespace android {

//declare
class MonzaxService :
    public BinderService<MonzaxService>,
    public BnMonzax
{
    friend class BinderService<MonzaxService>;
private:
    Mutex mExecuteLock;

    MonzaxService();
    virtual ~MonzaxService();

public:
    static char const* getServiceName() { return "rfid_monzax.service"; }
    virtual int file_open(char *name);
    virtual int file_close(int fd);
    virtual int file_seek(int fd, int offset, int whence);
    virtual int file_read(int fd, char *readBuf, int length);
    virtual int file_write(int fd, char *writeBuf, int length);
};

}//namespace

#endif
