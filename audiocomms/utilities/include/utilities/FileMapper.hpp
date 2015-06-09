/*
 * Copyright 2014 Intel Corporation
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
#pragma once

#include "result/ErrnoResult.hpp"
#include "NonCopyable.hpp"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <string>


namespace audio_comms
{
namespace utilities
{

/** Map a File in memory.
 *
 *  tparam T the type of the resulting data array
 */
template <class T>
class FileMapper : public NonCopyable
{

public:

    enum FailureCode
    {
        Success = 444,
        Unknown,
        NoSuchFile,
        MemoryError
    };

    struct FileMapperStatus
    {
        typedef FailureCode Code;

        /** Enum coding the failures that can occur in the class methods. */
        const static Code success = Success;
        const static Code defaultError = Unknown;

        static std::string codeToString(const Code &code)
        {
            switch (code) {
            case Success:
                return "Success";
            case Unknown:
                return "Unknown error";
            case NoSuchFile:
                return "No such file";
            case MemoryError:
                return "Memory error";
            }
            /* Unreachable, prevents gcc to complain */
            return "Invalid error (Unreachable)";
        }
    };

    /** The type of the method returns. */
    typedef result::Result<FileMapperStatus> Result;

    /** Mapper Constructor.
     *
     *  @param[in] fileName the name of the file to map.
     */
    FileMapper(const char *fileName);

    /** The mapper keep the owning of the resulting data array.  */
    ~FileMapper();

    /** Launch the mapping
     *
     *  @return the Result object containing information about mapping
     */
    Result map();

    /** Mapped File size getter */
    size_t getMappedFileSize() const;

    /** Mapped file getter */
    const T *getMappedFile() const;

private:
    enum FileState
    {
        Closed = 999,
        Opened,
        Mapped,
    };

    const char *const mFileName;

    /** The resulting data array */
    T *mMappedFile;

    /** Current state of the file, usefull to clean memory */
    FileState mFileState;

    int mFileDescriptor;
    size_t mFileSize;
};

template <class T>
FileMapper<T>::FileMapper(const char *fileName)
    : mFileName(fileName), mMappedFile(NULL), mFileState(Closed),
      mFileDescriptor(-1), mFileSize(0)
{}

template <class T>
FileMapper<T>::~FileMapper()
{

    if (mFileState == Mapped) {
        munmap(mMappedFile, mFileSize);
        mFileState = Opened;
    }

    if (mFileState == Opened) {
        close(mFileDescriptor);
    }
}

template <class T>
size_t FileMapper<T>::getMappedFileSize() const
{
    // FileSize is initialized to 0, no need to check the file state
    return mFileSize / sizeof(T);
}

template <class T>
const T *FileMapper<T>::getMappedFile() const
{
    if (mFileState == Mapped) {
        return mMappedFile;
    }

    return NULL;
}

template <class T>
typename FileMapper<T>::Result FileMapper<T>::map()
{
    using utilities::result::ErrnoResult;

    // Open the file
    mFileDescriptor = open(mFileName, O_RDONLY);
    if (mFileDescriptor < 0) {
        return Result(NoSuchFile) << mFileName << ErrnoResult(errno);
    }
    mFileState = Opened;

    // Get File size
    struct stat fileStat;
    if ((fstat(mFileDescriptor, &fileStat) == -1)) {
        return Result(Unknown) << "fstat fails on" << mFileName << ErrnoResult(errno);
    }
    mFileSize = fileStat.st_size;

    // Map the file in Memory
    mMappedFile = static_cast<T *>(mmap(
                                       NULL,
                                       mFileSize,
                                       PROT_READ,
                                       MAP_PRIVATE,
                                       mFileDescriptor,
                                       0));
    if (mMappedFile == MAP_FAILED) {
        return Result(MemoryError) << "while mapping " << mFileName << ErrnoResult(errno);
    }
    mFileState = Mapped;

    return Result::success();
}

}
}
