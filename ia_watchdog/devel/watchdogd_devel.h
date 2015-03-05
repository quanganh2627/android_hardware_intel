
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

/* Filename:	watchdogd_devel.h */

#ifndef _WATCHDOGD_DEVEL_H
#define _WATCHDOGD_DEVEL_H

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
 */
int watchdogd_register_client(char *id);

/* Function:	watchdogd_read_health_request
 *
 * Called by the Client to read the Health Request from the client socket.
 *
 * Parameters:
 * 	None
 *
 * Return:
 * 	1	Valid Health Request read from client socket
 * 	0	Timeout waiting for Health Request
 * 	-1	Timeout waiting for Health Request or invalid
 * 		health request from server
 */
int watchdogd_read_health_request();

/* Function:	watchdogd_health_response
 *
 * Called by the Client in response to a Watchdog Service Health Request,
 * if the Client believes that it is still healthy.
 *
 * Parameters:
 * 	None
 *
 * Return:
 * 	-1	Unable to send response
 * 	0	Response sent to watchdog daemon
 */
int watchdogd_health_response();

#endif /* _WATCHCLIENT_H */
