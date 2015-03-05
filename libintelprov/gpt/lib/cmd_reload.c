// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cgpt.h"

#include <getopt.h>
#include <string.h>
#include <linux/fs.h>
#include "cgpt_params.h"

#ifndef STORAGE_BASE_PATH
#define STORAGE_BASE_PATH "/dev/block/mmcblk0"
#endif

static void Usage(void)
{
  printf("\nUsage: %s reload DRIVE\n\n"
         "\n", progname);
}

int cmd_reload(int argc, char *argv[]) {
  char *drive = { STORAGE_BASE_PATH } ;
  int fd;
  int c;
  int errorcnt = 0;

  opterr = 0;                     // quiet, you
  while ((c=getopt(argc, argv, ":hv")) != -1)
  {
    switch (c)
    {
    case 'h':
      Usage();
      return CGPT_OK;
    case '?':
      Error("unrecognized option: -%c\n", optopt);
      errorcnt++;
      break;
    case ':':
      Error("missing argument to -%c\n", optopt);
      errorcnt++;
      break;
    default:
      errorcnt++;
      break;
    }
  }
  if (errorcnt)
  {
    Usage();
    return CGPT_FAILED;
  }

  fprintf(stderr, "reload: drive is %s and errorcnt %d\n", drive, errorcnt);


  int i;
  for (i=0; i<argc; i++)
      printf("argv[%d]=%s\n", i, argv[i]);
  if ( (fd=open(drive, O_RDWR | O_LARGEFILE | O_NOFOLLOW)) == -1 ) {
      errorcnt = 1;
      goto error;
  }

  errorcnt |= ioctl(fd, BLKRRPART, 0);

  if (errorcnt) {
    fprintf(stderr, "could not re-read partition table on %s\n", drive);
  }

  close(fd);

error:
  return errorcnt;
}
