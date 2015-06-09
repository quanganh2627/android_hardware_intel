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
#define EXTTIMER_DEFAULT_TIMEOUT 40		/* timer frequency to write to the WD device */
#define WATCHDOG_EXTRA_MARGIN 20		/* extra margin for scheduling next timer MSI, */
						/* must be >20s to comply with NMI Watchdog */
#define WATCHDOG_TIME_BETWEEN_WARN_AND_RESET 15	/* time to dump kernel status before resetting */
#define WATCHDOG_DEVICE "/dev/watchdog"

#define WDIOC_SETTIMERTIMEOUT     _IOW(WATCHDOG_IOCTL_BASE, 11, int)
#define WDIOC_GETTIMERTIMEOUT     _IOW(WATCHDOG_IOCTL_BASE, 12, int)

/* This kicks the watchdog device by reading and writing the device */
static int kick_watchdog(int fd)
{
	char buf = 'R';
	int ret;

	ret = read(fd, &buf, 1);
	if (ret <= 0)
		return ret;

	return write(fd, &buf, 1);
}

/* timeout: maximum interval between 2 timer interrupts */
int main(int argc, char **argv)
{
	int wd_fd, opt;
	fd_set set;
	int n, ret, timeout = EXTTIMER_DEFAULT_TIMEOUT, wd_timeout;
	int pre_timeout = WATCHDOG_TIME_BETWEEN_WARN_AND_RESET;
	int options;

	/* Start logging here */
	LOG_INIT(LOG_TAG);
	LOGW("Watchdog daemon starting\n");

	/* Look through the options */
	while ((opt = getopt(argc, argv, "t:")) != -1) {
		switch (opt) {
			case 't':
				timeout = atoi(optarg);
				break;
			default:
				LOGE("invalid parameters\n");
				exit (-1);
		}
	}

	wd_timeout = timeout + pre_timeout + WATCHDOG_EXTRA_MARGIN;

	/* Real job from here */
	LOGW("Watchdog Timer set to timer=%d margin=%d extra_margin=%d\n",
	     timeout,
	     WATCHDOG_TIME_BETWEEN_WARN_AND_RESET,
	     WATCHDOG_EXTRA_MARGIN);

	wd_fd = open(WATCHDOG_DEVICE, O_RDWR | O_NONBLOCK | O_SYNC | O_EXCL);
	if (wd_fd == -1) {
		LOGE("unable to open the wd device (ERRNO=%d)\n",errno);
		goto error_nop;
	}

	/* Stopping the watchdog */
	options = WDIOS_DISABLECARD;
	ret = ioctl(wd_fd, WDIOC_SETOPTIONS, &options);
	if (ret) {
		LOGE("Unable to stop the watchdog\n");
		goto error_open_watchdog;
	}

	/* Setting the kernel watchdog timeout */
	ret = ioctl(wd_fd, WDIOC_SETTIMEOUT, &wd_timeout);
	if (ret) {
		LOGE("Unable to Set Watchdog Timer Threshold\n");
		goto error_open_watchdog;
	}

	/* Setting the kernel watchdog pretimeout */
	ret = ioctl(wd_fd, WDIOC_SETPRETIMEOUT, &pre_timeout);
	if (ret) {
		LOGE("Unable to set the watchdog pretimeout\n");
		goto error_open_watchdog;
	}

	/* Setting the timer timeout */
	ret = ioctl(wd_fd, WDIOC_SETTIMERTIMEOUT, &timeout);
	if (ret) {
		LOGE("Unable to set the watchdog timer_timeout\n");
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
		FD_ZERO(&set);
		FD_SET(wd_fd, &set);

		LOGI("waiting for a timer interrupt\n");

		/* Waiting for an event (timer_msi or watchdog_msi) */
		n = select(wd_fd+1, &set, NULL, NULL, NULL);
		if (n == -1) {
			LOGE("select system call failed\n");
			sleep(5);
			continue;
		} else if (n == 0) {
			LOGE("select returned 0: abnormal\n");
			sleep(5);
			continue;
		}

		/* Do we have a timer_msi ? */
		if (FD_ISSET(wd_fd, &set)) {
			LOGD("Timer interrupt\n");

			/* kicking the watchdog device */
			ret = kick_watchdog(wd_fd);
			if (ret <= 0)
				LOGE("Unable to refresh Watchdog Timer\n");
		}
	}
	LOGW("Watchdog daemon exiting\n");
	return 0;

error_open_watchdog:
	close(wd_fd);
error_nop:
	LOGE("Watchdog daemon exiting\n");
	return 1;
}
