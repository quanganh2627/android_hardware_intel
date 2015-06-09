
/**
 * Copyright 2009 - 2010 (c) Intel Corporation. All rights reserved.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Filename:	watchdogd_devel.c */
#if 0

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <android/log.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "watchdogd_devel.h"
#include "watchdogd.h"

#define LOG_TAG "ia_watchdog_client"

char	client_sid[CLIENT_ID_SIZE] = "";
int	client_fd = 0;
char	client_uid = 0;
int	registered = 0;

/* Function:   watchdogd_register_client
 *
 * Registers the calling process as a Client of the Watchdog Service.
 * Once registered the Client process is responsible for responding
 * to Watchdog Service Health Requests, in a timely manner.
 * Failure to respond to a Watchdog Service Health Request, in a timely
 * manner, will result in the system being reset.
 *
 * Parameters:
 *	id	Pointer to character string with human readable ID string
 *		for this Client. Maximum size of ID sting is CLIENT_ID_SIZE.
 *
 * Return:
 *	File Descriptor of Client's watchdog request socket.  The Watchdog
 *	Service sends Health Requests to this socket. The Client is
 *	responsible for detecting these requests and responding to them,
 *	by calling watchdogd_health_response.
 *
 * Error:
 * 	If an error occurs while connecting to the service, a -1 will
 * 	be returned and errno will be set.
 */
int watchdogd_register_client(char *id)
{
	int			conn_ret;
	struct sockaddr_un	serv_addr;
	int			servlen;
	char			buf[80];
	ssize_t			rsize;

	if (id == NULL) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
			"Invalid NULL string for client ID\n");
		return -EINVAL;
	}

	if (strlen(id) > CLIENT_ID_SIZE) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
			"Client ID string too big\n");
		return -EINVAL;
	}

	/* Create our client socket */
	if ((client_fd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
		/* Error creating socket, escalate error to caller */
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
			"Error Creating Client Socket");
		return client_fd;
	}

	/* Setup local address of the Watchdog Service Socket */
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sun_family = AF_LOCAL;
	strncpy(serv_addr.sun_path, WATCH_SOCKET, sizeof(serv_addr.sun_path)-1);
	servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);

	/* Connect to the Service's socket */
	conn_ret = connect(client_fd, (struct sockaddr *) &serv_addr, servlen);
	if (conn_ret < 0) {
		/* Error connecting to Service's socket */
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
			"Error Connecting to Service's Socket");
		return conn_ret;
	}
	/* Read new connection handshake message from Service */
	rsize = read(client_fd,buf,80);
	if ((rsize == 0) || (rsize == -1)) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
			"Error reading new message from Service");
		return -1;
	}

	if (buf[0] != ACK) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
			"Server did not return ACK message");
		errno = EBADMSG;
		return -1;
	}
	client_uid = buf[1];

	/* Save the ID string to be used by watchdogd_health_response */
	strncpy(client_sid, id, CLIENT_ID_SIZE);
	client_sid[CLIENT_ID_SIZE - 1] = 0;

	__android_log_print(ANDROID_LOG_INFO, LOG_TAG,
		"my register: id %s uid is %d\n, fd is %d",
		client_sid, client_uid, client_fd);

	registered = 1;
	return client_fd;
};

/* Function:    watchdogd_read_health_request
 *
 * Called by the Client to read the Health Request from the client socket.
 *
 * Parameters:
 * 	None
 *
 *	Return:
 * 		1       Valid Health Request read from client socket
 *		0	Timeout waiting for Health Request.
 *		-1      Error reading client socket or invalid
 * 			health request from server
 */
int watchdogd_read_health_request()
{

	fd_set		rx_set;
	struct timeval	tout;
	int		n;
	char		buf[80];

	if (!registered) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
			"Sorry, not registered, goodbye!!\n");
		return -1;
	}

	FD_ZERO(&rx_set);
	FD_SET(client_fd, &rx_set);
	tout.tv_sec = 0;
	tout.tv_usec = 1;
	n = select(client_fd+1, &rx_set, NULL, NULL, &tout);
	if (n == -1) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
			"Error Selecting client FD: %d\n",client_fd);
		return -1;
	} else if (!n) {
		return 0;
	}
	n = read(client_fd,buf,80);
	if (n == 0) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
			"Read zero bytes from client socket\n");
		errno = EBADMSG;
		return -1;
	} else if (n == -1) {
		return -1;
	}

	/* check the message */
	if (buf[0] == ENQUIRY)
		return 1;

	/* Unexpected message */
	errno = EBADMSG;
	return -1;
};


/* Function:	watchdogd_health_response
 *
 * Called by the Client in response to a Watchdog Service Health Request,
 * if the Client believes that it is still healthy.
 *
 * Parameters:
 * 	None
 *
 * Return:
 * 	0	Client health response sent to Watchdog Service
 * 	-1	Error sending client health response, errno set.
 */
int watchdogd_health_response()
{
	client_resp_t	h_resp;
	int	wsize;

	if (!registered) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
			"Sorry, not registered, goodbye!!\n");
		return -1;
	}

	/* Create Health Response Message */
	h_resp.c_uid = client_uid;
	strncpy(h_resp.c_sid, client_sid, CLIENT_ID_SIZE);
	wsize = write(client_fd, &h_resp, CLIENT_ID_SIZE+4);
	if ((wsize == 0) || (wsize == -1)) {
		/* Problem writing to the client's socket */
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
			"Error writing to client socket %s",
			strerror(errno));
		errno = ECOMM;
		return -1;
	}
	return 0;
};

#endif
