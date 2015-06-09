/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2014 Intel
 * Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or
 * disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */

#pragma once

#include <stdint.h>
#include <sys/types.h>
#include <utils/Errors.h>
#include <string>

class HalAudioDump
{
public:
    HalAudioDump();
    ~HalAudioDump();

    /**
     * Dumps the raw audio samples in a file. The name of the
     * audio file contains the infos on the dump characteristics.
     *
     * @param[in] buf const pointer the buffer to be dumped.
     * @param[in] bytes size in bytes to be written.
     * @param[in] isOutput  boolean for direction of the stream.
     * @param[in] samplingRate sample rate of the stream.
     * @param[in] channelNb number of channels in the sample spec.
     * @param[in] nameContext context of the dump to be appended in the name of the dump file.
     *
     **/
    void dumpAudioSamples(const void *buf,
                          ssize_t bytes,
                          bool isOutput,
                          uint32_t samplingRate, // Use SampleSpec class as soon as it
                                                 // is unbound to the route manager, to
                                                 // avoid circular dependencies
                          uint32_t channelNb,
                          const std::string &nameContext);

    void close();

private:
    /**
     * Returns the string of the stream direction.
     *
     * @param[in] isOut true for input stream, false for input.
     *
     * @return direction name.
     */
    const char *streamDirectionStr(bool isOut) const;

    /**
     * Checks if dump file is ready for operation.
     * It not only checks the validity and rights of the dump file, but also
     * if the size will not reach the maximum allowed size.
     *
     * @param[in] bytes size expected to be written in the dump file.
     *
     * @return OK if ready to write, error code otherwise.
     */
    android::status_t checkDumpFile(ssize_t bytes);

    /**
     * Writes the PCM samples in the dump file.
     * Checks that file size does not exceed its maximum size.
     *
     * @param[in] buf const pointer to the buffer to be dumped.
     * @param[in] bytes size in bytes to be written.
     *
     * @return error code.
     **/
    android::status_t writeDumpFile(const void *buf,
                                    ssize_t bytes);

    FILE *mDumpFile;
    uint32_t mFileCount;

    /**
     * Maximum number of files per dump instance.
     */
    static const uint32_t mMaxNumberOfFiles;

    /**
     * Store stream directions in and out.
     */
    static const char *mStreamDirections[];

    /**
     * Dump directory path to store audio dump files.
     */
    static const char *mDumpDirPath;

    /**
     * Limit file size to about 20 MB for audio dump files
     * to avoid filling the mass storage to the brim.
     */
    static const uint32_t mMaxDumpFileSize = 20 * 1024 * 1024;
};
