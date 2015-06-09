/* Modem Manager - modem folder source file
**
** ** Copyright (C) Intel 2014
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

#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "errors.h"
#include "logs.h"
#include "folder.h"

#define TELEPHONY_USER "radio"
#define SYSTEM_USER "system"
#define FOLDER_PERMISSION (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | \
                           S_IXGRP)

int folder_create(const char *path)
{
    int ret = -1;
    mode_t old_umask = umask(~FOLDER_PERMISSION & 0777);

    ASSERT(path != NULL);

    errno = 0;
    if (mkdir(path, FOLDER_PERMISSION)) {
        if (EEXIST == errno)
            ret = 0;
        else
            LOG_ERROR("Failed to create %s: %s", path, strerror(errno));
    } else {
        struct passwd *pwd = getpwnam(SYSTEM_USER);
        struct group *gp = getgrnam(TELEPHONY_USER);
        if (pwd && gp) {
            if (!chown(path, pwd->pw_uid, gp->gr_gid) &&
                !chmod(path, FOLDER_PERMISSION))
                ret = 0;
        }
    }

    umask(old_umask & 0777);
    return ret;
}

int folder_remove(const char *path)
{
    int ret = 0;
    DIR *dir;
    struct dirent *entry;

    ASSERT(path != NULL);

    errno = 0;
    dir = opendir(path);
    if (!dir) {
        if (ENOENT != errno)
            ret = -1;
        goto out;
    }

    while ((entry = readdir(dir)) != NULL) {
        char file[PATH_MAX];
        snprintf(file, sizeof(file), "%s/%s", path, entry->d_name);

        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        if (folder_exist(file) && !(ret = folder_remove(file))) {
            break;
        } else {
            errno = 0;
            if (unlink(file)) {
                if (ENOENT != errno) {
                    ret = -1;
                    break;
                }
            }
        }
    }

    closedir(dir);
    if (!ret)
        ret = rmdir(path);

out:
    return ret;
}

/**
 * Look for folder existence
 *
 * @param [in] path folder path
 *
 * @return false by default
 * @return true if file exist
 */
bool folder_exist(const char *path)
{
    struct stat st;

    ASSERT(path != NULL);

    return !stat(path, &st) && S_ISDIR(st.st_mode);
}
