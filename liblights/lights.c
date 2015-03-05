/*
 * Copyright 2008, The Android Open Source Project
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

#define LOG_TAG "lights"

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <cutils/log.h>
#include <hardware/lights.h>
#include <linux/input.h>

/* #ifdef LIGHT_BUTTONS_AUTO_POWEROFF */

#define LIGHT_LED_OFF   0
#define LIGHT_LED_FULL  255

#define LIGHT_PATH_BASE "/sys/class"

#ifdef GRAPHIC_IS_GEN
#define LIGHT_ID_BACKLIGHT_PATH                         \
    LIGHT_PATH_BASE"/backlight/intel_backlight/brightness"
#define LIGHT_ID_MAX_BACKLIGHT_PATH                     \
    LIGHT_PATH_BASE"/backlight/intel_backlight/max_brightness"
#else
#define LIGHT_ID_BACKLIGHT_PATH                         \
    LIGHT_PATH_BASE"/backlight/psb-bl/brightness"
#define LIGHT_ID_MAX_BACKLIGHT_PATH                     \
    LIGHT_PATH_BASE"/backlight/psb-bl/max_brightness"
#endif /* CONFIG_INTEL_GEN_GRAPHICS */

/* if cdk board have leds, new sys path related to leds should be defined. */
#define LIGHT_ID_KEYBOARD_PATH                          \
    LIGHT_PATH_BASE"/keyboard-backlight/brightness"
#define LIGHT_ID_BUTTONS_PATH                           \
	LIGHT_PATH_BASE"/leds/intel_keypad_led/brightness"
#define LIGHT_ID_BATTERY_PATH                           \
    LIGHT_PATH_BASE"/battery-backlight/brightness"
#define LIGHT_ID_NOTIFICATIONS_PATH                     \
    LIGHT_PATH_BASE"/notifications-backlight/brightness"
#define LIGHT_ID_ATTENTION_PATH                         \
    LIGHT_PATH_BASE"/attention-baklight/brightness"

#define BRIGHT_MAX_BAR      255
#define bright_to_intensity(__max, __br, __its)     \
        do {                                        \
                __its = __max * __br;      \
                __its = __its / BRIGHT_MAX_BAR;     \
        } while (0)

#define WAKE_EVENT_MAX		8
#define WAKE_KEY_MAX		32
#define KEY_ANY			(KEY_MAX+0x1)

struct light_wake_event {
	char	*file;
	int	type;
	int	key[WAKE_KEY_MAX];
	int	fd;
};
struct light_info {
	char *name;
	int fd;
	unsigned char brightness;
	unsigned char brightness_status;
	int need_update;
	int need_auto_off;
	int auto_off_time;
	pthread_mutex_t lock;
	pthread_cond_t  cond;
	struct light_wake_event events[WAKE_EVENT_MAX];
};

#ifdef LIGHT_BUTTONS_AUTO_POWEROFF
#define TOUCH_KEY_EVENT_PATH "/dev/input/event1"
static struct light_info button_light_info = {
	.name = "button light",
	.auto_off_time = 5,
	.events = {
		/*touch key*/
		{.type = EV_KEY, .key = {KEY_ANY, -1}, .file = TOUCH_KEY_EVENT_PATH,},
	},
};
#endif

struct lights_fds {
    int backlight;
    int keyboard;
    int buttons;
    int battery;
    int notifications;
    int attention;
};

static struct lights_ctx {
    struct lights_fds fds;
#ifdef LIGHT_BUTTONS_AUTO_POWEROFF
    struct light_info *button_info;
#endif
} *context;

static int get_max_brightness()
{
    char tmp_s[8];
    int fd, value, ret;
    char *path = LIGHT_ID_MAX_BACKLIGHT_PATH;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
            LOGE("faild to open %s, ret = %d\n", path, errno);
            return -errno;
    }

    ret = read(fd, &tmp_s[0], sizeof(tmp_s));
    if (ret < 0) {
	    close(fd);
	    return ret;
    }

    value = atoi(&tmp_s[0]);

    close(fd);

    return value;
}

static int write_brightness(int fd, unsigned char brightness)
{
    char buff[32];
    int intensity;
    int bytes, ret, max_br;

    max_br = get_max_brightness(); 

    if(max_br < 0){
        LOGE("fail to read max brightness\n");
        return -1;
    }

    bright_to_intensity(max_br, brightness, intensity);

    bytes = snprintf(buff, sizeof(buff), "%d\n", intensity);
    if (bytes < 0)
	    return bytes;

    ret = write(fd, buff, bytes);
    if (ret < 0) {
        LOGE("faild to write %d (fd = %d, errno = %d)\n",
             intensity, fd, errno);
        return -errno;
    }

    return 0;
}

static inline int __is_on(const struct light_state_t *state)
{
    return state->color & 0x00ffffff;
}

static inline unsigned char
__rgb_to_brightness(const struct light_state_t *state)
{
    int color = state->color & 0x00ffffff;
    unsigned char brightness = ((77 * ((color >> 16) & 0x00ff))
                                + (150 * ((color >> 8) & 0x00ff))
                                + (29 * (color & 0x00ff))) >> 8;
    return brightness;
}

static int
set_light_backlight(struct light_device_t *dev,
                    const struct light_state_t *state)
{
    int brightness = __rgb_to_brightness(state);

    return write_brightness(context->fds.backlight, brightness);
}

static int set_light_keyboard(struct light_device_t *dev,
                              const struct light_state_t *state)
{
    int on = __is_on(state);

    return write_brightness(context->fds.keyboard,
                            on ? LIGHT_LED_FULL : LIGHT_LED_OFF);
}

static int set_light_buttons(struct light_device_t *dev,
                             const struct light_state_t *state)
{
    int on = __is_on(state);

#ifdef LIGHT_BUTTONS_AUTO_POWEROFF
    if (!pthread_mutex_lock(&context->button_info->lock)) {
	    context->button_info->brightness = on ? LIGHT_LED_FULL : LIGHT_LED_OFF;
	    context->button_info->need_update = 1;
	    if (pthread_cond_signal(&context->button_info->cond))
		    LOGE("Error: <%s>: pthread_cond_signal\n", __func__);
	    if (pthread_mutex_unlock(&context->button_info->lock)) {
		    LOGE("Error: <%s>: pthread_mutex_unlock\n", __func__);
		    return -1;
	    }
    } else {
	    LOGE("Error: <%s>: pthread_mutex_lock\n", __func__);
	    return -1;
    }

    return 0;
#else
    return write_brightness(context->fds.buttons,
			on ? LIGHT_LED_FULL : LIGHT_LED_OFF);
#endif
}

static int set_light_battery(struct light_device_t *dev,
                             const struct light_state_t *state)
{
    int on = __is_on(state);

    return write_brightness(context->fds.battery,
                            on ? LIGHT_LED_FULL : LIGHT_LED_OFF);
}

static int set_light_notifications(struct light_device_t *dev,
                                   const struct light_state_t *state)
{
    int on = __is_on(state);

    return write_brightness(context->fds.notifications,
                            on ? LIGHT_LED_FULL : LIGHT_LED_OFF);
}

static int set_light_attention(struct light_device_t *dev,
                               const struct light_state_t *state)
{
    int on = __is_on(state);

    return write_brightness(context->fds.attention,
                            on ? LIGHT_LED_FULL : LIGHT_LED_OFF);
}

/* lights close method */
static int close_lights_dev(struct light_device_t *dev)
{
    if (dev)
        free(dev);

    return 0;
}

static void *lights_events_thread(void *arg)
{
	struct light_info *info = arg;
	int i, j;
	fd_set rfds;
	struct input_event event;
	int need_wake;
	int ret;
	int max_fd;

	FD_ZERO(&rfds);
	for (;;) {
		need_wake = 0;
		for (i = 0; i < WAKE_EVENT_MAX && info->events[i].file; i ++)
			FD_SET(info->events[i].fd, &rfds);

		if (i == 0)
			return NULL;

		max_fd = info->events[i-1].fd;

		ret = select(max_fd + 1, &rfds, NULL, NULL, NULL);
		if (ret < 0) {
			if (ret == -EINTR)
				continue;
			LOGE("<%s>: fatal bug, select file error\n", info->name);
			return NULL;
		}
		for (i = 0; i < WAKE_EVENT_MAX && info->events[i].file; i ++) {
			if (!FD_ISSET(info->events[i].fd, &rfds))
				continue;
			while (read(info->events[i].fd, &event, sizeof(event)) == sizeof(event)) {
				if (need_wake)
					continue;
				if(event.type == info->events[i].type) {
					if (event.type == EV_ABS)
						need_wake = 1;
					else if (event.type == EV_KEY) {
						for (j = 0; j < WAKE_KEY_MAX && info->events[i].key[j] != -1; j ++) {
							if (info->events[i].key[j] == KEY_ANY
									|| info->events[i].key[j] == event.code) {
								LOGD("<%s>: EV_KEY wake up\n", info->name);
								need_wake = 1;
								break;
							}
						}
					}
				}
			}
		}
		if (need_wake) {
			if (!pthread_mutex_lock(&info->lock)) {
				info->need_update = 1;
				if (pthread_cond_signal(&info->cond))
					LOGE("Error: <%s>: pthread_cond_signal\n", __func__);
				if (pthread_mutex_unlock(&info->lock)) {
					LOGE("Error: <%s>: pthread_mutex_unlock\n", __func__);
					return NULL;
				}
			} else {
				LOGE("Error: <%s>: pthread_mutex_lock\n", __func__);
				return NULL;
			}
		}
	}

	return NULL;
}

static void *lights_update_thread(void *arg)
{
	struct light_info *info = arg;
	struct timespec ts;
	pthread_t tid;

	/*set brightness to default*/
	write_brightness(info->fd, info->brightness);
	info->brightness_status = info->brightness;
	if (info->brightness != LIGHT_LED_OFF)
		info->need_auto_off = 1;
	pthread_create(&tid, NULL, lights_events_thread, info);

	for (;;) {
		/* wait for update request */
		if (!pthread_mutex_lock(&info->lock)) {
			if (info->need_update) {
				LOGE("<%s>: update to %d\n", info->name, info->brightness);
				info->need_update = 0;
				if (info->brightness_status != info->brightness) {
					info->brightness_status = info->brightness;
					write_brightness(info->fd, info->brightness);
				}
			} else {
				LOGE("<%s>: auto off\n", info->name);
				if (info->brightness_status != LIGHT_LED_OFF) {
					info->brightness_status = LIGHT_LED_OFF;
					write_brightness(info->fd, LIGHT_LED_OFF);
				}
			}
			if (info->brightness_status == LIGHT_LED_OFF) {
				LOGE("<%s>: wait update\n", info->name);
				if (pthread_cond_wait(&info->cond, &info->lock))
					LOGE("Error: <%s>: pthread_cond_wait\n", __func__);
			} else {
				LOGE("<%s>: wait auto off\n", info->name);
				clock_gettime(CLOCK_REALTIME, &ts);
				ts.tv_sec += info->auto_off_time;
				if (pthread_cond_timedwait(&info->cond, &info->lock, &ts))
					LOGE("Error: <%s>: pthread_cond_timedwait\n", __func__);
			}
			if (pthread_mutex_unlock(&info->lock)) {
				LOGE("Error: <%s>: pthread_mutex_unlock\n", __func__);
				return NULL;
			}
		} else {
			LOGE("Error: <%s>: pthread_mutex_lock\n", __func__);
			return NULL;
		}
	}

	return NULL;
}

static void lights_init_info(struct light_info *info, int fd)
{
    int i;
    pthread_t tid;

    if ((info == NULL) || (fd < 0))
	    return;
    info->fd = fd;
    for (i = 0; i < WAKE_EVENT_MAX && info->events[i].file; i++) {
	    info->events[i].fd = open(info->events[i].file, O_RDONLY|O_NONBLOCK);
	    if (info->events[i].fd < 0) {
		    LOGE("<%s>: open %s failed\n", info->name, info->events[i].file);
	    } else {
		    LOGD("<%s>: open %s success\n", info->name, info->events[i].file);
	    }
    }
    if (pthread_mutex_init(&info->lock, NULL))
	    return;
    if (pthread_cond_init(&info->cond, NULL))
	    return;
    info->brightness = LIGHT_LED_OFF;
    pthread_create(&tid, NULL, lights_update_thread, info);
}

static int lights_open_node(struct lights_ctx *ctx, struct light_device_t *dev,
                            const char *id)
{
    struct light_info *info = NULL;
    int *pfd;
    char *path;
    int (*set_light)(struct light_device_t* dev,
                     struct light_state_t const* state);

    if (!strcmp(LIGHT_ID_BACKLIGHT, id)) {
        path = LIGHT_ID_BACKLIGHT_PATH;
        pfd = &ctx->fds.backlight;
        set_light = set_light_backlight;
    }
    else if (!strcmp(LIGHT_ID_KEYBOARD, id)) {
        path = LIGHT_ID_KEYBOARD_PATH;
        pfd = &ctx->fds.keyboard;
        set_light = set_light_keyboard;
    }
    else if (!strcmp(LIGHT_ID_BUTTONS, id)) {
        path = LIGHT_ID_BUTTONS_PATH;
        pfd = &ctx->fds.buttons;
        set_light = set_light_buttons;
#ifdef LIGHT_BUTTONS_AUTO_POWEROFF
	info = ctx->button_info = &button_light_info;
#endif
    }
    else if (!strcmp(LIGHT_ID_BATTERY, id)) {
        path = LIGHT_ID_BATTERY_PATH;
        pfd = &ctx->fds.battery;
        set_light = set_light_battery;
    }
    else if (!strcmp(LIGHT_ID_NOTIFICATIONS, id)) {
        path = LIGHT_ID_NOTIFICATIONS_PATH;
        pfd = &ctx->fds.notifications;
        set_light = set_light_notifications;
    }
    else if (!strcmp(LIGHT_ID_ATTENTION, id)) {
        path = LIGHT_ID_ATTENTION_PATH;
        pfd = &ctx->fds.attention;
        set_light = set_light_attention;
    }
    else
        return -EINVAL;

    *pfd = open(path, O_RDWR);
    if (*pfd < 0) {
        LOGE("faild to open %s, ret = %d\n", path, errno);
        return -errno;
    }

    LOGD("opened %s, fd = %d\n", path, *pfd);

    dev->set_light = set_light;
    lights_init_info(info, *pfd);

    return 0;
}

static struct lights_ctx *lights_init_context(void)
{
    struct lights_ctx *ctx;

    ctx = malloc(sizeof(struct lights_ctx));
    if (!ctx)
	    return NULL;

    memset(ctx, 0, sizeof(*ctx));

    ctx->fds.backlight = -1;
    ctx->fds.keyboard = -1;
    ctx->fds.buttons = -1;
    ctx->fds.battery = -1;
    ctx->fds.notifications = -1;
    ctx->fds.attention = -1;

    return ctx;
}

/*
 * module open method
 *
 * LIGHT_ID_BACKLIGHT          "backlight"
 * LIGHT_ID_KEYBOARD           "keyboard"
 * LIGHT_ID_BUTTONS            "buttons"
 * LIGHT_ID_BATTERY            "battery"
 * LIGHT_ID_NOTIFICATIONS      "notifications"
 * LIGHT_ID_ATTENTION          "attention"
 */
static int open_lights(const struct hw_module_t *module, const char *id,
                       struct hw_device_t **device)
{
    struct light_device_t *dev;
    int ret;

    dev = malloc(sizeof(struct light_device_t));
    if (!dev)
	    return -ENOMEM;

    memset(dev, 0, sizeof(*dev));

    if (!context) {
        context = lights_init_context();
	if (!context) {
		free(dev);
		return -ENOMEM;
	}
    }

    ret = lights_open_node(context, dev, id);
    if (ret < 0) {
        free(dev);
        return ret;
    }

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t *)module;
    dev->common.close = (int (*)(struct hw_device_t* device))close_lights_dev;

    *device = (struct hw_device_t *)dev;
    return 0;
}

/* module method */
static struct hw_module_methods_t lights_module_methods = {
    .open =  open_lights,
};

/* lights module */
struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 0,
    .version_minor = 1,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "Moorestown CDK lights Module",
	.author = "The Android Open Source Project",
    .methods = &lights_module_methods,
};
