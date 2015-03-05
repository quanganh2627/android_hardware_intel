/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _PARTLINK_H_
#define _PARTLINK_H_

#define BASE_PLATFORM "/dev/block/platform"
#define BASE_PLATFORM_INTEL BASE_PLATFORM "/intel"
#define BASE_PLATFORM_INTEL_UUID BASE_PLATFORM_INTEL "/by-guid"
#define BASE_PLATFORM_INTEL_LABEL BASE_PLATFORM_INTEL "/by-label"

int partlink_populate();

#endif	/* _PARTLINK_H_ */
