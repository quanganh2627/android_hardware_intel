/*****************************************************************************
INTEL CONFIDENTIAL
Copyright 2014 Intel Corporation All Rights Reserved.

The source code contained or described herein and all documents related to the
source code ("Material") are owned by Intel Corporation or its suppliers or
licensors. Title to the Material remains with Intel Corporation or its
suppliers and licensors. The Material may contain trade secrets and
proprietary and confidential information of Intel Corporation and its
suppliers and licensors, and is protected by worldwide copyright and trade
secret laws and treaty provisions. No part of the Material may be used, copied,
reproduced, modified, published, uploaded, posted, transmitted, distributed,
or disclosed in any way without Intel's prior express written permission.

No license under any patent, copyright, trade secret or other intellectual
property right is granted to or conferred upon you by disclosure or delivery of
the Materials, either expressly, by implication, inducement, estoppel or
otherwise. Any license under such intellectual property rights must be express
and approved by Intel in writing

File: bcu_cpufreq_release.c

Description: Application to release the capped cpu freq (scaling_max_freq) for
             all the available cpus.

Author: Albin B <albin.bala.krishnan@intel.com>

Date Created: 01 SEP 2014

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __ANDROID__
#define LOG_TAG                 "EM BCU App: "
#define LOG_NDEBUG              0
#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>
#else
#define ALOGD(format, ...) \
    printf("EM BCU App: " format, ## __VA_ARGS__)

#define ALOGI(format, ...) \
    printf("EM BCU App: " format, ## __VA_ARGS__)

#define ALOGE(format, ...) \
    printf("EM BCU App: " format, ## __VA_ARGS__)
#endif

#define CPU_DIR_PATH            "/sys/devices/system/cpu/"
#define CPUFREQ_DIR             "/cpufreq"
#define SCALING_MAXFREQ_FILE    "/scaling_max_freq"
#define SCALING_AVAILFREQ_FILE  "/scaling_available_frequencies"

#define MAX_DNAME_LEN                    64
#define MAX_CPUDIR_PATH_LEN              32
#define MAX_CPUFREQDIR_NAME_LEN          10
#define MAX_SCALINGMAXFREQ_FNAME_LEN     20
#define MAX_SCALINGAVAILFREQ_FNAME_LEN   32
#define SCALINGMAXFREQ_SYSFS_DATA_SIZE   64
#define SCALINGAVAILFREQ_SYSFS_DATA_SIZE 1024
#define MAXSYSFS_PATH_SIZE               1024

/**
 * check whether the input path is directory or not and returns true only if the
 * curent path is valid direcotry.
 */
bool is_dir(const char* path) {
    struct stat buf;
    stat(path, &buf);

    return S_ISDIR(buf.st_mode);
}

/**
 * Generic function to write the data into sysfs interface (path which is
 * provided as a input argument in path).
 * Returns: zero (0) - write success
 *          returns error code - write fails
 */
static int write_sysfs(char const *path, int data)
{
    char buf[80];
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        char buffer[SCALINGMAXFREQ_SYSFS_DATA_SIZE] = {'\0'};
        int bytes = snprintf(buffer, sizeof(buffer), "%d\n", data);
        int amt = write(fd, buffer, bytes);
        if (amt < 0) {
            strerror_r(errno, buf, sizeof(buf));
            ALOGE("Error in writing into %s: %s\n", path, buf);
        }
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error in opening %s: %s\n", path, buf);
        return -errno;
    }
}

/**
 * Generic function to read the sysfs interface (path which is provided as a
 * input argument in path) and returns with the read value as a string in *s.
 * Returns: number of bytes read - read success
 *          returns error code - read fails
 */
static int read_sysfs(char const *path, char *data, int num_bytes)
{
    char buf[80];
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        char buffer[20] = {'\0'};
        int amt = read(fd, data, num_bytes);
        if (amt < 0) {
            strerror_r(errno, buf, sizeof(buf));
            ALOGE("Error in reading from %s: %s\n", path, buf);
        }
        close(fd);
        return amt == -1 ? -errno : amt;
    } else {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error in opening %s: %s\n", path, buf);
        return -errno;
    }
}

/* getting the max freq value from the list of frequencies */
int get_maxfreq_from_list(int avail_freq[], int num)
{
    int max_freq = 0;
    int i;

    if (num > 0) {
        max_freq = avail_freq[0];
        for (i=0; i<num; i++) {
            if (avail_freq[i] > max_freq)
                max_freq = avail_freq[i];
        }
    }

    return max_freq;
}

/**
 * Reads the max freq value from the scaling_available_frequencies sysfs
 * interface and returns the same for the current cpu.
 */
int get_max_available_freq(char *cur_path)
{
    int ret;
    int num_freq = 0;
    int freq_list[256] = {'\0'};
    char *freq;
    char buf[SCALINGAVAILFREQ_SYSFS_DATA_SIZE] = {'\0'};

    strncat(cur_path, SCALING_AVAILFREQ_FILE, MAX_SCALINGAVAILFREQ_FNAME_LEN);
    ret = read_sysfs(cur_path, buf, SCALINGAVAILFREQ_SYSFS_DATA_SIZE);
    if (ret < 0) {
        ALOGE("Error in reading sysfs %s\n", cur_path);
    } else {
        freq = strtok (buf, " ");
        while (freq != NULL) {
            freq_list[num_freq++] = atoi(freq);
            freq = strtok (NULL, " ");
        }
        ret = get_maxfreq_from_list(freq_list, num_freq);
    }

    return ret;
}

/**
 * Reads the scaling_max_freq value from the sysfs interface and returns the
 * same for the current cpu.
 */
int get_scaling_max_freq(char *cur_path)
{
    int ret;
    char buf[SCALINGMAXFREQ_SYSFS_DATA_SIZE] = {'\0'};

    strncat(cur_path, SCALING_MAXFREQ_FILE, MAX_SCALINGMAXFREQ_FNAME_LEN);
    ret = read_sysfs(cur_path, buf, SCALINGMAXFREQ_SYSFS_DATA_SIZE);
    if (ret < 0)
        ALOGE("Error in reading sysfs %s\n", cur_path);
    else
        ret = atoi(buf);

    return ret;
}

int release_capped_freq(char *file, int freq)
{
    int ret = -EINVAL;

    if (freq > 0) {
        ret = write_sysfs(file, freq);
        if (ret < 0)
            ALOGE("Error in releasing capped freq (%s)\n", file);
        else
            ALOGI("Released the capped freq Successfully (%s: %d)\n",
                    file, freq);
    }

    return ret;
}

int do_release_freq(char *f_name, int cpunum)
{
    char cur_path[MAXSYSFS_PATH_SIZE] = {'\0'};
    char cur_cpu_path[MAXSYSFS_PATH_SIZE] = {'\0'};
    int scaling_max_freq;
    int max_avail_freq;
    int ret = 0;

    strncpy(cur_path, CPU_DIR_PATH, MAX_CPUDIR_PATH_LEN);
    strncat(cur_path, f_name, MAX_DNAME_LEN);

    if (!is_dir (cur_path)) {
        ALOGE("%s is not a direcotry\n", cur_path);
        ret = -ENOTDIR;
        goto error;
    }

    strncat(cur_path, CPUFREQ_DIR, MAX_CPUFREQDIR_NAME_LEN);
    strncpy(cur_cpu_path, cur_path,
               (MAXSYSFS_PATH_SIZE - MAX_SCALINGAVAILFREQ_FNAME_LEN));

    /* get the max scaling freq of the current cpu */
    scaling_max_freq = get_scaling_max_freq(cur_path);
    if (scaling_max_freq < 0) {
        ALOGE("Error in reading scaling_max_freq for cpu%d\n", cpunum);
        ret = scaling_max_freq;
        goto error;
    }
    ALOGI("Scaling_max_freq for cpu%d: %d\n", cpunum, scaling_max_freq);

    /* get the max value from the scaling available freq of the current cpu */
    max_avail_freq = get_max_available_freq(cur_cpu_path);
    if (max_avail_freq <= 0) {
        ALOGE("Error in reading scaling_available_frequencies for cpu%d\n", cpunum);
        ret = max_avail_freq;
        goto error;
    }
    ALOGD("Scaling Available Freq's max value for cpu%d: %d\n", cpunum, max_avail_freq);

    /**
     * Compare and check the max scaling freq along with the max freq of scaling
     * available frequencies. If the scaling cap freq is capped then release it
     * by writing the max value from the available scaling cpu freq to the
     * scaling_max_freq.
     */
    if (scaling_max_freq < max_avail_freq) {
        ret = release_capped_freq(cur_path, max_avail_freq);
        if (ret)
            ALOGE("Failed in releasing Capped Frequency for CPU%d\n", cpunum);
    }

error:
    return ret;
}

/**
 * main: Entry point of the executable.
 * bcu_cpufreqrel user app execution starts here. Also this needs to run for the
 * entire duration, it will not return.
 */
int main(int argc, char *argv[])
{
    DIR *dir;
    struct dirent *ent;
    char d_name[MAX_DNAME_LEN] = {'\0'};
    char *p;
    int ret;

    if ((dir = opendir(CPU_DIR_PATH)) == NULL) {
        /* could not open directory */
        ALOGE("Unable to Open %s\n", CPU_DIR_PATH);
        return -ENOENT;
    }

    /* read all the files and directories within directory */
    while ((ent = readdir(dir)) != NULL) {
        strncpy(d_name, ent->d_name, MAX_DNAME_LEN);
        p = strtok(d_name, "cpu");
        if(p == NULL)
            continue;

        if (isdigit(*p)) {
            /* check and release the max scaling freq, if capped */
            ret = do_release_freq(ent->d_name, atoi(p));
            if (ret)
                ALOGE("Error (%d) in do_release_freq\n", ret);
        }
    }

    closedir(dir);
    return 0;
}
