#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include "txei.h"

#undef MEI_IOCTL
#undef IOCTL_MEI_CONNECT_CLIENT
#define MEI_IOCTL_TYPE 0x48
#define IOCTL_MEI_CONNECT_CLIENT \
    _IOWR(MEI_IOCTL_TYPE, 0x01, mei_connect_client_data)

/*
 * IOCTL Connect Client Data structure
 */
typedef struct mei_connect_client_data_t {
	union {
		GUID in_client_uuid;
		MEI_CLIENT out_client_properties;
	} d;
} mei_connect_client_data;

// const GUID my_guid = {0x3c4852d6, 0xd478, 0x4f46, {0xb0, 0x5e, 0xb5, 0xed, 0xc1, 0xaa, 0x43, 0x0a}};

const GUID my_old_guid = {0xafa19346, 0x7459, 0x4f09, {0x9d, 0xad, 0x36, 0x61, 0x1f, 0xe4, 0x28, 0x58}};

MEI_VERSION my_mei_version;

void mei_print_buffer(char *label, uint8_t *buf, ssize_t len)
{
	int a;
	if ((label == NULL) || (buf == NULL)) {
		printf("cant print null buffer or null label\n");
		return;
	}

	printf("Printing %s buffer\n", label);
	for (a = 0; a < len; a += 1) {
		printf("%02x", *(buf + a));
		if ((a % 8) == 7) {
			printf("\n");
		}
	}
	printf("\n\n");
}

/* Note that this may not be possible with txei */
int mei_get_version_from_sysfs(MEI_HANDLE *my_handle_p)
{
	FILE *verfile = NULL;
	unsigned int major = 0;
	unsigned int minor = 0;
	unsigned int hotfix = 0;
	unsigned int build = 0;

	verfile = fopen(MEI_VERSION_SYSFS_FILE, "r");
	if (verfile == NULL) {
		printf("cant open version sysfs file, errno: %x\n", errno);
		return -1;
	}

	fscanf(verfile, "%u.%u.%u.%u",
		&major,
		&minor,
		&hotfix,
		&build);

	if (my_handle_p != NULL) {
		my_handle_p->mei_version.major = (uint8_t)major;
		my_handle_p->mei_version.minor = (uint8_t)minor;
		my_handle_p->mei_version.hotfix = (uint8_t)hotfix;
		my_handle_p->mei_version.build = (uint8_t)build;
	} else {
		printf("NULL pointer to my_handle, not stuffing handle\n");
	}

	printf("MEI Version: major %x minor %x hotfix %x build %x\n",
		major, minor, hotfix, build);

	fclose(verfile);
	return 0;
}

/**
 * Opens the device /dev/mei, then sends ioctl to
 * establish communication with heci client firmware
 * module with guid
 * Return is MEI_HANDLE which has open file ID as one
 * of its elements
 */
MEI_HANDLE *mei_connect(const GUID *guid)
{
	MEI_HANDLE *my_handle_p = NULL;
	mei_connect_client_data data;
	int result;

	if (guid == NULL) {
		printf("guid is null in mei_connect\n");
		return NULL;
	}

	my_handle_p = calloc(sizeof(MEI_HANDLE),1);
	if (my_handle_p == NULL) {
		printf("cannot allocate space for handle\n");
		return NULL;
	}

	/* Set the Guid */
	memcpy(&my_handle_p->guid, guid, sizeof(GUID));

	/* Get the verion which can be done before open or init */
	/* This does not work yet */
	/* result = mei_get_version_from_sysfs(my_handle_p); */
	/* if (result != 0) { */
	/*	printf("cant get version from sysfs\n"); */
	/*	return NULL; */
	/*} */

	/* Open the device file */
	my_handle_p->fd = open(MEI_DEVICE_FILE, O_RDWR);
	if (my_handle_p->fd == -1) {
		printf("can't open mei device\n");
		free(my_handle_p);
		my_handle_p = NULL;
		return NULL;
	}

	/* Send the GUID to establish connection with firmware */
	memset(&data, 0, sizeof(data));
	memcpy(&data.d.in_client_uuid, guid, sizeof(GUID));
	result = ioctl(my_handle_p->fd, IOCTL_MEI_CONNECT_CLIENT, &data);
	if (result) {
		printf("ioctl call failed; result is %x\n", result);
		close(my_handle_p->fd);
		free(my_handle_p);
		my_handle_p = NULL;
		return NULL;
	}

	/* Grab the client properties */
	memcpy(&my_handle_p->client_properties, &data.d.out_client_properties,
		sizeof(MEI_CLIENT));

	return my_handle_p;
}

/**
 * Close connection and close the device. Handle is freed
 * and does not exist ahen this is done
 */
void mei_disconnect(MEI_HANDLE *my_handle_p)
{
	if (my_handle_p == NULL) {
		printf("null pointer to handle for disconnect\n");
		return;
	}

	if (my_handle_p == NULL) {
		printf("null handle for disconnect\n");
		return;
	}

	close(my_handle_p->fd);
	free(my_handle_p);
}

int mei_sndmsg(MEI_HANDLE *my_handle_p, uint8_t *buf, ssize_t my_size)
{
	unsigned long timeout = 10000;
	int rv = 0;
	int return_length =0;
	int error = 0;
	fd_set set;
	struct timeval tv;

	tv.tv_sec =  timeout / 1000;
	tv.tv_usec =(timeout % 1000) * 1000000;

	if (my_handle_p == NULL) {
		printf("null handle for sndmsg\n");
		return -1;
	}

	if (buf == NULL) {
		printf("null buff for sndmsg\n");
		return -1;
	}

	if (my_handle_p->fd <= 0) {
		printf("file id not valid for sndmsg\n");
		return -1;
	}


//	fprintf(stdout, "call write length = %d\n", (int)my_size);

	rv = write(my_handle_p->fd, (void *)buf, my_size);
	if (rv < 0) {
		error = errno;
		fprintf(stderr,"write failed with status %d %d\n", rv, error);
		return -1;
	}

	return_length = rv;

	FD_ZERO(&set);
	FD_SET(my_handle_p->fd, &set);
	rv = select(my_handle_p->fd+1 ,&set, NULL, NULL, &tv);
	if (rv > 0 && FD_ISSET(my_handle_p->fd, &set)) {
		fprintf(stderr, "write success\n");
	}
	else if (rv == 0) {
		fprintf(stderr, "write failed on timeout with status\n");
		return -1;
	}
	else { //rv<0
		fprintf(stderr, "write failed on select with status %d\n", rv);
		return -1;
	}

	rv = return_length;
	return rv;
}

/**
 * Allocat a DMA buffer
 * The returns a pointer to a MEI_MM_DMA structure.
 * The caller is responsible for holding onto that
 * structure and then having it de-allocated using
 * the mei_clear_dma api call.
 */
MEI_MM_DMA *mei_alloc_dma(ssize_t my_size)
{

	int result = 0;
	MEI_MM_DMA *my_dma = NULL;

	my_dma = calloc(sizeof(MEI_MM_DMA), 1);
	if (my_dma == NULL) {
		printf("oops, cannot allocate MEI_MM_DMA structure\n");
		return NULL;
	}

	my_dma->data.size = (__u64)my_size;

	my_dma->fd = open("/dev/meimm", O_RDWR);
	if (my_dma->fd <= 0) {
		printf("cant open the /dev/meimm\n");
		printf("errno is %x\n", errno);
		my_dma->fd = 0;
		free(my_dma);
		return NULL;
	}

	/* call ioctl to allocate */
	result = ioctl(my_dma->fd, IOCTL_MEI_MM_ALLOC, &my_dma->data);
	if (result != 0) {
		printf("ioctl for memory alloc for snd dma failed\n");
		printf("errno is %x\n", errno);
		close(my_dma->fd);
		my_dma->fd = 0;
		free(my_dma);
		return NULL;
	}

	/* call mmap */
	my_dma->dmabuffer = mmap(NULL, my_size, PROT_READ | PROT_WRITE,
		MAP_SHARED, my_dma->fd, 0);

	if (my_dma->dmabuffer == NULL) {
		printf("mmap for dma buffer failed\n");
		printf("errno is %x\n", errno);
		close(my_dma->fd);
		return NULL;
	}

	return my_dma;
}

void mei_clear_dma(MEI_MM_DMA *my_dma)
{
	int result = 0;
	if (my_dma == NULL) {
		printf("null my_dma for clear; doing nothing\n");
		return;
	}

	result = ioctl(my_dma->fd, IOCTL_MEI_MM_FREE, &my_dma->data);
	if (result != 0) {
		printf("ioctl for memory free for snd dma failed\n");
		printf("errno is %x\n", errno);
		close(my_dma->fd);
		my_dma->fd = 0;
		return;
	}

	close(my_dma->fd);
	free(my_dma);
	return;
}

int mei_rcvmsg(MEI_HANDLE *my_handle_p, uint8_t *buf, ssize_t my_size)
{
	int rv = 0;
	int error = 0;

	if (my_handle_p == NULL) {
		printf("null handle for rcvmsg\n");
		return -1;
	}

	if (buf == NULL) {
		printf("null buff for rcvmsg\n");
		return -1;
	}

	if (my_handle_p->fd <= 0) {
		printf("file id not valid for rcvmsg\n");
		return -1;
	}


//	fprintf(stdout, "call read length = %d\n", (int)my_size);

	rv = read(my_handle_p->fd, (void *)buf, my_size);
	if (rv < 0) {
		error = errno;
		fprintf(stderr,"read failed with status %d %d\n", rv, error);
		return -1;
	}

	else {
		fprintf(stderr, "read successful and read %d\n", rv);
	}

	return rv;
}
