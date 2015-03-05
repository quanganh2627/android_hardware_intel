#include <input.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <macros.h>


#include <event_dev.h>

/*
#define UNIPOINT_DEBUG 0

#define DBUG_OUT(...) \
	if (UNIPOINT_DEBUG) \
		fprintf(stderr, __VA_ARGS__)
*/
static const unsigned int input_expected  = 11;

int event_dev_open (FILE **stream, const char *path)
{
    int error = 0;

    *stream = fopen(path, "r");
    if (*stream == NULL) {
        error = errno;
        perror("Could not open Unipoint as stream: ");
        goto out;
    }

    error = setvbuf(*stream, NULL, _IOFBF,
                    sizeof(struct input_event) * input_expected);
    if (error) {
        perror("Could not set Unipoint stream buffer size: ");
    }

out:
    return error;
}

void event_dev_close (FILE *stream)
{
    int error = 0;

    error = fclose(stream);
    if (error) {
        perror("Error closing Unipoint Stream: ");
    }

}

static inline int evs_to_out
(struct input_event * restrict ev, unsigned int * const restrict out)
{
    unsigned int i;
    int error, recv_mask = 0, index = 0;
    /*
        int rx, ry, ft, gt;

        i = input_expected;
        error = 0;
        while (i--) {
            printf("type %d code %d value %d\n", ev[i].type, ev[i].code, ev[i].value);
            if (ev[i].type == EV_ABS) {
                if (ev[i].code == MSC_GESTURE) {
                    rx = -1;
                    ry = -1;
                    ft = -1;
                    gt = ev[i].value;
                }
                if (ev[i].code == ABS_MISC && rx == -1)
                    rx = ev[i].value;
                else if (ev[i].code == ABS_MISC && ry == -1)
                    ry = ev[i].value;
                else if (ev[i].code == ABS_MISC && ft == -1)
                    ft = ev[i].value;
            }
        }
    */
    for (i=0; i<input_expected; i++)
        printf("ev[%d] type %d code %d value %d\n", i, ev[i].type, ev[i].code, ev[i].value);
    error = 0;
    if (ev[0].type == EV_ABS)
        for (i=0; i<input_expected-1; i++)
            out[i] = ev[i].value;
    else
        error = -1;

    return error;
}

int event_dev_get(FILE *input, unsigned int * restrict out)
{
    struct input_event ev[input_expected];
    unsigned int ev_read;

    ev_read = fread(ev, sizeof(struct input_event), input_expected,
                    input);

    if (ev_read < input_expected) {
        fprintf(stderr, "Error: tried to read %i input events from "
                "Unipoint input device and received %i\n",
                input_expected, ev_read);
        return -EIO;
    }

    ev_read  = evs_to_out(ev, out);
    if (ev_read)
        ev_read =  -EIO;

    fflush(input);

    return ev_read;
}

