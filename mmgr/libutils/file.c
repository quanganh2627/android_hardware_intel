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
#include <regex.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include "errors.h"
#include "file.h"
#include "logs.h"

#define MASK_ALL (S_IRWXU | S_IRWXG | S_IRWXO)
#define SIZE_CHUNK 20
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

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
e_mmgr_errors_t file_write(const char *path, unsigned long mode, char *value,
                           size_t size)
{
    int fd;
    e_mmgr_errors_t ret = E_ERR_FAILED;

    ASSERT(path != NULL);
    ASSERT(value != NULL);

    fd = open(path, O_WRONLY | O_CREAT, mode);
    if (fd < 0) {
        LOG_ERROR("open of (%s) failed (%s)", path, strerror(errno));
    } else {
        ssize_t write_size = 0;
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
 * Reads a string to a file
 *
 * @param [in] path complete file path
 * @param [out] value string to read
 * @param [in] size string size
 *
 * @return E_ERR_FAILED if open fails
 * @return E_ERR_SUCCESS if successful
 */
e_mmgr_errors_t file_read(const char *path, char *value, size_t size)
{
    int fd;
    e_mmgr_errors_t ret = E_ERR_FAILED;

    ASSERT(path != NULL);
    ASSERT(value != NULL);

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        LOG_ERROR("open of (%s) failed (%s)", path, strerror(errno));
    } else {
        ssize_t read_size = 0;
        if (size > 0)
            read_size = read(fd, value, size);
        if ((close(fd) == 0) && ((size_t)read_size > 0)) {
            ret = E_ERR_SUCCESS;
            LOG_DEBUG("read from (%s) succeed", path);
            value[MIN(read_size, (ssize_t)size - 1)] = '\0';
        } else {
            LOG_ERROR("read to (%s) failed. (%s)", path, strerror(errno));
        }
    }

    return ret;
}

/**
 * Look for file
 *
 * @param [in] path file path
 *
 * @return false by default
 * @return true if file exist
 */
bool file_exist(const char *path)
{
    struct stat st;

    ASSERT(path != NULL);

    return !stat(path, &st) && S_ISREG(st.st_mode);
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
    int in_fd;
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
        LOG_DEBUG("Cannot open source file (%s), errno = %d", src, errno);
        goto out;
    }

    out_fd = open(dst, O_CREAT | O_WRONLY | O_TRUNC, dst_mode);
    if (out_fd < 0) {
        ret = E_ERR_FAILED;
        LOG_DEBUG("Cannot create destination file: (%s), errno = %d", dst,
                  errno);
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
 * @param [in] reg regcomp context
 * @param [in] files array of files. Must be NULL at first call
 * @param [out] found number of files found
 *
 * @return an array of files
 * @return NULL if no file found
 */
static char **file_find_op(const char *folder, regex_t *reg, char **files,
                           size_t *found)
{
    DIR *dir = NULL;

    ASSERT(folder != NULL);
    ASSERT(reg != NULL);
    ASSERT(found != NULL);
    /* files can be NULL */

    dir = opendir(folder);
    if (!dir) {
        LOG_DEBUG("Path (%s) does not exist", folder);
    } else {
        struct dirent *entry = NULL;
        while ((entry = readdir(dir))) {
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
                continue;

            if (entry->d_type == DT_DIR) {
                char subfolder[PATH_MAX];

                snprintf(subfolder, sizeof(subfolder), "%s/%s", folder,
                         entry->d_name);
                /* recursive call: will stop when the last subfolder is reached
                **/
                files = file_find_op(subfolder, reg, files, found);
            } else if (!regexec(reg, entry->d_name, 0, NULL, 0)) {
                if (!(*found % SIZE_CHUNK)) {
                    size_t size = *found + SIZE_CHUNK;
                    files = realloc(files, size * sizeof(char *));
                    ASSERT(files != NULL);
                }

                int len = strlen(folder) + strlen(entry->d_name) + 2;
                files[*found] = malloc(sizeof(char) * len);
                ASSERT(files[*found] != NULL);
                snprintf(files[*found], len, "%s/%s", folder,
                         entry->d_name);
                (*found)++;
            }
        }
        closedir(dir);
    }

    return files;
}

static int file_cmp_str(const void *p1, const void *p2)
{
    return strcmp(*(char *const *)p1, *(char *const *)p2);
}

/**
 * This function looks for all files matching the pattern in all subfolders.
 * NB: Caller shall deallocate all pointers listed in files
 *
 * @param [in] folder
 * @param [in] regexp
 * @param [out] nb number of files found
 *
 * @return an array of files
 * @return NULL if no file found
 */
char **file_find(const char *folder, const char *regexp, size_t *found)
{
    char **files = NULL;
    regex_t reg;

    ASSERT(found != NULL);

    LOG_DEBUG("regexp: %s", regexp);

    *found = 0;
    if (!regcomp(&reg, regexp, REG_ICASE | REG_EXTENDED))
        files = file_find_op(folder, &reg, files, found);

    /* files are sorted to always provide the same list */
    if (files && *found > 0)
        qsort(files, *found, sizeof(char *), file_cmp_str);

    regfree(&reg);

    return files;
}

/**
 * This function looks for all files matching the extension in all subfolders.
 * NB: Caller shall deallocate all pointers listed in files
 *
 * @param [in] folder
 * @param [in] extension file extension
 * @param [out] found number of files found
 *
 * @return an array of files
 * @return NULL if no file found
 */
char **file_find_ext(const char *folder, const char *extension, size_t *found)
{
    char regexp[10];

    snprintf(regexp, sizeof(regexp), ".*\\.%s$", extension);

    return file_find(folder, regexp, found);
}
