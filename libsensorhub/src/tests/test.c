#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <fcntl.h>

#define PSH_DATA_NODE "/sys/class/hwmon/hwmon2/device/data"
#define PSH_DATA_SIZE_NODE "/sys/class/hwmon/hwmon2/device/data_size"

int get_streaming_data(char *buf, size_t size, int timeout)
{
	char tmp[256];
	struct pollfd fds[1];
	int fd, fd2;
	int data;
	int ret = -1;
	int total = 0;

	fd = open(PSH_DATA_NODE, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open streaming_data error.\n");
		goto exit;
	}
	fd2 = open(PSH_DATA_SIZE_NODE, O_RDONLY);
	if (fd2 < 0) {
		fprintf(stderr, "open streaming_data_size error.\n");
		goto close;
	}
again:
	ret = read(fd2, tmp, 256);
	if (ret <= 0)
		goto close1;
	sscanf(tmp, "%d", &data);
	if (!data) {
		fds[0].fd = fd2;
		fds[0].events = POLLPRI;

		/* blocked for data */
		printf("before block\n");
		ret = poll(fds, 1, timeout);
		printf("after block\n");
		if (ret <= 0)
			goto close1;
		lseek(fd2, 0, SEEK_SET);
		goto again;
	}
	while (size) {
		ret = read(fd, buf, size);
		if (ret > 0) {
			buf += ret;
			size -= ret;
			total += ret;
		} else
			break;
	}
	ret = total;
close1:
	close(fd2);
close:
	close(fd);
exit:
	return ret;
}

int main(void)
{
	char buf[1024];

	get_streaming_data(buf, 1024, 10000);

	return 0;
}


