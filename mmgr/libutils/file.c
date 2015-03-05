/* Modem Manager - file source file
**
** Copyright (C) Intel 2012
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
*/

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include "errors.h"
#include "file.h"
#include "logs.h"

#define MASK_ALL (S_IRWXU | S_IRWXG | S_IRWXO)

/**
 * write a string to a file
 *
 * @param [in] path complete file path
 * @param [in] mode open permissions
 * @param [in] value string to write
 * @param [in] size string size
 *
 * @return E_ERR_FAILED if open fails
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t file_write(char *path, unsigned long mode, char *value,
                           size_t size)
{
    int fd;
    e_mmgr_errors_t ret = E_ERR_FAILED;
    ssize_t write_size = 0;

    ASSERT(path != NULL);
    ASSERT(value != NULL);

    fd = open(path, O_WRONLY | O_CREAT, mode);
    if (fd < 0) {
        LOG_ERROR("open of (%s) failed (%s)", path, strerror(errno));
    } else {
        if (size > 0)
            write_size = write(fd, value, size);
        if ((close(fd) == 0) && ((size_t)write_size == size)) {
            ret = E_ERR_SUCCESS;
            LOG_DEBUG("write to (%s) succeed", path);
        } else {
            LOG_ERROR("write to (%s) failed. (%s)", path, strerror(errno));
        }
    }

    return ret;
}

/**
 * Look for file
 *
 * @param [in] path file path
 * @param [in] rights file rights should be equal to rights. if rights is 0 the
 * check is not performed
 *
 * @return false by default
 * @return true if file exist
 */
bool file_exist(const char *path, unsigned long rights)
{
    struct stat st;
    bool result = false;

    ASSERT(path != NULL);

    if (!stat(path, &st) && S_ISREG(st.st_mode)) {
        result = true;
        if (rights && ((st.st_mode & MASK_ALL) != (rights & ~MMGR_UMASK))) {
            LOG_DEBUG("bad file permissions");
            result = false;
        }
    }

    return result;
}

/**
 * Copies a file from specified source to destination.
 *
 * @param [in] src Source file to copy.
 * @param [in] dst Destination file to copy to.
 * @param [in] dst_mode Mode to give to destination file.
 *
 * @return E_ERR_FAILED file not copied
 * @return E_ERR_SUCCESS file copied
 */
e_mmgr_errors_t file_copy(const char *src, const char *dst, mode_t dst_mode)
{
    int in_fd = -1;
    int out_fd = -1;
    e_mmgr_errors_t ret = E_ERR_SUCCESS;
    mode_t old_umask = 0;
    struct stat sb;
    off_t offset = 0;

    ASSERT(src != NULL);
    ASSERT(dst != NULL);

    old_umask = umask(~dst_mode & 0777);

    in_fd = open(src, O_RDONLY);
    if (in_fd < 0) {
        ret = E_ERR_FAILED;
        LOG_DEBUG("Cannot open source file, errno = %d", errno);
        goto out;
    }

    out_fd = open(dst, O_CREAT | O_WRONLY | O_TRUNC, dst_mode);
    if (out_fd < 0) {
        ret = E_ERR_FAILED;
        LOG_DEBUG("Cannot create destination file, errno = %d", errno);
        goto out;
    }

    if (fstat(in_fd, &sb) == -1) {
        ret = E_ERR_FAILED;
        LOG_ERROR("Failed obtaining file status");
        goto out;
    }

    if (sendfile(out_fd, in_fd, &offset, sb.st_size) == -1) {
        ret = E_ERR_FAILED;
        LOG_ERROR("Copying file failed, errno = %d", errno);
    }

out:
    if (in_fd >= 0)
        close(in_fd);
    if (out_fd >= 0) {
        if (close(out_fd) < 0) {
            LOG_ERROR("Error while closing %s: %d", dst, errno);
            ret = E_ERR_FAILED;
        }
    }
    umask(old_umask & 0777);
    return ret;
}

/**
 * Private function.
 * This function looks for all files matching a pattern in all subfolders.
 * NB: Caller shall deallocate all pointer listed in files
 *
 * @param [in] folder
 * @param [in] pattern
 * @param [out] files list of files found files
 * @param [out] found number of files found
 * @param [in] max size of files
 *
 * @return none
 */
static void find(const char *folder, const char *pattern, char **files,
                 int *found, int max)
{
    DIR *dir = NULL;
    struct dirent *entry = NULL;

    ASSERT(folder != NULL);
    ASSERT(pattern != NULL);
    ASSERT(files != NULL);
    ASSERT(found != NULL);

    dir = opendir(folder);
    if (!dir) {
        LOG_ERROR("wrong path: %s", folder);
    } else {
        while ((entry = readdir(dir)) && *found < max) {
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
                continue;
            } else if (entry->d_type == DT_DIR) {
                char subfolder[PATH_MAX] = "";
                snprintf(subfolder, sizeof(subfolder) - 1, "%s/%s", folder,
                         entry->d_name);
                /* recursive call: will stop when the last subfolder is reached
                **/
                find(subfolder, pattern, files, found, max);
            } else if (strstr(entry->d_name, pattern)) {
                int size = strlen(folder) + strlen(entry->d_name) + 2;
                files[*found] = malloc(sizeof(char) * size);
                ASSERT(files[*found] != NULL);
                snprintf(files[*found], size, "%s/%s", folder, entry->d_name);
                (*found)++;
            }
        }
        closedir(dir);
    }
}

/**
 * This function looks for all files matching the pattern in all subfolders.
 * NB: Caller shall deallocate all pointers listed in files
 *
 * @param [in] folder
 * @param [in] pattern
 * @param [out] files list of files found files
 * @param [in] max size of files
 *
 * @return the number of files found
 */
int file_find(const char *folder, const char *pattern, char **files, int max)
{
    int found = 0;

    find(folder, pattern, files, &found, max);
    return found;
}
