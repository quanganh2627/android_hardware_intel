// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cgpt.h"

#include <getopt.h>
#include <string.h>

#include "cgpt_params.h"

static void Usage(void);

int cmd_create(int argc, char *argv[]);
