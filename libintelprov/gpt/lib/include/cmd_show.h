// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cgpt.h"

#define __STDC_FORMAT_MACROS
#include <getopt.h>
#include <inttypes.h>
#include <string.h>
#include "cgpt_params.h"
#include "cmd_create.h"

static void Usage(void);
int cmd_show(int argc, char *argv[]);
