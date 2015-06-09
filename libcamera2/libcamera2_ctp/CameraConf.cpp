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
#define LOG_TAG "Camera_Conf"

#include "CameraConf.h"
#include "LogHelper.h"
#include "PlatformData.h"
#include <dirent.h>        // DIR, dirent
#include <fnmatch.h>       // fnmatch()
#include <fcntl.h>         // open(), close()
#include <linux/media.h>   // media controller
#include <linux/kdev_t.h>  // MAJOR(), MINOR()
#include <utils/Errors.h>  // Error codes

namespace android {

using namespace CPF;

const char *cpfConfigPath = "/etc/atomisp/";  // Where CPF files are located
// FIXME: The spec for following is "dr%02d[0-9][0-9]??????????????.cpf"
const char *cpfConfigPattern = "%02d*.cpf";  // How CPF file name should look

// Defining and initializing static members
Vector<struct CpfStore::SensorDriver> CpfStore::RegisteredDrivers;
Vector<struct stat> CpfStore::ValidatedCpfFiles;

CameraBlob::Blob::Blob(const int size, void *& ptr)
{
    ptr = 0;
    if (size == 0) {
        mPtr = 0;
    } else {
        ptr = mPtr = malloc(size);
    }
}

CameraBlob::Blob::~Blob()
{
    free(mPtr);
    mPtr = NULL;
}

CameraBlob::CameraBlob(const int size)
{
    mSize = 0;
    mPtr = 0;
    if (size) {
        mBlob = new Blob(size, mPtr);
        if (mBlob != 0) {
            if (mPtr) {
                mSize = size;
            } else {
                mBlob.clear();
            }
        }
    }
}

CameraBlob::CameraBlob(const CameraBlob& refBlob, const int offset, const int size)
{
    mSize = 0;
    mPtr = 0;
    if (refBlob.mBlob == 0) {
        ALOGE("ERROR referring to null object!");
        return;
    }
    // Must refer only to memory allocated by reference object
    if (refBlob.size() < offset + size) {
        ALOGE("ERROR illegal allocation!");
        return;
    }
    mBlob = refBlob.mBlob;
    mSize = size;
    mPtr = (char *)(refBlob.ptr()) + offset;
}

CameraBlob::CameraBlob(const CameraBlob& refBlob, void * const ptr, const int size)
{
    mSize = 0;
    mPtr = 0;
    if (refBlob.mBlob == 0) {
        ALOGE("ERROR referring to null object!");
        return;
    }
    // Must refer only to memory allocated by reference object
    int offset = (char *)(ptr) - (char *)(refBlob.ptr());
    if ((offset < 0) || (offset + size > refBlob.size())) {
        ALOGE("ERROR illegal allocation!");
        return;
    }
    mBlob = refBlob.mBlob;
    mSize = size;
    mPtr = ptr;
}

CameraBlob CameraBlob::copy()
{
    CameraBlob newBlob(size());
    if (newBlob && ptr() && size()) {
        memcpy(newBlob, ptr(), size());
    }
    return newBlob;
}

void CameraBlob::clear()
{
     mBlob.clear();
     mSize = 0;
     mPtr = 0;
}

// Common macro to all functions retrieving values from CPF HAL data
#define GETANY(status, anyPtr, type, tag, warn_if_fail)             \
    status_t status;                                                \
                                                                    \
    va_list args;                                                   \
    va_start(args, tag);                                            \
                                                                    \
    switch ((status = getAny(anyPtr, type, tag, args))) {           \
    case BAD_VALUE:                                                 \
    case BAD_TYPE:                                                  \
    default:                                                        \
        ALOGE("ERROR %d in %s, for tag %d of type 0x%08x!", status, __FUNCTION__, tag, type); \
    case OK:                                                        \
        break;                                                      \
    case NO_INIT:                                                   \
    case NAME_NOT_FOUND:                                            \
        if (warn_if_fail) {                                         \
            ALOGW("WARNING %d in %s, using default value for tag %d!", status, __FUNCTION__, tag); \
        }                                                           \
    }                                                               \
                                                                    \
    va_end(args);
// End of macro to functions retrieving values from CPF HAL data

status_t HalConf::getValue(int& value, cpf_hal_tag_t tag, ...)
{
    GETANY(ret, &value, tag_value, tag, 0);
    return ret;
}

status_t HalConf::getBool(bool& boolean, cpf_hal_tag_t tag, ...)
{
    GETANY(ret, &boolean, tag_bool, tag, 0);
    return ret;
}

status_t HalConf::getString(const char *& string, cpf_hal_tag_t tag, ...)
{
    GETANY(ret, &string, tag_string, tag, 0);
    return ret;
}

status_t HalConf::getFpoint(int32_t& value, cpf_hal_tag_t tag, ...)
{
    GETANY(ret, &value, tag_fpoint, tag, 0);
    return ret;
}

status_t HalConf::getFloat(float& value, cpf_hal_tag_t tag, ...)
{
    GETANY(ret, &value, tag_float, tag, 0);
    return ret;
}

int HalConf::getValue(cpf_hal_tag_t tag, ...)
{
    int value = 0;
    GETANY(dummy, &value, tag_value, tag, 1);
    return value;
}

bool HalConf::getBool(cpf_hal_tag_t tag, ...)
{
    bool boolean = false;
    GETANY(dummy, &boolean, tag_bool, tag, 1);
    return boolean;
}

const char *HalConf::getString(cpf_hal_tag_t tag, ...)
{
    const char *stringPtr = 0;
    GETANY(dummy, &stringPtr, tag_string, tag, 1);
    return stringPtr;
}

int32_t HalConf::getFpoint(cpf_hal_tag_t tag, ...)
{
    int32_t value = 0;
    GETANY(dummy, &value, tag_fpoint, tag, 1);
    return value;
}

float HalConf::getFloat(cpf_hal_tag_t tag, ...)
{
    float value = 0;
    GETANY(dummy, &value, tag_float, tag, 1);
    return value;
}

status_t HalConf::getAny(void *anyPtr, cpf_hal_tagtype_t type, CPF::cpf_hal_tag_t tag, va_list args)
{
    // In case HAL CPF is not present, we will see that ptr == 0
    if (ptr() == 0) {
        return NO_INIT;
    }

    cpf_hal_header_t *headerPtr = (cpf_hal_header_t *)(ptr());
    const int32_t *dataPtr   = (int32_t *)((char *)(headerPtr) + headerPtr->data_offset);
    const int32_t *tablePtr  = (int32_t *)((char *)(headerPtr) + headerPtr->table_offset);
    const char    *stringsPtr = (char *)(headerPtr) + headerPtr->string_offset;

    if (tag & 0xffff0000) {
        return BAD_VALUE;
    }

    if ((headerPtr->tags_count == 0) || (tag < headerPtr->tags_min) || (tag > headerPtr->tags_max)) {
        return NAME_NOT_FOUND;
    }

    const int32_t *flaggedTagPtr;
    if (headerPtr->flags & sparse_en) {
        int count = headerPtr->tags_count;
        for (int i = 0; i < count; i++) {
            flaggedTagPtr = dataPtr + 2 * i;
            if (tag == (*flaggedTagPtr & 0xffff)) {
                goto loop;
            }
        }
        return NAME_NOT_FOUND;
    } else {
        flaggedTagPtr = dataPtr + 2 * (tag - headerPtr->tags_min);
    }

loop:
    if (*flaggedTagPtr & tag_unused) {
        return NAME_NOT_FOUND;
    }

    if (*flaggedTagPtr & tag_table) {
        int32_t *newTablePtr = (int32_t *)((char *)(tablePtr) + *(flaggedTagPtr + 1));
        int count = *newTablePtr++;
        tag = cpf_hal_tag_t(va_arg(args, int));
        if (tag & 0xffff0000) {
            return BAD_VALUE;
        }
        for (int i = 0; i < count; i++) {
            flaggedTagPtr = newTablePtr + 2 * i;
            if (tag == (*flaggedTagPtr & 0xffff)) {
                goto loop;
            }
        }
        return NAME_NOT_FOUND;
    }

    // Use of "tag_value" wasn't mandatory in old CPF files, so we are
    // ignoring it for now. Also, floats are represented as fpoints...
    cpf_hal_tagtype_t testType = (type == tag_float ? tag_fpoint : type);
    if (((*flaggedTagPtr++ & 0xffff0000) ^ testType) & ~tag_value) {
        return BAD_TYPE;
    }

    // Different types need to be handled differently
    const int32_t &value = *flaggedTagPtr;
    switch (type) {
    case tag_string:
        *((const char **)(anyPtr)) = stringsPtr + value;
        break;
    case tag_value:
        *((int *)(anyPtr)) = value;
        break;
    case tag_bool:
        *((bool *)(anyPtr)) = value;
        break;
    case tag_fpoint:
        *((int32_t *)(anyPtr)) = value;
        break;
    case tag_float:
        *((float *)(anyPtr)) = (float)(value) / 65536.0;
        break;
    default:
        return BAD_VALUE;
    }

    return 0;
}

CpfStore::CpfStore(const int cameraId)
    : mCameraId(cameraId)
    , mIsOldConfig(false)
{
    CameraBlob aiqConf, drvConf, halConf;

    // If anything goes wrong here, we simply return silently.
    // CPF should merely be seen as a way to do multiple configurations
    // at once; failing in that is not a reason to return with errors
    // and terminate the camera (some cameras may not have any CPF at all)

    if (mCameraId >= PlatformData::numberOfCameras()) {
        ALOGE("ERROR bad camera index!");
        mCameraId = -1;
        return;
    }

    // Find out the related file names
    if (initFileNames(mCpfPathName, mSysfsPathName)) {
        // Error message given already
        return;
    }

    // Obtain the configurations
    if (initConf(aiqConf, drvConf, halConf)) {
        // Error message given already
        return;
    }

    // Provide configuration data for algorithms and image
    // quality purposes, and continue further even if errors did occur.
    // Pointer to that data is cleared later, whenever seen suitable,
    // so that the memory reserved for CPF data can then be freed
    processAiqConf(aiqConf);

    // Provide configuration data to driver, and continue further even
    // if errors did occur. Clean pointer to that data, so that
    // the memory reserved for CPF data can then be freed
    processDrvConf(drvConf);

    // Process (make a copy of...) configuration data to HAL, and
    // continue further even if errors did occur. Clean pointer to
    // that data, so that the memory reserved for CPF data can then
    // be freed
    processHalConf(halConf);
}

CpfStore::~CpfStore()
{
}

status_t CpfStore::initFileNames(String8& cpfPathName, String8& sysfsPathName)
{
    status_t ret = 0;

    // First, we see what drivers we have in the system
    if ((ret = initDriverList())) {
        // Error message given already
        return ret;
    }

    // Secondly, we will find a matching configuration file
    int drvIndex = -1;   // Index of registered driver for which CPF file exists
    String8 cpfFileName; // Name of the corresponding CPF config file
    if ((ret = findConfigWithDriver(cpfFileName, drvIndex))) {
        // Error message given already
        return ret;
    }

    // Thirdly, we will find out the I²C bus and address for the driver
    int i2cBus;
    int i2cAddress;
    if ((ret = findBusAddress(drvIndex, i2cBus, i2cAddress))) {
        // Error message given already
        return ret;
    }

    // Here is the correct CPF file
    cpfPathName = cpfConfigPath;
    cpfPathName.appendPath(cpfFileName);

    // Here is the correct sysfs file
    const char *sysfsPath = "/sys/class/i2c-dev/i2c-%d/device/%d-%04x/sensordata";
    sysfsPathName = String8::format(sysfsPath, i2cBus, i2cBus, i2cAddress);

    ALOGD("cpf config file name: %s", cpfPathName.string());
    ALOGD("cpf sysfs file name: %s", sysfsPathName.string());

    return ret;
}

status_t CpfStore::initDriverList()
{
    status_t ret = 0;

    if (RegisteredDrivers.size() > 0) {
        // We only need to go through the drivers once
        return 0;
    }

    // Sensor drivers have been registered to media controller
    const char *mcPathName = "/dev/media0";
    int fd = open(mcPathName, O_RDONLY);
    if (fd == -1) {
        ALOGE("ERROR in opening media controller: %s!", strerror(errno));
        return ENXIO;
    }

    struct media_entity_desc entity;
    memset(&entity, 0, sizeof(entity));
    do {
        // Go through the list of media controller entities
        entity.id |= MEDIA_ENT_ID_FLAG_NEXT;
        if (ioctl(fd, MEDIA_IOC_ENUM_ENTITIES, &entity) < 0) {
            if (errno == EINVAL) {
                // Ending up here when no more entities left.
                // Will simply 'break' if everything was ok
                if (RegisteredDrivers.size() == 0) {
                    // No registered drivers found
                    ALOGE("ERROR no sensor driver registered in media controller!");
                    ret = NO_INIT;
                }
            } else {
                ALOGE("ERROR in browsing media controller entities: %s!", strerror(errno));
                ret = FAILED_TRANSACTION;
            }
            break;
        } else {
            if (entity.type == MEDIA_ENT_T_V4L2_SUBDEV_SENSOR) {
                // A driver has been found!
                // The driver is using sensor name when registering
                // to media controller (we will truncate that to
                // first space, if any); but we also have to find the
                // proper driver name for sysfs usage
                SensorDriver drvInfo;
                drvInfo.mSensorName = entity.name;
                // Cut the name to first space
                for (int i = 0; (i = drvInfo.mSensorName.find(" ")) > 0; drvInfo.mSensorName.setTo(drvInfo.mSensorName, i));

                unsigned major = entity.v4l.major;
                unsigned minor = entity.v4l.minor;

                // Go thru the subdevs one by one, see which one
                // corresponds to this driver (if there is an error,
                // the looping ends at 'while')
                ret = initDriverListHelper(major, minor, drvInfo);
            }
        }
    } while (!ret);

    if (close(fd)) {
        ALOGE("ERROR in closing media controller: %s!", strerror(errno));
        if (!ret) ret = EPERM;
    }

    return ret;
}

status_t CpfStore::initDriverListHelper(unsigned major, unsigned minor, SensorDriver& drvInfo)
{
    String8 subdevPathNameN;

    const char *subdevPathName = "/dev/v4l-subdev%d";
    for (int n = 0; true; n++) {
        subdevPathNameN = String8::format(subdevPathName, n);
        struct stat fileInfo;
        if (stat(subdevPathNameN, &fileInfo) < 0) {
            // We end up here when there are no more subdevs
            if (errno == ENOENT) {
                ALOGE("ERROR sensor subdev missing: \"%s\"!", subdevPathNameN.string());
                return NO_INIT;
            } else {
                ALOGE("ERROR querying sensor subdev filestat for \"%s\": %s!", subdevPathNameN.string(), strerror(errno));
                return FAILED_TRANSACTION;
            }
        }
        if ((major == MAJOR(fileInfo.st_rdev)) && (minor == MINOR(fileInfo.st_rdev))) {
            drvInfo.mDeviceName = subdevPathNameN.getPathLeaf();
            RegisteredDrivers.push(drvInfo);
            ALOGD("Registered sensor driver \"%s\" found for sensor \"%s\"", drvInfo.mDeviceName.string(), drvInfo.mSensorName.string());
            // All ok
            break;
        }
    }

    return 0;
}

status_t CpfStore::findConfigWithDriver(String8& cpfName, int& drvIndex)
{
    status_t ret = 0;
    bool anyMatch = false;

    // We go the directory containing CPF files thru one by one file,
    // and see if a particular file is something to react upon. If yes,
    // we then see if there is a corresponding driver registered. It
    // is allowed to have more than one CPF file for particular driver
    // (spId values are used for further distinguishing in that case),
    // but having more than one suitable driver registered is a strict no no...

    DIR *dir = opendir(cpfConfigPath);
    if (!dir) {
        ALOGE("ERROR in opening CPF folder \"%s\": %s!", cpfConfigPath, strerror(errno));
        return ENOTDIR;
    }

    do {
        dirent *entry;
        if (errno = 0, !(entry = readdir(dir))) {
            if (errno) {
                ALOGE("ERROR in browsing CPF folder \"%s\": %s!", cpfConfigPath, strerror(errno));
                ret = FAILED_TRANSACTION;
            }
            // If errno was not set, return 0 means end of directory.
            // So, let's see if we found any (suitable) CPF files
            if (drvIndex < 0) {
                if (anyMatch) {
                    ALOGE("NOTE no suitable CPF files found in CPF folder \"%s\" (ok for SOC cameras)", cpfConfigPath);
                } else {
                    ALOGE("NOTE not a single CPF file found in CPF folder \"%s\" (ok for SOC cameras)", cpfConfigPath);
                }
                ret = NO_INIT;
            }
            break;
        }
        if ((strcmp(entry->d_name, ".") == 0) ||
            (strcmp(entry->d_name, "..") == 0)) {
            continue;  // Skip self and parent
        }
        String8 pattern = String8::format(cpfConfigPattern, mCameraId);
        int r = fnmatch(pattern, entry->d_name, 0);
        switch (r) {
        case 0:
            // The file name looks like a valid CPF file name
            anyMatch = true;
            // See if we have corresponding driver registered
            // (if there is an error, the looping ends at 'while')
            ret = findConfigWithDriverHelper(String8(entry->d_name), cpfName, drvIndex);
            continue;
        case FNM_NOMATCH:
            // The file name did not look like a CPF file name
            continue;
        default:
            // Unknown error (the looping ends at 'while')
            ALOGE("ERROR in pattern matching file name \"%s\"!", entry->d_name);
            ret = UNKNOWN_ERROR;
            continue;
        }
    } while (!ret);

    if (closedir(dir)) {
        ALOGE("ERROR in closing CPF folder \"%s\": %s!", cpfConfigPath, strerror(errno));
        if (!ret) ret = EPERM;
    }

    return ret;
}

status_t CpfStore::findConfigWithDriverHelper(const String8& fileName, String8& cpfName, int& index)
{
    status_t ret = 0;

    for (int i = RegisteredDrivers.size(); i-- > 0; ) {
        if (fileName.find(RegisteredDrivers[i].mSensorName) < 0) {
            // Name of this registered driver was not found
            // from within CPF looking file name -> skip it
            continue;
        } else {
            // Since we are here, we do have a registered
            // driver whose name maps to this CPF file name
            if (index < 0) {
                // No previous CPF<>driver pairs
                index = i;
                cpfName = fileName;
            } else {
                if (index == i) {
                    // Multiple CPF files match the driver
                    // If there are cpf files for different products with the same sensor name,
                    // the files are distinguished by spId
                    // Let's check for the vendor_id, platform_family_id and product_line_id
                    String8 vendorPlatformProduct;
                    if ((PlatformData::createVendorPlatformProductName(vendorPlatformProduct) == 0) &&
                        (fileName.find(vendorPlatformProduct) >= 0)) {
                            cpfName = fileName;
                    }
                } else {
                    // We just got lost:
                    // Which is the correct sensor driver?
                    ALOGE("ERROR multiple driver candidates for CPF file \"%s\"!", fileName.string());
                    ret = ENOTUNIQ;
                }
            }
        }
    }

    return ret;
}

status_t CpfStore::findBusAddress(const int drvIndex, int& i2cBus, int& i2cAddress)
{
    FILE *file;
    status_t ret = 0;

    const char *sysfsInfoPathName = "/sys/class/video4linux/%s/name";
    String8 i2cInfoPathName = String8::format(sysfsInfoPathName, RegisteredDrivers[drvIndex].mDeviceName.string());

    file = fopen(i2cInfoPathName, "rb");
    if (!file) {
        ALOGE("ERROR in opening file \"%s\" for I²C info: %s!", i2cInfoPathName.string(), strerror(errno));
        return NAME_NOT_FOUND;
    }

    do {
        if (fscanf(file, " %*s %d-%x", &i2cBus, &i2cAddress) < 2) {
            ALOGE("ERROR reading file \"%s\"!", i2cInfoPathName.string());
            ret = EIO;
            break;
        }

    } while (0);

    if (fclose(file)) {
        ALOGE("ERROR in closing file \"%s\": %s!", i2cInfoPathName.string(), strerror(errno));
        if (!ret) ret = EPERM;
    }

    return ret;
}

status_t CpfStore::initConf(CameraBlob& aiqConf, CameraBlob& drvConf, CameraBlob& halConf)
{
    CameraBlob allConf;
    status_t ret = 0;

    // First, we load the correct configuration file.
    // It will be behind reference counted MemoryHeapBase
    // object "allConf", meaning that the memory will be
    // automatically freed when it is no longer being pointed at
    if ((ret = loadConf(allConf)))
        return ret;

    // Then, we will dig out component specific configuration
    // data from within "allConf". That will be placed behind
    // reference counting MemoryBase memory descriptors.
    // We only need to verify checksum once
    if ((ret = fetchConf(allConf, aiqConf, tbd_class_aiq, "AIQ")))
        return ret;
    if ((ret = fetchConf(allConf, drvConf, tbd_class_drv, "DRV")))
        return ret;
    if ((ret = fetchConf(allConf, halConf, tbd_class_hal, "HAL")))
        return ret;

    return ret;
}

status_t CpfStore::loadConf(CameraBlob& allConf)
{
    FILE *file;
    struct stat statCurrent;
    status_t ret = 0;

    ALOGD("Opening CPF file \"%s\"", mCpfPathName.string());
    file = fopen(mCpfPathName, "rb");
    if (!file) {
        ALOGE("ERROR in opening CPF file \"%s\": %s!", mCpfPathName.string(), strerror(errno));
        return NAME_NOT_FOUND;
    }

    do {
        int fileSize;
        if ((fseek(file, 0, SEEK_END) < 0) ||
            ((fileSize = ftell(file)) < 0) ||
            (fseek(file, 0, SEEK_SET) < 0)) {
            ALOGE("ERROR querying properties of CPF file \"%s\": %s!", mCpfPathName.string(), strerror(errno));
            ret = ESPIPE;
            break;
        }

        allConf = CameraBlob(fileSize);
        if (!allConf) {
            ALOGE("ERROR no memory in %s!", __func__);
            ret = NO_MEMORY;
            break;
        }

        if (fread(allConf, fileSize, 1, file) < 1) {
            ALOGE("ERROR reading CPF file \"%s\"!", mCpfPathName.string());
            ret = EIO;
            break;
        }

        // We use file statistics for file identification purposes.
        // The access time was just changed (because of us!),
        // so let's nullify the access time info
        if (stat(mCpfPathName, &statCurrent) < 0) {
            ALOGE("ERROR querying filestat of CPF file \"%s\": %s!", mCpfPathName.string(), strerror(errno));
            ret = FAILED_TRANSACTION;
            break;
        }
        statCurrent.st_atime = 0;
        statCurrent.st_atime_nsec = 0;

    } while (0);

    if (fclose(file)) {
        ALOGE("ERROR in closing CPF file \"%s\": %s!", mCpfPathName.string(), strerror(errno));
        if (!ret) ret = EPERM;
    }

    if (!ret) {
        ret = validateConf(allConf, statCurrent);
    }

    return ret;
}

status_t CpfStore::validateConf(const CameraBlob& allConf, const struct stat& statCurrent)
{
    // In case the very same CPF configuration file has been verified
    // already earlier, checksum calculation will be skipped this time.
    // Files are identified by their stat structure. If we set the
    // cache size equal to number of cameras in the system, checksum
    // calculations are avoided when user switches between cameras.
    // Note: the capacity could be set to zero as well if one wants
    // to validate the file in every case
    ValidatedCpfFiles.setCapacity(PlatformData::numberOfCameras());
    bool& canSkipChecksum = mIsOldConfig = false;

    // See if we know the file already
    for (int i = ValidatedCpfFiles.size() - 1; i >= 0; i--) {
        if (!memcmp(&ValidatedCpfFiles[i], &statCurrent, sizeof(struct stat))) {
            canSkipChecksum = true;
            break;
        }
    }

    if (canSkipChecksum) {
        ALOGD("CPF file already validated");
    } else {
        ALOGD("CPF file not validated yet, validating...");
        if (tbd_validate(allConf, allConf.size(), tbd_tag_cpff)) {
            // Error, looks like we had unknown file
            ALOGE("ERROR corrupted CPF file!");
            return DEAD_OBJECT;
        }
    }

    // If we are here, the file was ok. If it wasn't cached already,
    // then do so now (adding to end of cache, removing from beginning)
    if (!canSkipChecksum) {
        if (ValidatedCpfFiles.size() < ValidatedCpfFiles.capacity()) {
            ValidatedCpfFiles.push_back(statCurrent);
        } else {
            if (ValidatedCpfFiles.size() > 0) {
                ValidatedCpfFiles.removeAt(0);
                ValidatedCpfFiles.push_back(statCurrent);
            }
        }
    }

    return 0;
}

status_t CpfStore::fetchConf(const CameraBlob& allConf, CameraBlob& recConf, tbd_class_t recordClass, const char *blockDebugName)
{
    status_t ret = 0;

    if (!allConf) {
        // This should never happen; CPF file has not been loaded properly
        ALOGE("ERROR null pointer provided!");
        return NO_MEMORY;
    }

    // The contents have been validated already, let's look for specific record
    void *data;
    size_t size;
    if (!(ret = tbd_get_record(allConf, recordClass, tbd_format_any, &data, &size))) {
        if (data && size) {
            recConf = CameraBlob(allConf, data, size);
            if (!recConf) {
                ALOGE("ERROR no memory in %s!", __func__);
                return NO_MEMORY;
            } else {
                ALOGD("CPF %s record found!", blockDebugName);
            }
        } else {
            // Looks like we didn't have the requested record in CPF file
            ALOGD("CPF %s record missing!", blockDebugName);
        }
    }

    return ret;
}

status_t CpfStore::processAiqConf(CameraBlob& aiqConf)
{
    AiqConfig = aiqConf;
    return 0;
}

status_t CpfStore::processDrvConf(CameraBlob& drvConf)
{
    // Only act if CPF file has been updated and there is some data
    // to be sent
    if (mIsOldConfig || !drvConf) {
        return 0;
    }

    status_t ret = 0;

    // We are only interested in actual HAL data, not the header
    void *data;
    size_t size;
    if (tbd_get_record(drvConf, tbd_class_drv, tbd_format_any, &data, &size) || (data == 0) || (size == 0)) {
        // Looks like the DRV record was broken
        ALOGE("ERROR corrupted DRV record!");
        return DEAD_OBJECT;
    }

    // There is a limitation in sysfs; maximum data size to be sent
    // is one page
    if (size > (size_t)(getpagesize())) {
        ALOGE("ERROR too big driver configuration record!");
        return EOVERFLOW;
    }

    // Now, let's write the driver configuration data via sysfs
    ALOGD("Writing %d bytes to sysfs file \"%s\"", size, mSysfsPathName.string());
    int fd = open(mSysfsPathName, O_WRONLY);
    if (fd == -1) {
        ALOGE("ERROR in opening sysfs write file \"%s\": %s!", mSysfsPathName.string(), strerror(errno));
        return NO_INIT;
    }

    // Writing the driver data record...
    int bytes = write(fd, data, size);
    if (bytes < 0) {
        ALOGE("ERROR in writing sysfs data: %s!", strerror(errno));
        ret = EIO;
    } else if ((bytes == 0) || ((size_t)(bytes) != size)) {
        ALOGE("ERROR in writing sysfs data: %d bytes written (expecting %d)!", bytes, size);
        ret = EIO;
    }

    if (close(fd)) {
        ALOGE("ERROR in closing sysfs write file \"%s\": %s!", mSysfsPathName.string(), strerror(errno));
        if (!ret) ret = EPERM;
    }

    return ret;
}

status_t CpfStore::processHalConf(CameraBlob& halConf)
{
    if (halConf) {
        // We are only interested in actual HAL data, not the header
        void *data;
        size_t size;
        if (tbd_get_record(halConf, tbd_class_hal, tbd_format_any, &data, &size) || (data == 0) || (size == 0)) {
            // Looks like the HAL record was broken
            ALOGE("ERROR corrupted HAL record!");
            return DEAD_OBJECT;
        }
        // CPF HAL contains lot of strings, so the easiest way to allow
        // freeing of the original CPF data (with AIQ and DRV data) and
        // still having the strings stored somewhere is to make a copy
        // of the entire CPF HAL data
        HalConfig = CameraBlob(halConf, data, size).copy();
        if (!HalConfig) {
            ALOGE("ERROR no memory in %s!", __func__);
            return NO_MEMORY;
        }

    }

    return 0;
}

} // namespace android
