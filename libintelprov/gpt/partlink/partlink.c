/* Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>

#include "cgpt.h"
#include "cmd_show.h"
#include "cgpt_params.h"
#include "partlink.h"

#ifndef STORAGE_BASE_PATH
#define STORAGE_BASE_PATH "/dev/block/mmcblk0"
#endif

#ifndef STORAGE_PARTITION_FORMAT
#define STORAGE_PARTITION_FORMAT "%sp%d"
#endif

static int clean_directory(const char *path)
{
    struct dirent *cur;
    char filename[strlen(path) + sizeof(cur->d_name)];
    char *filename_ptr;

    DIR *dir = opendir(path);
    if (dir == NULL)
        return EXIT_FAILURE;

    strcpy(filename, path);
    filename_ptr = filename + strlen(filename);

    while ((cur = readdir(dir)) != NULL)
        if (strcmp(cur->d_name, ".") != 0 && strcmp(cur->d_name, "..") != 0) {
            strcpy(filename_ptr, cur->d_name);
            if (unlink(filename) == -1)
                return EXIT_FAILURE;
        }

    closedir(dir);

    return EXIT_SUCCESS;
}

int partlink_populate()
{
    CgptShowParams params;
    GptEntry *entry;

    struct drive drive;
    int gpt_retval;
    int ret = EXIT_FAILURE;
    uint32_t i;
    uint8_t label[GPT_PARTNAME_LEN];
    char  uuid[GUID_STRLEN];

    char from[BUFSIZ];
    char to[BUFSIZ];

    memset(&params, 0, sizeof(params));
    params.drive_name = STORAGE_BASE_PATH;

    mkdir(BASE_PLATFORM, 0600);
    mkdir(BASE_PLATFORM_INTEL,0600);
    mkdir(BASE_PLATFORM_INTEL_UUID, 0600);
    mkdir(BASE_PLATFORM_INTEL_LABEL, 0600);

    if (clean_directory(BASE_PLATFORM_INTEL_UUID"/") != EXIT_SUCCESS ||
        clean_directory(BASE_PLATFORM_INTEL_LABEL"/") != EXIT_SUCCESS)
        goto exit;

    if (CGPT_OK != DriveOpen(params.drive_name, &drive, O_RDONLY))
        goto exit;

    if (GPT_SUCCESS != (gpt_retval = GptSanityCheck(&drive.gpt))) {
        goto close;
    }

    for (i = 0; i < GetNumberOfEntries(&drive.gpt); ++i) {
        entry = GetEntry(&drive.gpt, ANY_VALID, i);

        if (IsZero(&entry->type))
            continue;

        UTF16ToUTF8(entry->name, sizeof(entry->name) / sizeof(entry->name[0]),
                    label, sizeof(label));

        GuidToStr(&entry->unique, uuid, GUID_STRLEN);

        snprintf(from, sizeof(from) - 1, STORAGE_PARTITION_FORMAT, STORAGE_BASE_PATH, i + 1);

        snprintf(to, sizeof(to) - 1, BASE_PLATFORM_INTEL_LABEL "/%s" , label);
        symlink(from, to);

        snprintf(to, sizeof(to) - 1, BASE_PLATFORM_INTEL_UUID "/%s" , uuid);
        symlink(from, to);
    }

    ret = EXIT_SUCCESS;

close:
    DriveClose(&drive, 0);

exit:
    return ret;
}
