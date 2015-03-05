/* Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <cgpt.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#define K_MAX_LINE_LEN 8192
#define K_MAX_ARGS 256
#define K_MAX_ARG_LEN 256


static char **str_to_array(char *str, int *argc)
{
    char *str1, *token;
    char *saveptr1;
    int j;
    int num_tokens;
    char **tokens;

    tokens=malloc(sizeof(char *) * K_MAX_ARGS);

    if(tokens==NULL)
        return NULL;

    num_tokens = 0;

    for (j = 1, str1 = str; ; j++, str1 = NULL) {
        token = strtok_r(str1, " ", &saveptr1);

        if (token == NULL)
            break;

        tokens[num_tokens] = (char *) malloc(sizeof(char) * K_MAX_ARG_LEN+1);

        if(tokens[num_tokens]==NULL)
            break;

        strncpy(tokens[num_tokens], token, K_MAX_ARG_LEN);
        num_tokens++;

        if (num_tokens == K_MAX_ARGS)
            break;
    }
    *argc = num_tokens;
    return tokens;
}

static int cmd_start_partitioning(int argc, char **argv)
{
    return 0;
}

static int cmd_stop_partitioning(int argc, char **argv)
{
    return 0;
}

static int _oem_partition_gpt_sub_command(int argc, char **argv)
{

    static struct {
        const char *name;
        int (*fp)(int argc, char *argv[]);
    } cmds[] = {
        {"create", cmd_create },
        {"add", cmd_add},
        {"show", cmd_show},
        {"repair", cmd_repair},
        {"boot", cmd_bootable},
        {"find", cmd_find},
        {"prioritize", cmd_prioritize},
        {"legacy", cmd_legacy},
        {"start", cmd_start_partitioning},
        {"reload", cmd_reload},
        {"end", cmd_stop_partitioning},
    };


    //printf("%s\n", __func__);
    unsigned int i;
    int match_count = 0;
    int match_index = 0;
    char *command = argv[0];

    optind=0;
    for (i = 0; command && i < sizeof(cmds)/sizeof(cmds[0]); ++i) {
        // exact match?
        if (0 == strcmp(cmds[i].name, command)) {
            match_index = i;
            match_count = 1;
            break;
        }
        // unique match?
        else if (0 == strncmp(cmds[i].name, command, strlen(command))) {
            match_index = i;
            match_count++;
        }
    }

    if (match_count == 1)
        return cmds[match_index].fp(argc, argv);

    return -1;
}

static int oem_partition_gpt_handler(FILE *fp)
{
    //printf("%s\n", __func__);
    char buffer[K_MAX_ARG_LEN];
    char **argv=NULL;
    int argc=0;
    int i;
    int ret=0;

    uuid_generator=uuid_generate;
    dup2(2, 1);

    while (fgets(buffer, sizeof(buffer), fp)) {
        buffer[strlen(buffer)-1]='\0';
        argv=str_to_array(buffer, &argc);
            ret|=_oem_partition_gpt_sub_command(argc, argv);

        for(i = 0;i<argc; i++) {
            if (argv[i]) {
                free(argv[i]);
                argv[i]=NULL;
            }
        }

        if (argv) {
            free(argv);
            argv=NULL;
        }
    }

    return ret;
}

static int oem_partition_mbr_handler(FILE *fp)
{
    return 0;
}


int main(int argc, char **argv)
{
    char buffer[K_MAX_ARG_LEN];
    char partition_type[K_MAX_ARG_LEN];
    FILE *fp;

    memset(buffer, 0, sizeof(buffer));
    int j=1;
    int ret=-1;
    while (j--) {
        if (argc == 2) {

            fp = fopen(argv[1], "r");
            if (!fp)
                return -1;

            if (!fgets(buffer, sizeof(buffer), fp))
                return -1;

            buffer[strlen(buffer)-1]='\0';

            if (sscanf(buffer, "%*[^=]=%255s", partition_type) != 1)
                return -1;

            if (!strncmp("gpt", partition_type, strlen(partition_type)))
                ret=oem_partition_gpt_handler(fp);

            if (!strncmp("mbr", partition_type, strlen(partition_type)))
                ret=oem_partition_mbr_handler(fp);

            fclose(fp);
        }
    }
    return ret;
}

