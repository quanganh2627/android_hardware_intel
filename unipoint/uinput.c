#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <uinput.h>

#define NELEM(x) (sizeof((x)) / sizeof((x[0])))
#define CONFIG_USE_DAVI_ALGORITHM

#define UINPUT_DEBUG 0

int uinput_write_event(int uinput_fd, const struct input_event *event)
{
    size_t bytes;
    bytes = write(uinput_fd, event, sizeof(struct input_event));
    if (bytes != sizeof(struct input_event))
        return -1;
    return 0;
}

int uinput_write(int uinput_fd, uint16_t ev_type, uint16_t ev_code,
                  int32_t ev_value)
{
    struct input_event event;
    memset(&event, 0, sizeof(event));
    gettimeofday(&event.time, 0);
    event.type = ev_type;
    event.code = ev_code;
    event.value = ev_value;

    if (UINPUT_DEBUG) {
	fprintf(stderr, "[%s] writing event:\n"
		"\tType: %i\tCode: %i\tValue:%i\t\n",
		__FILE__, event.type, event.code, event.value);
    }

    return uinput_write_event(uinput_fd, &event);
}

int uinput_syn(int uinput_fd)
{
    return uinput_write(uinput_fd, EV_SYN, SYN_REPORT, 0);
}

/* No udev dev library in Android. Use this constant */
const char *uinput_get_uinput_path(void)
{
    return "/dev/uinput";
}

int uinput_open(void)
{
    int uinput_fd;
    const char *uinput_devnode;

    if ((uinput_devnode = uinput_get_uinput_path()) == NULL)
        return -1;

    if ((uinput_fd = open(uinput_devnode, O_WRONLY | O_NONBLOCK)) == -1)
        return -1;

    return uinput_fd;
}

FILE *sample_open(char *op, char *fname)
{
	FILE *fd = NULL;

	if (fname && fname[0]) {
		if (op && *op == 'w')
			fd = fopen(fname, "wb");
		else
			fd = fopen(fname, "rb");
	}
	return fd;
}

int uinput_create(int uinput_fd, const struct uinput_user_dev *user_dev)
{
    size_t bytes;

    bytes = write(uinput_fd, user_dev, sizeof(struct uinput_user_dev));
    if (bytes != sizeof(struct uinput_user_dev))
        return -1;

    if (ioctl(uinput_fd, UI_DEV_CREATE) == -1)
        return -1;

    if (getenv("DISPLAY"))
        sleep(1);

    return 0;
}

int uinput_destroy(int uinput_fd)
{
    int original_errno;

    if (ioctl(uinput_fd, UI_DEV_DESTROY) == -1) {
        original_errno = errno;
        close(uinput_fd);
        errno = original_errno;
        return -1;
    }

    return close(uinput_fd);
}

int uinput_set_capabilities(int uinput_fd, uint16_t ev_type,
                             const int *ev_codes, size_t ev_codes_len)
{
    int i;
    int io;

    if (ioctl(uinput_fd, UI_SET_EVBIT, ev_type) == -1)
        return -1;

    switch (ev_type) {
    case EV_REL:
        io = UI_SET_RELBIT;
        break;
    case EV_MSC:
        io = UI_SET_MSCBIT;
        break;
    case EV_KEY:
        io = UI_SET_KEYBIT;
        break;
    case EV_ABS:
        io = UI_SET_ABSBIT;
        break;
    case EV_SW:
        io = UI_SET_SWBIT;
        break;
    case EV_LED:
        io = UI_SET_LEDBIT;
        break;
    case EV_SND:
        io = UI_SET_SNDBIT;
        break;
    case EV_FF:
        io = UI_SET_FFBIT;
        break;
    default:
        return -2;
    }

    for (i = 0; i < ev_codes_len; ++i) {
        int ev_code = ev_codes[i];
        if (ioctl(uinput_fd, io, ev_code) == -1)
            return -1;
    }
    return 0;
}

int uinput_gen_setup(int *uinput_fd, FILE **sample_fd, char *op, char *fname)
{
	static int rel_axes[] = { REL_X, REL_Y };
#ifdef CONFIG_USE_DAVI_ALGORITHM
	
		static int btn_axes[] = { BTN_MOUSE,KEY_UP,KEY_RIGHT,KEY_LEFT,KEY_DOWN,KEY_VOLUMEUP,KEY_VOLUMEDOWN };
#else
		static int btn_axes[] = { BTN_MOUSE };
#endif

	struct uinput_user_dev user_dev;
	int fd;
	int rc;

	memset(&user_dev, 0, sizeof(struct uinput_user_dev));
	strcpy(user_dev.name, "unipoint-device");

	fd = uinput_open();
	*sample_fd = sample_open(op, fname);
	if (fd == -1) {
		perror("uinput_init cannot setup input: ");
		return -1;
	}

	rc = uinput_set_capabilities(fd, EV_REL, rel_axes, NELEM(rel_axes));
	rc |= uinput_set_capabilities(fd, EV_KEY, btn_axes, NELEM(btn_axes));
	if (rc == -1) {
		perror("unable to set caps: ");
		uinput_destroy(fd);
		return -1;
	}

	rc = uinput_create(fd, &user_dev);

	*uinput_fd = fd;
	return rc;
}

