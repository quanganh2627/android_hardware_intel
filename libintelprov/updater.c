/*
 * Copyright 2011-2013 Intel Corporation
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <edify/expr.h>
#include <updater/updater.h>
#include <common.h>
#include <cutils/properties.h>
#include <sys/mman.h>

#include "update_osip.h"
#include "util.h"
#include "fw_version_check.h"
#include "flash_ifwi.h"
#include "fpt.h"
#include "txemanuf.h"
#ifdef TEE_FRAMEWORK
#include "tee_connector.h"
#endif
#include "oem_partition.h"
#include "gpt/partlink/partlink.h"
#include "flash_image.h"

#ifdef BOARD_HAVE_MODEM
#include "telephony/updater.h"
#endif

Value *ExtractImageFn(const char *name, State * state, int argc, Expr * argv[])
{
	Value *ret = NULL;
	char *filename = NULL;
	char *source = NULL;
	void *data = NULL;
	int size;

	if (ReadArgs(state, argv, 2, &filename, &source) < 0) {
		return NULL;
	}

	if (filename == NULL || strlen(filename) == 0) {
		ErrorAbort(state, "filename argument to %s can't be empty", name);
		goto done;
	}

	if (source == NULL || strlen(source) == 0) {
		ErrorAbort(state, "source argument to %s can't be empty", name);
		goto done;
	}

	if ((size = read_image(source, &data)) < 0) {
		ErrorAbort(state, "Couldn't read image %s", source);
		goto done;
	}

	if (file_write(filename, data, size) < 0) {
		ErrorAbort(state, "Couldn't write %s data to %s", source, filename);
		goto done;
	}

	ret = StringValue(strdup(""));
done:
	if (source)
		free(source);
	if (filename)
		free(filename);
	if (data)
		free(data);

	return ret;
}

Value *ExecuteOsipFunction(const char *name, State *state, int argc, Expr *argv[], int (*action)(char*)) {
    Value *ret = NULL;
    char *destination = NULL;

    if (ReadArgs(state, argv, 1, &destination) < 0) {
        return NULL;
    }

    if (destination == NULL || strlen(destination) == 0) {
        ErrorAbort(state, "destination argument to %s can't be empty", name);
        goto done;
    }

    if (action(destination) == -1) {
        ErrorAbort(state, "Error writing %s to OSIP", destination);
        goto done;
    }

    ret = StringValue(strdup(""));

done:
    if (destination)
        free(destination);

    return ret;
}

Value *InvalidateOsFn(const char *name, State *state, int argc, Expr *argv[]) {
    return ExecuteOsipFunction(name, state, argc, argv, invalidate_osii);
}

Value *RestoreOsFn(const char *name, State *state, int argc, Expr *argv[]) {
    return ExecuteOsipFunction(name, state, argc, argv, restore_osii);
}

#define IFWI_BIN_PATH "/tmp/ifwi.bin"
#define IFWI_NAME     "ifwi"
#define FILEMODE      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH

#ifdef MRFLD

enum flash_option_type {
    FLASH_IFWI_BINARY,
    FLASH_BOM_TOKEN_BINARY,
};


#ifdef TEE_FRAMEWORK
#define BOM_TOKEN_NAME "bom-token"
#endif

Value *FlashIfwiOrBomFn(enum flash_option_type flash_option, const char *name, State *state, int argc, Expr *argv[]) {
    Value *ret = NULL;
    char *filename = NULL;
    int err, i, num, buffsize;
    char ifwi_name[128];
    ZipArchive ifwi_za;
    const ZipEntry *ifwi_entry;
    unsigned char *buffer;
#ifdef TEE_FRAMEWORK
    char bom_token_name[128];
    const ZipEntry *bom_token_entry;
    int bom_token_buffsize;
    unsigned char *bom_token_buffer;
#endif

    if (ReadArgs(state, argv, 1, &filename) < 0) {
        return NULL;
    }

    if (filename == NULL || strlen(filename) == 0) {
        ErrorAbort(state, "filename argument to %s can't be empty", name);
        goto done;
    }

    err = mzOpenZipArchive(filename, &ifwi_za);
    if (err) {
        ErrorAbort(state, "Failed to open zip archive %s\n", filename);
        goto done;
    }

    num = mzZipEntryCount(&ifwi_za);
    for (i = 0; i < num; i++) {
        ifwi_entry = mzGetZipEntryAt(&ifwi_za, i);
        if ((ifwi_entry->fileNameLen + 1) < sizeof(ifwi_name)){
            strncpy(ifwi_name, ifwi_entry->fileName, ifwi_entry->fileNameLen);
            ifwi_name[ifwi_entry->fileNameLen] = '\0';
        } else {
            ErrorAbort(state, "ifwi file name is too big. Size max is:%d.\n", sizeof(ifwi_name));
            goto error;
        }
        if (strncmp(ifwi_name, IFWI_NAME, strlen(IFWI_NAME)))
            continue;
        buffsize = mzGetZipEntryUncompLen(ifwi_entry);
        if (buffsize <= 0) {
            ErrorAbort(state, "Bad ifwi_entry size : %d.\n", buffsize);
            goto error;
        }
        buffer = (unsigned char*)malloc(sizeof(unsigned char)*buffsize);
        if (buffer == NULL) {
            ErrorAbort(state, "Unable to alloc ifwi buffer of %d bytes.\n", buffsize);
            goto error;
        }
        err = mzExtractZipEntryToBuffer(&ifwi_za, ifwi_entry, buffer);
        if (!err) {
            ErrorAbort(state, "Failed to unzip %s\n", IFWI_BIN_PATH);
            free(buffer);
            goto error;
        }

        if (check_ifwi_file(buffer, buffsize) < 1) {
            free(buffer);
            continue;
        }

        if (flash_option == FLASH_BOM_TOKEN_BINARY) {
#ifdef TEE_FRAMEWORK
            strcpy(bom_token_name, BOM_TOKEN_NAME);
            strncat(bom_token_name, &(ifwi_name[strlen(IFWI_NAME)]), sizeof(bom_token_name) - strlen(BOM_TOKEN_NAME) - 1);
            bom_token_entry = mzFindZipEntry(&ifwi_za, bom_token_name);

            if (bom_token_entry != NULL) {
                bom_token_buffsize = mzGetZipEntryUncompLen(bom_token_entry);
                if (bom_token_buffsize <= 0) {
                    ErrorAbort(state, "Bad bom_token_entry size : %d.\n", bom_token_buffsize);
                    free(buffer);
                    goto error;
                }
                bom_token_buffer = (unsigned char*)malloc(sizeof(unsigned char)*bom_token_buffsize);
                if (bom_token_buffer == NULL) {
                    ErrorAbort(state, "Unable to alloc bom token buffer of %d bytes.\n", bom_token_buffsize);
                    free(buffer);
                    goto error;
                }
                err = mzExtractZipEntryToBuffer(&ifwi_za, bom_token_entry, bom_token_buffer);
                if (!err) {
                    ErrorAbort(state, "Failed to unzip %s.\n", IFWI_BIN_PATH);
                    free(bom_token_buffer);
                    free(buffer);
                    goto error;
                }
                if (write_token(bom_token_buffer, bom_token_buffsize) == 0) {
                    printf("BOM token written\n");
                } else {
                    printf("Unable to write BOM token.\n");
                    cancel_update(0, NULL);
                    free(bom_token_buffer);
                    free(buffer);
                    ret = StringValue(strdup("fail"));
                    goto error;
                }
                free(bom_token_buffer);
            }
#else
            printf("BOM token flashing not supported\n");
#endif
        } else if (flash_option == FLASH_IFWI_BINARY) {
            printf("Flashing IFWI\n");
            update_ifwi_file(buffer, buffsize);
        } else {
            ErrorAbort(state, "Don't know what to do with option %d\n", flash_option);
            free(buffer);
            goto error;
        }
        free(buffer);
    }

    ret = StringValue(strdup(""));

error:
    mzCloseZipArchive(&ifwi_za);

done:
    if (filename)
        free(filename);

    return ret;
}

Value *FlashIfwiFn(const char *name, State *state, int argc, Expr *argv[]) {
    return FlashIfwiOrBomFn(FLASH_IFWI_BINARY, name, state, argc, argv);
}

#ifdef TEE_FRAMEWORK
Value *FlashBomFn(const char *name, State *state, int argc, Expr *argv[]) {
    return FlashIfwiOrBomFn(FLASH_BOM_TOKEN_BINARY, name, state, argc, argv);
}
#endif

#else /* MRFLD */

#define DNX_BIN_PATH "/tmp/dnx.bin"
#define DNX_NAME     "dnx"

Value *FlashIfwiFn(const char *name, State *state, int argc, Expr *argv[]) {
    Value *ret = NULL;
    char *filename = NULL;
    int err, ifwi_bin_fd, dnx_bin_fd, i, num;
    char ifwi_name[128], dnx_name[128];
    ZipArchive ifwi_za;
    const ZipEntry *dnx_entry, *ifwi_entry;

    if (ReadArgs(state, argv, 1, &filename) < 0) {
        return NULL;
    }

    if (filename == NULL || strlen(filename) == 0) {
        ErrorAbort(state, "filename argument to %s can't be empty", name);
        goto done;
    }

    err = mzOpenZipArchive(filename, &ifwi_za);
    if (err) {
        ErrorAbort(state, "Failed to open zip archive %s\n", filename);
        goto done;
    }

    num = mzZipEntryCount(&ifwi_za);
    for (i = 0; i < num; i++) {
        ifwi_entry = mzGetZipEntryAt(&ifwi_za, i);
        if ((ifwi_entry->fileNameLen + 1) < sizeof(ifwi_name)){
            strncpy(ifwi_name, ifwi_entry->fileName, ifwi_entry->fileNameLen);
            ifwi_name[ifwi_entry->fileNameLen] = '\0';
        } else {
            ErrorAbort(state, "ifwi file name is too big. Size max is:%d.\n", sizeof(ifwi_name));
            goto error;
        }
        if (strncmp(ifwi_name, IFWI_NAME, strlen(IFWI_NAME)))
            continue;

        if ((ifwi_bin_fd = open(IFWI_BIN_PATH, O_RDWR | O_TRUNC | O_CREAT, FILEMODE)) < 0) {
            ErrorAbort(state, "unable to create Extracted file:%s.\n", IFWI_BIN_PATH);
            goto error;
        }
        if ((dnx_bin_fd = open(DNX_BIN_PATH, O_RDWR | O_TRUNC | O_CREAT, FILEMODE)) < 0) {
            ErrorAbort(state, "unable to create Extracted file:%s.\n", IFWI_BIN_PATH);
            close(ifwi_bin_fd);
            goto error;
        }
        strcpy(dnx_name, "dnx_fwr");
        strncat(dnx_name, &(ifwi_name[strlen(IFWI_NAME)]), sizeof(dnx_name) - strlen("dnx_fwr") -1);
        dnx_entry = mzFindZipEntry(&ifwi_za, dnx_name);

        if (dnx_entry == NULL) {
            ErrorAbort(state, "Could not find DNX entry");
            close(ifwi_bin_fd);
            close(dnx_bin_fd);
            goto error;
        }

        err = mzExtractZipEntryToFile(&ifwi_za, dnx_entry, dnx_bin_fd);
        if (!err) {
            ErrorAbort(state, "Failed to unzip %s\n", DNX_BIN_PATH);
            close(ifwi_bin_fd);
            close(dnx_bin_fd);
            goto error;
        }
        close(dnx_bin_fd);
        err = mzExtractZipEntryToFile(&ifwi_za, ifwi_entry, ifwi_bin_fd);
        if (!err) {
            ErrorAbort(state, "Failed to unzip %s\n", DNX_BIN_PATH);
            close(ifwi_bin_fd);
            goto error;
        }
        close(ifwi_bin_fd);
        update_ifwi_file(DNX_BIN_PATH, IFWI_BIN_PATH);
    }

    ret = StringValue(strdup(""));

error:
    mzCloseZipArchive(&ifwi_za);

done:
    if (filename)
        free(filename);

    return ret;
}
#endif /* MRFLD */

Value *FlashCapsuleFn(const char *name, State *state, int argc, Expr *argv[]) {
    Value *ret = NULL;
    char *filename = NULL;
    void *data = NULL;
    unsigned size;


    if (ReadArgs(state, argv, 1, &filename) < 0) {
        ErrorAbort(state, "ReadArgs() failed");
        goto done;
    }

    if (filename == NULL || strlen(filename) == 0) {
        ErrorAbort(state, "filename argument to %s can't be empty", name);
        goto done;
    }

    if (file_read(filename, &data, &size)) {
        ErrorAbort(state, "file_read %s failed", filename);
        goto done;
    }

    if (flash_capsule(data, size) != 0) {
        ErrorAbort(state, "flash_capsule failed");
        goto done;
    }

    /* no error */
    ret = StringValue(strdup(""));
done:
    if (filename)
        free(filename);
    if (data)
        free(data);

    return ret;
}

Value *FlashUlpmcFn(const char *name, State *state, int argc, Expr *argv[]) {
    Value *ret = NULL;
    char *filename = NULL;
    void *data = NULL;
    unsigned size;

    if (ReadArgs(state, argv, 1, &filename) < 0) {
        ErrorAbort(state, "ReadArgs() failed");
        goto done;
    }


    if (filename == NULL || strlen(filename) == 0) {
        ErrorAbort(state, "filename argument to %s can't be empty", name);
        goto done;
    }

    if (file_read(filename, &data, &size)) {
        ErrorAbort(state, "file_read failed %s failed", filename);
        goto done;
    }

    if (flash_ulpmc(data, size) != 0) {
        ErrorAbort(state, "flash_ulpmc failed");
        goto done;
    }

    /* no error */
    ret = StringValue(strdup(""));
done:
    if (filename)
        free(filename);
    if (data)
        free(data);

    return ret;
}

static void recovery_error(const char *msg)
{
    fprintf(stderr, "%s", msg);
}

Value *FlashCallFunction(int (*fun)(char *), const char *name, State *state,
                         int argc, Expr *argv[])
{
    Value *ret = NULL;
    char *filename = NULL;

    if (ReadArgs(state, argv, 1, &filename) < 0)
        goto done;

    if (fun(filename) != EXIT_SUCCESS) {
        ErrorAbort(state, "%s failed.", name);
        goto done;
    }

    ret = StringValue(strdup(""));

done:
    if (filename)
        free(filename);
    return ret;
}

Value *FlashFptIfwi(const char *name, State *state, int argc, Expr *argv[]) {
    return FlashCallFunction(flash_fpt_file_ifwi, name, state, argc, argv);
}

Value *FlashFptTxe(const char *name, State *state, int argc, Expr *argv[]) {
    return FlashCallFunction(flash_fpt_file_txe, name, state, argc, argv);
}

Value *FlashFptPdr(const char *name, State *state, int argc, Expr *argv[]) {
    return FlashCallFunction(flash_fpt_file_pdr, name, state, argc, argv);
}

Value *FlashFptBios(const char *name, State *state, int argc, Expr *argv[]) {
    return FlashCallFunction(flash_fpt_file_bios, name, state, argc, argv);
}

Value *FlashFptFpfs(const char *name, State *state, int argc, Expr *argv[]) {
    return FlashCallFunction(flash_fpt_file_fpfs, name, state, argc, argv);
}

Value *FlashTxemanuf(const char *name, State *state, int argc, Expr *argv[]) {
    return FlashCallFunction(flash_txemanuf_file, name, state, argc, argv);
}

Value *CommandFunction(int (*fun)(int, char **), const char *name, State *state,
                       int argc, Expr *argv[]) {
    Value *ret = NULL;
    char *argv_str[argc + 1];
    int i;

    char **argv_read = ReadVarArgs(state, argc, argv);
    if (argv_read == NULL) {
        ErrorAbort(state, "%s parameter parsing failed.", name);
        goto done;
    }

    argv_str[0] = (char *)name;
    for (i = 0 ; i < argc ; i++)
        argv_str[i + 1] = argv_read[i];

    if (fun(sizeof(argv_str) / sizeof(char *), argv_str) != EXIT_SUCCESS) {
        ErrorAbort(state, "%s failed.", name);
        goto done;
    }

    for (i = 0 ; i < argc ; i++)
        free(argv_read[i]);
    free(argv_read);

    ret = StringValue(strdup(""));

done:
    return ret;
}

Value *FptWriteItem(const char *name, State *state, int argc, Expr *argv[]) {
    return CommandFunction(fpt_writeitem, name, state, argc, argv);
}

Value *FptWriteValidBit(const char *name, State *state, int argc, Expr *argv[]) {
    return CommandFunction(fpt_writevalidbit, name, state, argc, argv);
}

Value *FptCloseMnf(const char *name, State *state, int argc, Expr *argv[]) {
    return CommandFunction(fpt_closemnf, name, state, argc, argv);
}

Value *TxemanufEofTest(const char *name, State *state, int argc, Expr *argv[]) {
    return CommandFunction(txemanuf_eof_test, name, state, argc, argv);
}

Value *TxemanufBistTest(const char *name, State *state, int argc, Expr *argv[]) {
    return CommandFunction(txemanuf_bist_test, name, state, argc, argv);
}

Value *FlashOSImage(const char *name, State *state, int argc, Expr *argv[]) {
    Value *funret = NULL;
    char *filename, *image_name;
    void *data;
    int ret;

    if (argc != 2) {
        ErrorAbort(state, "%s: Invalid parameters.", name);
        goto exit;
    }

    if (ReadArgs(state, argv, 2, &filename, &image_name) < 0) {
        ErrorAbort(state, "%s: ReadArgs failed.", name);
        goto exit;
    }

    int length = file_size(filename);
    if (length == -1)
        goto free;

    data = file_mmap(filename, length, true);
    if (data == MAP_FAILED)
        goto free;

    ret = flash_image(data, length, image_name);
    if (ret != 0) {
        ErrorAbort(state, "%s: Failed to flash %s image %s, %s.",
                   name, filename, image_name, strerror(errno));
        goto unmap;
    }

    funret = StringValue(strdup(""));

unmap:
    munmap(data, length);
free:
    free(filename);
    free(image_name);
exit:
    return funret;
}

Value *FlashImageAtOffset(const char *name, State *state, int argc, Expr *argv[]) {
    Value *funret = NULL;
    char *filename, *offset_str;
    void *data;
    off_t offset;
    int fd;
    int ret;

    if (argc != 2) {
        ErrorAbort(state, "%s: Invalid parameters.", name);
        goto exit;
    }

    if (ReadArgs(state, argv, 2, &filename, &offset_str) < 0) {
        ErrorAbort(state, "%s: ReadArgs failed.", name);
        goto exit;
    }

    char *end;
    offset = strtoul(offset_str, &end, 10);
    if (*end != '\0' || errno == ERANGE) {
        ErrorAbort(state, "%s: offset argument parsing failed.", name);
        goto free;
    }

    int length = file_size(filename);
    if (length == -1)
        goto free;

    data = file_mmap(filename, length, false);
    if (data == MAP_FAILED) {
        goto free;
    }

    fd = open(MMC_DEV_POS, O_WRONLY);
    if (fd == -1) {
        ErrorAbort(state, "%s: Failed to open %s device block, %s.",
                   name, MMC_DEV_POS, strerror(errno));
        goto unmmap_file;
    }

    ret = lseek(fd, offset, SEEK_SET);
    if (ret == -1) {
        ErrorAbort(state, "%s: Failed to seek into %s device block, %s.",
                   name, MMC_DEV_POS, strerror(errno));
        goto close;
    }

    char *ptr = (char *)data;
    ret = 0;
    for (; length ; length -= ret, ptr += ret) {
        ret = write(fd, ptr, length);
        if (ret == -1) {
            ErrorAbort(state, "%s: Failed to write into %s device block, %s.",
                       name, MMC_DEV_POS, strerror(errno));
            goto close;
        }
    }

    ret = fsync(fd);
    if (ret == -1) {
        ErrorAbort(state, "%s: Failed to sync %s, %s.",
                   name, MMC_DEV_POS, strerror(errno));
        goto close;
    }

    funret = StringValue(strdup(""));

close:
    close(fd);
unmmap_file:
    munmap(data, length);
free:
    free(filename);
    free(offset_str);
exit:
    return funret;
}

/* Warning: USE THIS FUNCTION VERY CAUTIOUSLY. It only has been added
 * for an OTA update which REALLY needs to modify the partition
 * scheme. Before Android KitKat, we have some running services that
 * needs some partition to be mounted and thus we cannot reload the
 * partition scheme and the /dev/block/ nodes won't be accurately
 * representation of the actual partition scheme after this function
 * is called. Think about this before calling this function.  */
Value *FlashPartition(const char *name, State *state, int argc, Expr *argv[]) {
    Value *ret = NULL;

    /* Ensure that the partition links are there before smashing up
     * the partition scheme.  */
    partlink_populate();

    /* Do not reload partition table during OTA since some partition
     * are still mounted, reload would failed.  */
    oem_partition_disable_cmd_reload();

    property_set("sys.partitioning", "1");
    ret = CommandFunction(oem_partition_cmd_handler, name, state, argc, argv);
    property_set("sys.partitioning", "0");

    return ret;
}

void Register_libintel_updater(void)
{
    RegisterFunction("flash_ifwi", FlashIfwiFn);
#ifdef TEE_FRAMEWORK
    RegisterFunction("flash_bom_token", FlashBomFn);
#endif  /* TEE_FRAMEWORK */

#ifdef BOARD_HAVE_MODEM
    RegisterTelephonyFunctions();
#endif
    RegisterFunction("extract_image", ExtractImageFn);
    RegisterFunction("invalidate_os", InvalidateOsFn);
    RegisterFunction("restore_os", RestoreOsFn);

    RegisterFunction("flash_capsule", FlashCapsuleFn);
    RegisterFunction("flash_ulpmc", FlashUlpmcFn);

    RegisterFunction("flash_fpt_ifwi", FlashFptIfwi);
    RegisterFunction("flash_fpt_txe", FlashFptTxe);
    RegisterFunction("flash_fpt_pdr", FlashFptPdr);
    RegisterFunction("flash_fpt_bios", FlashFptBios);
    RegisterFunction("flash_fpt_fpfs", FlashFptFpfs);
    RegisterFunction("flash_txemanuf", FlashTxemanuf);

    RegisterFunction("fpt_writeite", FptWriteItem);
    RegisterFunction("fpt_writevalidbit", FptWriteValidBit);
    RegisterFunction("fpt_closemnf", FptCloseMnf);
    RegisterFunction("txemanuf_eof_test", TxemanufEofTest);
    RegisterFunction("txemanuf_bist_test", TxemanufBistTest);
    RegisterFunction("flash_partition", FlashPartition);
    RegisterFunction("flash_image_at_offset", FlashImageAtOffset);
    RegisterFunction("flash_os_image", FlashOSImage);

    util_init(recovery_error, NULL);
}
