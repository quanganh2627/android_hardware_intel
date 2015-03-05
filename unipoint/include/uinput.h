/** \file uinput.h
 *  \brief Provides convenience functions for writing to the Linux
 *	uinput device.
 *  \author Venkatesh Srinivas
 *  \date 7/30/2010
 */
#ifndef SUINPUT_H
#define SUINPUT_H
#include <stdint.h>
#include <linux/uinput.h>

int uinput_write_event(int uinput_fd, const struct input_event *event);

int uinput_write(int uinput_fd, uint16_t ev_type, uint16_t ev_code,
                 int32_t ev_value);

int uinput_syn(int uinput_fd);

const char *uinput_get_uinput_path(void);

int uinput_open(void);

int uinput_create(int uinput_fd, const struct uinput_user_dev *user_dev);

int uinput_destroy(int uinput_fd);

int uinput_set_capabilities(int uinput_fd, uint16_t ev_type,
                            const int *ev_codes, size_t ev_codes_len);

int uinput_gen_setup(int *uinput_fd,FILE **sample_fd, char *op, char *fname);


#endif /* SUINPUT_H */


