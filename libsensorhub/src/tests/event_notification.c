#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <cutils/log.h>

#include "../include/libsensorhub.h"

static void dump_event_data(int fd, short event_id)
{
	char buf[512];
	int size = 0, i = 0, count = 0;

	while (((size = read(fd, buf, 512)) > 0) && (count < 10)) {
		unsigned char p;
		i = 0;
		while (i < size) {
			p = buf[i];
			if (p == event_id) {
				printf("event notification: matching event id \n");
			} else {
				printf("event notification: NOT matching event id \n");
			}
			i++;
		}
		printf("notification: count is %d\n", count);
		count++;
	}
}

// Expect evt_num relation [[sensor_id chan_id opt param1 param2] ...]
int parse_subevt(char* arg, struct sub_event* evts, int max_evt, char* rel)
{
	int evt_num;
	char *ptr;
	int i;

	printf ("ARGS:%s\n", arg);

	evt_num = strtol(arg, &ptr, 10);
	#define NEXT strtol(ptr + 1, &ptr, 10)
	*rel = NEXT;

	printf ("evt_num:%d rel:%d\n", evt_num, *rel);

	for (i = 0; i < evt_num && i < max_evt; ++i){
		struct sub_event * e;
		e = evts + i;

		e->sensor_id = NEXT;
		e->chan_id = NEXT;
		e->opt_id = NEXT;
		e->param1 = NEXT;
		e->param2 = NEXT;
		printf ("[%d]sensor_id:%d, chan_id:%d, opt_id:%d "
				"param1:%d, param2:%d\n",
			i, e->sensor_id, e->chan_id, e->opt_id,
			e->param1, e->param2);

	}
	#undef NEXT

	return i;
}

int main(int argc, char **argv)
{
	handle_t handle;
	error_t ret;
	char buf[512];
	struct sub_event sub_evt[6]; /* For event number limit test. */
	int evt_num, i;
	char rel;
	short event_id;
	int fd;

	if (argc != 2){
		printf ("Usage: event_notification parameter\n");
		printf ("\t [Note, only one parameter(a big string)]\n");
		return 0;
	}

	evt_num = parse_subevt(argv[1], sub_evt, sizeof(sub_evt), &rel);

	if (evt_num <= 0){
		printf ("Failed ot parse parameter:%s\n", argv[1]);
		return -1;
	}

#undef LOG_TAG
#define LOG_TAG "event_notification_test"
	LOGD("event notification test started \n");

	handle = psh_open_session(SENSOR_EVENT);

	if (handle == NULL) {
		printf("psh_open_session() returned NULL handle. \n");
		return -1;
	}

	psh_event_set_relation(handle, rel);

	for (i = 0; i < evt_num; ++i)
	{
		error_t t = psh_event_append(handle, sub_evt + i);
		if (t != ERROR_NONE){
			printf ("----- error:%d\n", t);
			return -1;
		}
	}

	ret = psh_add_event(handle);

	if (ret != ERROR_NONE) {
		printf("ret of psh_add_event() is %d \n", ret);
		return -1;
	}

	event_id = psh_get_event_id(handle);
	fd = psh_get_fd(handle);
	printf("event id is %d, fd is %d \n", event_id, fd);

	dump_event_data(fd, event_id);

	psh_clear_event(handle);

//	sleep(200);

	psh_close_session(handle);

	return 0;
}
