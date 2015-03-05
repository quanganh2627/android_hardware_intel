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
#include <time.h>

#define GUID_BUF_LENGTH	(11)

const GUID nfc_block_guid = {0x0bb17a78, 0x2a8e, 0x4c50, {0x94, 0xd4, 0x50, 0x26, 0x67, 0x23, 0x77, 0x5c}};
#define MAX_DISCONNECT_TIMEOUT (256)

void print_usage(void)
{
	printf("Following arguments are needed\n");
	printf("	1. Name of guid file (text file)\n");
	printf("	2. Size of input message (hex number)\n");
	printf("	3. Size of output message (hex number)\n");
	printf("		Following files are not needed. If you do not\n");
	printf("		want one, mark it with - and it will not be opened\n");
	printf("		You must have a - or file name for each of the four\n");
	printf("		file parameters below.\n");
	printf("		Missing input files will be zero filled; missing output\n");
	printf("		files will have output printed to standard output\n");
	printf("		please note that all files are binary, not text\n");
	printf("		Please also note that we will try to read up to the\n");
	printf("		buffer size from the file. If there are less bytes in the\n");
	printf("		file, then the remainder of the buffer will be zeros.\n\n");
	printf("	4. Name of input message file (binary file) - means no file\n");
	printf("	5. Name of output message file (binary file) - means no file\n");
	printf("	6. Name of second output message file (binary file) - means no file\n");
	printf("	7. Timeout in seconds, prior to disconnect. - means no timeout.\n");
}

int main(int argc, char **argv)
{
	MEI_HANDLE *my_handle_p;
	ssize_t input_snd_count = 0;
	ssize_t input_rcv_count = 0;

	ssize_t file_ops_result = 0;
	
	int result;
	int ret_val = 0;

	int snd_count;
	int rcv_count;

	FILE *fp1;

	int fd_in = 0;
	int fd_out = 0;

	uint8_t *snd_msg_buf = NULL;
	uint8_t *rcv_msg_buf = NULL;

	unsigned int read_guid[GUID_BUF_LENGTH] = {0};

	GUID my_guid;
	/* By default disconnect timeout is set to 1 seconds */
	uint32_t disconnect_timeout = 1;

	if ((argc != 6) && (argc != 7) && (argc != 8)) {
		printf("Incorrected number of arguments %d\n", argc);
		print_usage();
		return -1;
	}

	printf("opening guid file%s\n", (char *)argv[1]);

	/* Grab the contents of the guid file */
	fp1 = fopen((char *)argv[1], "r");
	if (fp1 == NULL) {
		printf("cannot open guid file %s\n", (char *)argv[1]);
		print_usage();
		return -1;
	}

	/* To resolve stack corruption issue, using buffer
	that conforms to format in file. */
	fscanf(fp1, "%x %x %x %x %x %x %x %x %x %x %x\n",
		(unsigned int *)&read_guid[0],
		(unsigned int *)&read_guid[1],
		(unsigned int *)&read_guid[2],
		(unsigned int *)&read_guid[3],
		(unsigned int *)&read_guid[4],
		(unsigned int *)&read_guid[5],
		(unsigned int *)&read_guid[6],
		(unsigned int *)&read_guid[7],
		(unsigned int *)&read_guid[8],
		(unsigned int *)&read_guid[9],
		(unsigned int *)&read_guid[10]);

	my_guid.data1 = read_guid[0];
	my_guid.data2 = (unsigned short)read_guid[1];
	my_guid.data3 = (unsigned short)read_guid[2];
	my_guid.data4[0] = (unsigned char)read_guid[3];
	my_guid.data4[1] = (unsigned char)read_guid[4];
	my_guid.data4[2] = (unsigned char)read_guid[5];
	my_guid.data4[3] = (unsigned char)read_guid[6];
	my_guid.data4[4] = (unsigned char)read_guid[7];
	my_guid.data4[5] = (unsigned char)read_guid[8];
	my_guid.data4[6] = (unsigned char)read_guid[9];
	my_guid.data4[7] = (unsigned char)read_guid[10];

	printf("guid %08x %08x %08x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		(unsigned int)my_guid.data1,
		(unsigned short)my_guid.data2,
		(unsigned short)my_guid.data3,
		(unsigned char)my_guid.data4[0],
		(unsigned char)my_guid.data4[1],
		(unsigned char)my_guid.data4[2],
		(unsigned char)my_guid.data4[3],
		(unsigned char)my_guid.data4[4],
		(unsigned char)my_guid.data4[5],
		(unsigned char)my_guid.data4[6],
		(unsigned char)my_guid.data4[7]);
	fclose(fp1);

	/* Grab the input buffer size */
	result = sscanf(argv[2], "%x", (unsigned int *)&input_snd_count);
	if (result != 1) {
		printf("could not read snd count value\n");
		print_usage();
		return -1;
	}
	printf("Will be setting up send message buffer of %x bytes\n",
		(unsigned int)input_snd_count);

	/* Grab the output buffer size */
	result = sscanf(argv[3], "%x", (unsigned int *)&input_rcv_count);
	if (result != 1) {
		printf("could not read rcv count value\n");
		print_usage();
		return -1;
	}
	printf("Will be setting up receive message buffer of %x bytes\n",
		(unsigned int)input_snd_count);

	/* Now try to allocate the message buffers */
	if (input_snd_count) {
		snd_msg_buf = calloc(input_snd_count, 1);
		if (snd_msg_buf == NULL) {
			printf("cannot allocate send message buffer\n");
			print_usage();
			return -1;
		}
	}

	if (input_rcv_count) {
		rcv_msg_buf = calloc(input_rcv_count, 1);
		if (rcv_msg_buf == NULL) {
			printf("cannot allocate receive message buffer\n");
			print_usage();
	
			if (input_snd_count)
				free(snd_msg_buf);

			return -1;
		}
	}

	/**
	 * Now try to connect to the device using the guid
	 * that we got from the guid file
	 */
	
	my_handle_p = mei_connect(&my_guid);
	if (my_handle_p  == NULL) {
		printf("cannot connect to device\n");
		ret_val = -1;
		goto connect_failed;
	} else
		printf("Connect success\n");

	printf("After open/init\n");
	mei_print_buffer("fd", (uint8_t *)&my_handle_p->fd, sizeof(my_handle_p->fd));
	mei_print_buffer("guid", (uint8_t *)&my_handle_p->guid, sizeof(my_handle_p->guid));
	mei_print_buffer("client_properties", (uint8_t *)&my_handle_p->client_properties,
		sizeof(my_handle_p->client_properties));
	mei_print_buffer("mei_version", (uint8_t *)&my_handle_p->mei_version,
		sizeof(my_handle_p->mei_version));


	if (input_snd_count) {

		/* We're connected - now try to read the input buffer file 
		(if any) */
		if (strncmp((char *)argv[4], "-", strlen("-")) != 0) {
			printf("We have input message file %s\n", (char *)argv[6]);
			fd_in = open((char *)argv[4], O_RDONLY);
			if (fd_in <= 0) {
				printf("cannot open input message file\n");
				ret_val = -1;
				goto error_exit;
			}

		file_ops_result = read(fd_in, snd_msg_buf, input_snd_count);
		printf("we read %x bytes into input send buffer\n",
			(unsigned int)file_ops_result);
		close(fd_in);
		} else {
			printf("There is no input buffer file; buffer will be all zeros\n");
		}

		printf("Sending output buffer\n");
		mei_print_buffer("first 10 chrs of snd_msg_buf", snd_msg_buf, 10);
		
		while(1) {
			printf("keep sending the message\n");
			snd_count = mei_sndmsg(my_handle_p, snd_msg_buf, input_snd_count);
	
			if (snd_count != input_snd_count) {
				printf("incorrect size sent %x instead of %x\n", snd_count,
					(unsigned int)input_snd_count);
			}
		}
	
	}

	if (input_rcv_count) {
		
		rcv_count = mei_rcvmsg(my_handle_p, rcv_msg_buf, input_rcv_count);
		
		if (rcv_count != input_rcv_count) {
			printf("incorrect size received %x instead of %x\n", rcv_count,
				(unsigned int)input_rcv_count);
		}

		printf("Received input buffer\n");
		mei_print_buffer("first 10 chrs rcv_msg_buf", rcv_msg_buf, 10);


		/* Now copy the rcv_msg_buf to a file if a file was  provided */
		if (strncmp((char *)argv[5], "-", strlen("-")) != 0) {
			printf("We have output message file %s\n", (char *)argv[5]);
			fd_out = open((char *)argv[5], O_WRONLY| O_CREAT | O_TRUNC,
			S_IRWXU | S_IRWXG);
			if (fd_out <= 0) {
				printf("cannot open output message file\n");
				ret_val = -1;
				goto error_exit;
			}

			file_ops_result = write(fd_out, rcv_msg_buf, rcv_count);
			printf("we wrote %x bytes out of output rcv buffer\n",
			(unsigned int)rcv_count);
			close(fd_out);
		} else {
			printf("There is no output buffer file\n");
			mei_print_buffer("entire rcv_msg_buf", rcv_msg_buf,
			input_rcv_count);
		}

		if (0 == memcmp(&nfc_block_guid, &my_guid, sizeof(GUID))) {
			if (snd_msg_buf[2] || snd_msg_buf[3]) {
				printf("Receiving 2nd response...\n");
				rcv_count = mei_rcvmsg(my_handle_p, rcv_msg_buf, input_rcv_count);

				if (strncmp((char *)argv[6], "-", strlen("-")) != 0) {
					fd_out = open(argv[6], O_WRONLY| O_CREAT | O_TRUNC,
					S_IRWXU | S_IRWXG);
					if (fd_out <= 0) {
						printf("cannot open output 1 message file\n");
						ret_val = -1;
						goto error_exit;
					}
					file_ops_result = write(fd_out, rcv_msg_buf, rcv_count);
					printf("we wrote %x bytes out of output rcv buffer\n",
					(unsigned int)rcv_count);
					close(fd_out);
				} else {
					printf("There is no output 1 buffer file\n");
					mei_print_buffer("2nd response", rcv_msg_buf, rcv_count);
				}
			}
		}

	}

	if (argc == 8) {

		if (strncmp((char *)argv[7], "-", strlen("-")) != 0) {

			result = sscanf(argv[7], "%d", (unsigned int *)&disconnect_timeout);
			if (result != 1) {
				printf("could not get timeout value.\n");
				print_usage();
				return -1;
			}
			if (disconnect_timeout > MAX_DISCONNECT_TIMEOUT) {
				printf("Timeout -> %d, cannot be more than %d seconds.\n",
				disconnect_timeout, MAX_DISCONNECT_TIMEOUT);
				print_usage();
				return -1;
			} else if (disconnect_timeout == 0) {
				/* Setting it to default value 1, in this case */
				disconnect_timeout = 1;
			}

			printf("Set timeout to %d\n", disconnect_timeout);
		}

	}

error_exit:

	if (!input_snd_count && !input_rcv_count) {
		/* There is a timing issue in the driver, for a simple,
		connect followed by a disconnect.
		NOTE: Linux file system, calls close on the file descriptor,
		so mei driver, still ends up disconnecting.
		This is a temp fix. This is not a common use-case.
		Other option is to fix in driver.*/
		sleep(disconnect_timeout);
	} else {
		/* We are done. Now we only process output. Close device */
		mei_disconnect(my_handle_p);
		printf("mei_disconnect: Dis-connect done.\n");
	}

connect_failed:

if (input_snd_count)
	free(snd_msg_buf);

if (input_rcv_count)
	free(rcv_msg_buf);

if (ret_val)
	print_usage();

	return ret_val;

}
