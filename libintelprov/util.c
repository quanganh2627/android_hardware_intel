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

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include "util.h"

#define pr_perror(x)	fprintf(stderr, "%s failed: %s\n", x, strerror(errno))

#define FILEMODE  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH

int safe_read(int fd, void *data, size_t size)
{
	int ret;
	unsigned char *bytes = (unsigned char *)data;
	while (size) {
		ret = read(fd, bytes, size);
		if (ret <= 0 && errno != EINTR) {
			pr_perror("read");
			return -1;
		}
		size -= ret;
		bytes += ret;
	}
	return 0;
}

int file_read(const char *filename, void **datap, size_t *szp)
{
	struct stat sb;
	size_t sz;
	char *data;
	int fd;

	if (stat(filename, &sb)) {
		printf("file_read: can't stat %s: %s\n", filename, strerror(errno));
		return -1;
	}

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		printf("file_read: can't open file %s: %s\n",
				filename, strerror(errno));
		return -1;
	}

	sz = sb.st_size;
	data = malloc(sz);
	if (!datap) {
		printf("memory allocation failure: %s\n",
				strerror(errno));
		return -1;
	}

	if (safe_read(fd, data, sz)) {
		close(fd);
		free(data);
		return -1;
	}

	*datap = data;
	*szp = sz;
	close(fd);
	return 0;
}

int file_write(const char *filename, const void *data, size_t sz)
{
	int fd;
	int ret;
	const unsigned char *what = (const unsigned char *)data;

	fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, FILEMODE);
	if (fd < 0) {
		printf("file_write: Can't open file %s: %s\n",
				filename, strerror(errno));
		return -1;
	}

	while (sz) {
		ret = write(fd, what, sz);
		if (ret <= 0 && errno != EINTR) {
			printf("file_write: Failed to write to %s: %s\n",
					filename, strerror(errno));
			close(fd);
			return -1;
		}
		what += ret;
		sz -= ret;
	}
	fsync(fd);
	close(fd);
	return 0;
}

int file_size(const char *filename)
{
	int ret;
	struct stat stat_buf;

	ret = stat(filename, &stat_buf);
	if (ret == -1) {
		error("Failed to get stat on %s file, %s.",
		      filename, strerror(errno));
		return -1;
	}

	return stat_buf.st_size;
}

void *file_mmap(const char *filename, size_t length, bool writable)
{
	void *ret = NULL;
	int fd;

	fd = open(filename, writable ? O_RDWR : O_RDONLY);
	if (fd == -1) {
		error("Failed to open %s file, %s.",
		      filename, strerror(errno));
		goto exit;
	}

	ret = mmap(NULL, length, (writable ? PROT_WRITE : 0) | PROT_READ, MAP_SHARED, fd, 0);
	if (ret == MAP_FAILED) {
		error("Failed to map %s file into memory, %s.",
		      filename, strerror(errno));
		goto close;
	}

close:
	close(fd);
exit:
	return ret;
}

int file_string_write(const char *filename, const char *what)
{
	return file_write(filename, what, strlen(what));
}

void dump_trace_file(const char *filename)
{
	char buf[1024];
	FILE *fp;

	fp = fopen(filename, "r");
	if (!fp) {
		printf("can't open trace file %s: %s\n",
				filename, strerror(errno));
		return;
	}

	while (fgets(buf, sizeof(buf), fp))
		printf("%s", buf);
	fclose(fp);
}

int snhexdump(char *str, size_t size, const unsigned char *data, unsigned int sz)
{
	int ret=0;
	while(sz > 0 && size >4) {
		int len = snprintf(str, 4, "%02x ",*data);
		str += len;
		size -= len;
		sz--;
		data++;
		ret += len;
	}
	return ret;
}

void hexdump_buffer(const unsigned char *buffer, unsigned int buffer_size,
		void (*printrow)(const char *text), unsigned int bytes_per_row)
{
	unsigned int left = buffer_size;
	static char buffer_txt[1024];

	while (left > 0) {
		unsigned int row = left < bytes_per_row ? left : bytes_per_row;
		unsigned int rowlen = snhexdump(buffer_txt, sizeof(buffer_txt)
				- 1, buffer, row);
		snprintf(buffer_txt + rowlen,
				sizeof(buffer_txt) - rowlen - 1, "\n");
		printrow(buffer_txt);
		buffer += row;
		left -= row;
	}
}

void twoscomplement(unsigned char *cs, unsigned char *buf, unsigned int size)
{
	*cs = 0;
	while (size > 0) {
		*cs += *buf;
		buf++;
		size--;
	}
	*cs = (~*cs) + 1;
}

int is_hex(char c) {
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

void eprintf(const char *msg)
{
	fprintf(stderr, "%s", msg);
}

/* Output stream management.  */
static void (*error_fun)(const char *msg) = (void (*)(const char *))eprintf;
static void (*print_fun)(const char *msg) = (void (*)(const char *))printf;

#define MSG_BUF_LENGTH 256

void error(const char *fmt, ...)
{
	char buf[MSG_BUF_LENGTH];

	va_list argptr;
	va_start(argptr, fmt);
	vsnprintf(buf, sizeof(buf), fmt, argptr);
	va_end(argptr);

	error_fun(buf);
}

void print(const char *fmt, ...)
{
	char buf[MSG_BUF_LENGTH];

	va_list argptr;
	va_start(argptr, fmt);
	vsnprintf(buf, sizeof(buf), fmt, argptr);
	va_end(argptr);

	print_fun(buf);
}

/* External program management.  */
static int allocated_time_expired;

static void sigalarm_handler(int signal)
{
	allocated_time_expired = 1;
}

static void run_program(const char *path, int output_fd, char *argv[])
{
	int ret;
	int err_fd;

	if ((err_fd = dup(STDERR_FILENO)) == -1) {
		error("Failed to save stderr.");
		goto exit;
	}

	if ((dup2(output_fd, STDOUT_FILENO) == -1) ||
	    (dup2(output_fd, STDERR_FILENO) == -1)) {
		error("Failed to redirect %s program output.", path);
		goto exit;
	}

	argv[0] = basename(path);

	ret = execv(path, argv);
	if (ret != -1)
		goto exit;


	FILE *file = fdopen(err_fd, "w");
	if (file == NULL)
		goto exit;

	fprintf(file, "execv failed on %s, %s\n", path, strerror(errno));
	fclose(file);

exit:
	_exit(EXIT_FAILURE);
}

int call_program(const char *path, const char *log_file,
		 const char *pass_string, unsigned int timeout, char *argv[])
{
	pid_t pid;
	char buf[strlen(pass_string) + 1];
	int fd, status, ret;
	FILE *file = NULL;
	int fun_ret = EXIT_FAILURE;

	fd = open(log_file, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);

	if (fd == -1) {
		error("Failed to open %s file.", log_file);
		return EXIT_FAILURE;
	}

	pid = fork();
	if (pid == -1) {
		error("Fork() systemcall failed, %s.", strerror(errno));
		goto close;
	}

	if (pid == 0)
		run_program(path, fd, argv);

	const struct sigaction alarm_sigaction = {
		.sa_handler = sigalarm_handler,
	};
	struct sigaction old_alarm_sigaction;
	ret = sigaction(SIGALRM, &alarm_sigaction, &old_alarm_sigaction);
	if (ret == -1) {
		error("Failed to install SIGALRM handler.");
		goto close;
	}

	alarm(timeout);
	ret = waitpid(pid, &status, 0);
	sigaction(SIGALRM, &old_alarm_sigaction, NULL);
	if (ret == -1) {
		if (allocated_time_expired) {
			error("%s program takes too long, aborting.", path);
			kill(pid, SIGTERM);
		} else
			error("Wait for %s program completion failed.", path);
		goto close;
	}

	if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
		error("%s program ended unexpectedly.", path);
		goto close;
	}

	if (lseek(fd, 0, SEEK_SET) == -1) {
		error("Seek failed on %s file, %s.", log_file, strerror(errno));
		goto close;
	}

	file = fdopen(fd, "r+");
	if (file == NULL) {
		error("Failed to fdopen on %s (already opened via open), %s.",
		      log_file, strerror(errno));
		goto close;
	}

	while (fgets(buf, sizeof(buf), file) != NULL)
		if (strstr(buf, pass_string)) {
			fun_ret = EXIT_SUCCESS;
			goto close;
		}

	error("The %s program failed to perform its task.", path);

close:
	if (file)
		fclose(file);
	else
		close(fd);

	return fun_ret;
}


/* Initialization.  */
void util_init(void (*err_fun)(const char *), void (*pr_fun)(const char *))
{
	error_fun = err_fun ? err_fun : error_fun;
	print_fun = pr_fun ? pr_fun : print_fun;
}
