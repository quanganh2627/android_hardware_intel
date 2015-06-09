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
#include "txei_drv.h"

/* define LOG_TAG before including the log header file */
#define LOG_TAG      "libtxei-drv"
#include "txei_log.h"

#undef MEI_IOCTL_TYPE
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

/* Note that this may not be possible with txei */
int mei_get_version_from_sysfs(MEI_HANDLE *my_handle_p) {
    FILE *verfile = NULL;
    unsigned int major = 0;
    unsigned int minor = 0;
    unsigned int hotfix = 0;
    unsigned int build = 0;

    verfile = fopen(MEI_VERSION_SYSFS_FILE, "r");
    if(verfile == NULL) {
        LOGERR("cant open version sysfs file, errno: %x\n", errno);
        return -1;
    }

    fscanf(verfile, "%u.%u.%u.%u",
           &major,
           &minor,
           &hotfix,
           &build);

    if(my_handle_p != NULL) {
        my_handle_p->mei_version.major = (uint8_t)major;
        my_handle_p->mei_version.minor = (uint8_t)minor;
        my_handle_p->mei_version.hotfix = (uint8_t)hotfix;
        my_handle_p->mei_version.build = (uint8_t)build;
    } else {
        LOGINFO("NULL pointer to my_handle, not stuffing handle\n");
    }

    LOGINFO("MEI Version: major %x minor %x hotfix %x build %x\n",
            major, minor, hotfix, build);

    fclose(verfile);
    return 0;
}


MEI_HANDLE *mei_connect(const GUID *guid) {
    MEI_HANDLE *my_handle_p = NULL;
    mei_connect_client_data data;
    int result;

    if(guid == NULL) {
        LOGERR("guid is null in mei_connect\n");
        return NULL;
    }

    my_handle_p = calloc(sizeof(MEI_HANDLE), 1);
    if(my_handle_p == NULL) {
        LOGERR("cannot allocate space for handle\n");
        return NULL;
    }

    /* Set the Guid */
    memcpy(&my_handle_p->guid, guid, sizeof(GUID));

    /* Get the verion which can be done before open or init */
    /* This does not work yet */
    /* result = mei_get_version_from_sysfs(my_handle_p); */
    /* if (result != 0) { */
    /*	LOGERR("cant get version from sysfs\n"); */
    /*	return NULL; */
    /*} */

    /* Open the device file */
    my_handle_p->fd = open(MEI_DEVICE_FILE, O_RDWR);
    if(my_handle_p->fd == -1) {
        LOGERR("can't open mei device\n");
        free(my_handle_p);
        my_handle_p = NULL;
        return NULL;
    }

    /* Send the GUID to establish connection with firmware */
    memset(&data, 0, sizeof(data));
    memcpy(&data.d.in_client_uuid, guid, sizeof(GUID));
    result = ioctl(my_handle_p->fd, IOCTL_MEI_CONNECT_CLIENT, &data);
    if(result) {
        LOGERR("ioctl call failed; result is %x\n", result);
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


void mei_disconnect(MEI_HANDLE *my_handle_p) {
    if(my_handle_p == NULL) {
        LOGERR("null pointer to handle for disconnect\n");
        return;
    }

    if(my_handle_p == NULL) {
        LOGERR("null handle for disconnect\n");
        return;
    }

    close(my_handle_p->fd);
    free(my_handle_p);
}


int mei_sndmsg(MEI_HANDLE *my_handle_p, void *buf, ssize_t my_size) {
    unsigned long timeout = 10000;
    int rv = 0;
    int return_length = 0;
    int error = 0;
    fd_set set;
    struct timeval tv;

    tv.tv_sec =  timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000000;

    if(my_handle_p == NULL) {
        LOGERR("null handle for sndmsg\n");
        return -1;
    }

    if(buf == NULL) {
        LOGERR("null buff for sndmsg\n");
        return -1;
    }

    if(my_handle_p->fd <= 0) {
        LOGERR("file id not valid for sndmsg\n");
        return -1;
    }

    LOGDBG("call write length = %d\n", (int)my_size);

    rv = write(my_handle_p->fd, (void *)buf, my_size);
    if(rv < 0) {
        error = errno;
        LOGERR("write failed with status %d %d\n", rv, error);
        return -1;
    }

    return_length = rv;

    FD_ZERO(&set);
    FD_SET(my_handle_p->fd, &set);
    rv = select(my_handle_p->fd + 1 , &set, NULL, NULL, &tv);
    if(rv > 0 && FD_ISSET(my_handle_p->fd, &set)) {
        LOGINFO("write success\n");
    } else if(rv == 0) {
        LOGERR("write failed on timeout with status\n");
        return -1;
    } else { //rv<0
        LOGERR("write failed on select with status %d\n", rv);
        return -1;
    }

    rv = return_length;
    return rv;
}


int mei_rcvmsg(MEI_HANDLE *my_handle_p, void *buf, ssize_t my_size) {
    int rv = 0;
    int error = 0;

    if(my_handle_p == NULL) {
        LOGERR("null handle for rcvmsg\n");
        return -1;
    }

    if(buf == NULL) {
        LOGERR("null buff for rcvmsg\n");
        return -1;
    }

    if(my_handle_p->fd <= 0) {
        LOGERR("file id not valid for rcvmsg\n");
        return -1;
    }

    LOGDBG("call read length = %d\n", (int)my_size);

    rv = read(my_handle_p->fd, (void *)buf, my_size);
    if(rv < 0) {
        error = errno;
        LOGERR("read failed with status %d %d\n", rv, error);
        return -1;
    } else {
        LOGINFO("read successful and read %d\n", rv);
    }

    return rv;
}


int mei_snd_rcv(MEI_HANDLE *my_handle_p, void *snd_buf, ssize_t snd_size, void *rcv_buf, ssize_t rcv_size) {
    int Status = 0;

    Status = mei_sndmsg(my_handle_p, snd_buf, snd_size);
    if(Status <= 0) {
        return Status;
    }
    if(Status != snd_size) {
        LOGERR("mei_sndmsg succeeded, but wrong size was written\n");
        return -1;
    }

    Status = mei_rcvmsg(my_handle_p, rcv_buf, rcv_size);
    if(Status <= 0 || Status != rcv_size) {
        return Status;
    }
    if(Status <= 0 || Status != rcv_size) {
        LOGERR("mei_rcvmsg succeeded, but wrong size was read\n");
        return -1;
    }

    return 0;
}


MEI_MM_DMA *mei_alloc_dma(ssize_t my_size) {

    int result = 0;
    MEI_MM_DMA *my_dma = NULL;

    my_dma = calloc(sizeof(MEI_MM_DMA), 1);
    if(my_dma == NULL) {
        LOGERR("oops, cannot allocate MEI_MM_DMA structure\n");
        return NULL;
    }

    my_dma->data.size = (__u64)my_size;

    my_dma->fd = open("/dev/meimm", O_RDWR);
    if(my_dma->fd <= 0) {
        LOGERR("cant open the /dev/meimm\n");
        LOGERR("errno is %x\n", errno);
        my_dma->fd = 0;
        free(my_dma);
        return NULL;
    }

    /* call ioctl to allocate */
    result = ioctl(my_dma->fd, IOCTL_MEI_MM_ALLOC, &my_dma->data);
    if(result != 0) {
        LOGERR("ioctl for memory alloc for snd dma failed\n");
        LOGERR("errno is %x\n", errno);
        close(my_dma->fd);
        my_dma->fd = 0;
        free(my_dma);
        return NULL;
    }

    /* call mmap */
    my_dma->dmabuffer = mmap(NULL, my_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, my_dma->fd, 0);

    if(my_dma->dmabuffer == NULL) {
        LOGERR("mmap for dma buffer failed\n");
        LOGERR("errno is %x\n", errno);
        close(my_dma->fd);
        return NULL;
    }

    return my_dma;
}


void mei_clear_dma(MEI_MM_DMA *my_dma) {
    int result = 0;
    if(my_dma == NULL) {
        LOGINFO("null my_dma for clear; doing nothing\n");
        return;
    }

    result = ioctl(my_dma->fd, IOCTL_MEI_MM_FREE, &my_dma->data);
    if(result != 0) {
        LOGERR("ioctl for memory free for snd dma failed\n");
        LOGERR("errno is %x\n", errno);
        close(my_dma->fd);
        my_dma->fd = 0;
        return;
    }

    close(my_dma->fd);
    free(my_dma);
    return;
}
