/**
 * Copyright 2009 - 2010 (c) Intel Corporation. All rights reserved.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* includes */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include "watchdogd.h"
#include <linux/watchdog.h>
#include <sys/mman.h>

#define __NR_mlockall       152
/* High level configuration */
#define DLTIMER_DEFAULT_TIMEOUT 75		/* dead line timer frequency */
#define WATCHDOG_TIME_BETWEEN_WARN_AND_RESET 15	/* time to dump kernel status before resetting */
#define WATCHDOG_KICKING_MARGIN 15		/* time watchdog is kicked before timeout expires */
#define WATCHDOG_DEVICE "/dev/watchdog"

/* This kicks the watchdog device by writing the device */
static int kick_watchdog(int fd)
{
	char buf = 'R';
	return write(fd, &buf, 1);
}

/* timeout: maximum interval between 2 timer interrupts */
int main(int argc, char **argv)
{
	int wd_fd, opt;
	int ret;
	int pre_timeout = DLTIMER_DEFAULT_TIMEOUT;
	int timeout;
	int options;

	/* Start logging here */
	LOG_INIT(LOG_TAG);
	LOGW("Watchdog daemon starting\n");

	/* Look through the options */
	while ((opt = getopt(argc, argv, "t:")) != -1) {
		switch (opt) {
			case 't':
				pre_timeout = atoi(optarg);
				break;
			default:
				LOGE("invalid parameters\n");
				exit (-1);
		}
	}

	timeout = pre_timeout + WATCHDOG_TIME_BETWEEN_WARN_AND_RESET;

	/* Real job from here */
	LOGW("Watchdog Timer set to timer=%d margin=%d\n",
	     pre_timeout,
	     WATCHDOG_TIME_BETWEEN_WARN_AND_RESET);

	wd_fd = open(WATCHDOG_DEVICE, O_RDWR | O_NONBLOCK | O_SYNC | O_EXCL);
	if (wd_fd == -1) {
		LOGE("unable to open the wd device (ERRNO=%d)\n",errno);
		goto error_nop;
	}

	/* Setting the kernel watchdog timeout */
	ret = ioctl(wd_fd, WDIOC_SETTIMEOUT, &timeout);
	if (ret) {
		LOGE("Unable to set the watchdog timer timeout\n");
		goto error_open_watchdog;
	}

	/* Setting the kernel deadline timeout */
	ret = ioctl(wd_fd, WDIOC_SETPRETIMEOUT, &pre_timeout);
	if (ret) {
		LOGE("Unable to set the deadline timer timeout\n");
		goto error_open_watchdog;
	}

	/* Restarting the watchdog */
	options = WDIOS_ENABLECARD;
	ret = ioctl(wd_fd, WDIOC_SETOPTIONS, &options);
	if (ret) {
		LOGE("Unable to restart the watchdog\n");
		goto error_open_watchdog;
	}

	/* Then looping for ever */
	for (;;) {
		sleep(DLTIMER_DEFAULT_TIMEOUT-WATCHDOG_KICKING_MARGIN);
		/* kicking the watchdog device */
		LOGI("Refreshing Watchdog Timers\n");
		ret = kick_watchdog(wd_fd);
		if (ret <= 0)
			LOGE("Unable to refresh Watchdog Timers\n");
	}
	LOGW("Watchdog daemon exiting\n");
	return 0;

error_open_watchdog:
	close(wd_fd);
error_nop:
	LOGE("Watchdog daemon exiting\n");
	return 1;
}
