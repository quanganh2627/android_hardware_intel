/*
 * Copyright 2012 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_LIBCAMERA_CAMERA_CONFIGURATION_H
#define ANDROID_LIBCAMERA_CAMERA_CONFIGURATION_H

#include <cpf-hal.h>
#include <libtbd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/String8.h>
#include <utils/Vector.h>

namespace android {

class CameraBlob
{
    class Blob : public RefBase
    {
    public:
        Blob(const int size, void *& ptr);
        virtual ~Blob();
    private:
        void *mPtr;
        // Disallow copy and assignment
        Blob(const Blob&);
        void operator=(const Blob&);
    };

public:
    explicit CameraBlob(const int size = 0);
    CameraBlob(const CameraBlob& refBlob, const int offset, const int size);
    CameraBlob(const CameraBlob& refBlob, void * const ptr, const int size);
    virtual inline ~CameraBlob() { clear(); }
    inline void *ptr() const { return mPtr; }
    inline int size() const { return mSize; }
    inline operator void *() const { return mPtr; }
    inline operator bool() const { return mPtr; }
    CameraBlob copy();
    void clear();
private:
    sp<Blob> mBlob;
    void *mPtr;
    int mSize;
};

class AiqConf : public CameraBlob
{
public:
    explicit inline AiqConf() {}
    inline AiqConf(const CameraBlob& refBlob) : CameraBlob(refBlob) {}
    inline virtual ~AiqConf() {}
};

class DrvConf : public CameraBlob
{
public:
    explicit inline DrvConf() {}
    inline DrvConf(const CameraBlob& refBlob) : CameraBlob(refBlob) {}
    inline virtual ~DrvConf() {}
};

class HalConf : public CameraBlob
{
public:
    explicit inline HalConf() {}
    inline HalConf(const CameraBlob& refBlob) : CameraBlob(refBlob) {}
    inline virtual ~HalConf() {}
    // Queries with return codes
    status_t getValue(int& value, CPF::cpf_hal_tag_t tag, ...);
    status_t getBool(bool& boolean, CPF::cpf_hal_tag_t tag, ...);
    status_t getString(const char *& string, CPF::cpf_hal_tag_t tag, ...);
    status_t getFpoint(int32_t& value, CPF::cpf_hal_tag_t tag, ...);
    status_t getFloat(float& value, CPF::cpf_hal_tag_t tag, ...);
    // Straightforward queries
    int getValue(CPF::cpf_hal_tag_t tag, ...);
    bool getBool(CPF::cpf_hal_tag_t tag, ...);
    const char *getString(CPF::cpf_hal_tag_t tag, ...);
    int32_t getFpoint(CPF::cpf_hal_tag_t tag, ...);
    float getFloat(CPF::cpf_hal_tag_t tag, ...);
private:
    status_t getAny(void *anyPtr, cpf_hal_tagtype_t type, CPF::cpf_hal_tag_t tag, va_list args);
};

class CpfStore
{
    struct SensorDriver {
        String8 mSensorName;
        String8 mDeviceName;
    };

public:
    explicit CpfStore(const int cameraId);
    virtual ~CpfStore();
public:
    AiqConf AiqConfig;
    HalConf HalConfig;
private:
    status_t initFileNames(String8& cpfPathName, String8& sysfsPathName);
    status_t initDriverList();
    status_t initDriverListHelper(unsigned major, unsigned minor, SensorDriver& drvInfo);
    status_t findConfigWithDriver(String8& cpfName, int& drvIndex);
    status_t findConfigWithDriverHelper(const String8& fileName, String8& cpfName, int& index);
    status_t findBusAddress(const int drvIndex, int& i2cBus, int& i2cAddress);
    status_t initConf(CameraBlob& aiqConf, CameraBlob& drvConf, CameraBlob& halConf);
    status_t loadConf(CameraBlob& allConf);
    status_t validateConf(const CameraBlob& allConf, const struct stat& statCurrent);
    status_t fetchConf(const CameraBlob& allConf, CameraBlob& recConf, tbd_class_t recordClass, const char *blockDebugName = 0);
    status_t processAiqConf(CameraBlob& aiqConf);
    status_t processDrvConf(CameraBlob& drvConf);
    status_t processHalConf(CameraBlob& halConf);
private:
    int mCameraId;
    bool mIsOldConfig;
    String8 mCpfPathName;
    String8 mSysfsPathName;
    static Vector<struct SensorDriver> RegisteredDrivers;
    static Vector<struct stat> ValidatedCpfFiles;
    // Disallow copy and assignment
    CpfStore(const CpfStore&);
    void operator=(const CpfStore&);
};

}; // namespace android

#endif // ANDROID_LIBCAMERA_CAMERA_CONFIGURATION_H
