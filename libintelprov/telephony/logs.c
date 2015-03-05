/*
 * Copyright 2011-2014 Intel Corporation
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
#include <stdarg.h>
#include "logs.h"

void miu_progress_cb(int progress, int total)
{
	printf("Progress: %d / %d\n", progress, total);
}

void miu_log_cb(const char *msg, ...)
{
	va_list ap;

	if (msg != NULL) {
		va_start(ap, msg);
		vprintf(msg, ap);
		printf("\n");
		va_end(ap);
	}
}
