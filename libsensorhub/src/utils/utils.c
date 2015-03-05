#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "../include/utils.h"

#define MAX_LOG_SIZE 256

static const char *log_file = "/data/sensorhubd.log";

static message_level current_level = CRITICAL;

void set_log_level(message_level log_level)
{
	if (log_level >= MAX_LEVEL)
		return;

	current_level = log_level;
}

void log_message(const message_level level, char *char_ptr, ...)
{
	char buffer[MAX_LOG_SIZE];
	va_list list_ptr;

	if (level > current_level)
		return;

	va_start(list_ptr, char_ptr);
	vsnprintf(buffer, MAX_LOG_SIZE, char_ptr, list_ptr);
	va_end(list_ptr);

	FILE *logf = fopen(log_file, "a");
	if (logf) {
		fprintf(logf, "%ld: %d %s", time(NULL), level, buffer);
		fclose(logf);
	} else {
		printf("Open log file failed, errno is %d \n", errno);
	}
}

int max_common_factor(int data1, int data2)
{
	int m = data1, n = data2, r;

	while (n != 0) {
		r = (m % n);
		m = n;
		n = r;
	}

	return m;
}

int least_common_multiple(int data1, int data2)
{
	int m = data1, n = data2, r;

	while (n != 0) {
		r = (m % n);
		m = n;
		n = r;
	}

	return ((data1 * data2) / m);
}

/* return 0 if not first; 1 if first */
int is_first_instance()
{
	int fd, count;
	char buf[8];

	system("pidof sensorhubd | wc -w > /data/pids");

	fd = open("/data/pids", 0);

	read(fd, buf, 8);
	/* To avoid the buffer overflow */
	buf[7] = '\0';
	count = atoi(buf);

	close(fd);

	if (count > 1)
		return 0;
	return 1;
}
