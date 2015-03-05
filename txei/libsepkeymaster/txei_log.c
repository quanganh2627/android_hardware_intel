#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef ANDROID
#include <android/log.h>
#endif

#define LOG_TAG      "libtxei-log"
#include "txei_log.h"

static int txei_log_level = TXEI_LOG_LEVEL_DBG;		/*!< Current minimum logging level to print */
static int txei_log_dest = TXEI_LOG_DEST_PRINTF;	/*!< Current logging output location */
static FILE* txei_log_out_fp = stdout;				/*!< File descriptor to print non-error messages */
static FILE* txei_log_err_fp = stderr;				/*!< File descriptor to print error messages */

void txei_log_set_dest(TXEI_LOG_DEST dest, FILE *out_fp, FILE *err_fp) {
    switch (dest) {
    case TXEI_LOG_DEST_ANDROID:
#ifdef ANDROID
        txei_log_dest = TXEI_LOG_DEST_ANDROID;
        LOGDBG("Logging switched to Android logger");
#else
        LOGERR("Library not compiled with Android logging support");
#endif
        break;
    case TXEI_LOG_DEST_PRINTF:
        txei_log_dest = TXEI_LOG_DEST_PRINTF;
        txei_log_out_fp = stdout;
        txei_log_err_fp = stderr;
        LOGDBG("Logging switched to printing to stdout, stderr");
        break;
    case TXEI_LOG_DEST_FILE:
        if (out_fp == NULL || err_fp == NULL) {
            LOGERR("Logging not switched to file because specified file descriptor is NULL");
        } else {
            txei_log_dest = TXEI_LOG_DEST_FILE;
            txei_log_out_fp = out_fp;
            txei_log_err_fp = err_fp;
            LOGDBG("Logging switched to printing to specified file descriptors");
        }
        break;
    default:
        LOGERR("Unknown logging mechanism %d", dest);
        break;
    }
}

void txei_log_set_level(TXEI_LOG_LEVEL level) {
    switch(level) {
    case TXEI_LOG_LEVEL_DBG:
    case TXEI_LOG_LEVEL_INFO:
    case TXEI_LOG_LEVEL_ERR:
    case TXEI_LOG_LEVEL_NONE:
        txei_log_level = level;
        break;
    default:
        LOGERR("Unknown logging level %d", level);
        break;
    }
}

/**
 * This function properly terminates a string with exactly one newline and null character.
 * Useful in this logging code to sanitize user input
 * @param[in] buffer			Character buffer containing string to sanitize
 * @param[in] buffer_len		Max size of the string buffer
 * @param[in] total_written
 * 		Number of bytes already written into the buffer. Function loops back starting from
 * 		this point, looking for the last non null & non newline character. If unknown,
 * 		use the size of the buffer.
 * @return
 * 		The number of valid characters in the string, including the newline but not the
 * 		null character terminator.
 */
static __inline int newline_terminate(char *buffer, int buffer_len, int total_written) {
	int i;

    /* Every print should end in a newline, but not more than one */
    /* First, figure out the last valid character location */
    if (total_written > (int) buffer_len - 3) {
        i = buffer_len - 3;
    } else {
        i = total_written;
    }
    /* Loop backwards to find the last non null & non return character */
    for (; i >= 0; i --) {
        if (buffer[i] != '\0' && buffer[i] != '\n') {
            /* Replace the two characters after the last character with newline and null characters */
            buffer[i + 1] = '\n';
            buffer[i + 2] = '\0';
            break;
        }
    }

    /* Return the length of the string in the buffer, not counting the trailing \0 */
    return i + 2;
}

void txei_log(int level, const char* tag, const char* format, ...) {
    char buffer[1024] = "";
    int total_written = 0, last_written, i;
    va_list args;
    FILE* dest_fp;

#ifdef ANDROID
    int android_log_level;
#endif

	if (format == NULL) {
		LOGERR("Format string to be printed is NULL");
		return;
	}
	if (tag == NULL) {
		LOGERR("Tag to be printed is NULL");
		return;
	}

    if (txei_log_level <= level) {
        /* Fill the buffer with null characters */
        memset(buffer, '\0', sizeof(buffer));

        /* Write the tag into the buffer first, if we're not printing to the android logger */
        if (txei_log_dest != TXEI_LOG_DEST_ANDROID) {
            last_written = snprintf(&buffer[total_written], sizeof(buffer) - total_written, "%s - ", tag);
            if (last_written < 0) return; /*!< If something went horribly wrong, abort this log message */

            total_written += last_written;
            if (total_written >= (int) sizeof(buffer)) {
                /* Message too long for buffer, add a note and print whatever fits */
                snprintf(&buffer[sizeof(buffer) - 25], 25, "...(message too long)\n");
                goto print_message;
            }
        }

        /* Write the actual message */
        va_start(args, format);
        last_written = vsnprintf(&buffer[total_written], sizeof(buffer) - total_written, format, args);
        va_end(args);

        if (last_written < 0) return; /*!< If something went horribly wrong, abort this log message */

        total_written += last_written;
        if (total_written >= (int) sizeof(buffer)) {
            /* Message too long for buffer, add a note and print whatever fits */
            snprintf(&buffer[sizeof(buffer) - 25], 25, "...(message too long)\n");
            goto print_message;
        }

		/* Make sure message ends with only one newline */
        total_written = newline_terminate(buffer, sizeof(buffer), total_written);
        goto print_message;
    }

    /* Didn't fall into the print message handler, abort function */
    return;

print_message:
    /* Need to print the message to the desired output */
    if (txei_log_dest == TXEI_LOG_DEST_ANDROID) {
#ifdef ANDROID
        /* Map TXEI log levels to Android log levels */
        switch (txei_log_level) {
        case TXEI_LOG_LEVEL_ERR:
            android_log_level = ANDROID_LOG_ERROR;
            break;
        case TXEI_LOG_LEVEL_INFO:
            android_log_level = ANDROID_LOG_INFO;
            break;
        case TXEI_LOG_LEVEL_DBG:
        default:
            android_log_level = ANDROID_LOG_DEBUG;
            break;
        }
        __android_log_print(android_log_level, tag, "%s", buffer);
#endif
    } else {
        /* Choose appropriate file descriptor to write to */
        if (txei_log_level < TXEI_LOG_LEVEL_ERR) {
            dest_fp = txei_log_out_fp;
        } else {
            dest_fp = txei_log_err_fp;
        }

        fprintf(dest_fp, "%s", buffer);
    }
}

void txei_log_buf(int level, const char* tag, const char* label, const char* func, int line, unsigned char *buf, int len) {
    char buffer[1024] = "";
    int total_written = 0, last_written, i;
    va_list args;
    FILE* dest_fp;

#ifdef ANDROID
    int android_log_level;
#endif

	if (label == NULL) {
		LOGERR("Label to be printed is NULL");
		return;
	}
	if (tag == NULL) {
		LOGERR("Tag to be printed is NULL");
		return;
	}
	if (buf == NULL) {
		LOGERR("Buffer to be printed is NULL");
		return;
	}

    if (txei_log_level <= level) {
        /* Fill the buffer with null characters */
        memset(buffer, '\0', sizeof(buffer));

        /* Write the tag into the buffer first, if we're not printing to the android logger */
        if (txei_log_dest != TXEI_LOG_DEST_ANDROID) {
            last_written = snprintf(&buffer[total_written], sizeof(buffer) - total_written, "%s - ", tag);
            if (last_written < 0) return; /*!< If something went horribly wrong, abort this log message */

            total_written += last_written;
            if (total_written >= (int) sizeof(buffer)) {
                /* Message too long for buffer, add a note and print whatever fits */
                snprintf(&buffer[sizeof(buffer) - 25], 25, "...(message too long)\n");
                goto print_message;
            }
        }

        /* Write the label */
        last_written = snprintf(&buffer[total_written], sizeof(buffer) - total_written, label, func, line);
        if (last_written < 0) return; /*!< If something went horribly wrong, abort this log message */

        total_written += last_written;
        if (total_written >= (int) sizeof(buffer)) {
            /* Message too long for buffer, add a note and print whatever fits */
            snprintf(&buffer[sizeof(buffer) - 25], 25, "...(message too long)\n");
            goto print_message;
        }

        /* Make sure label line ends in a newline */
        total_written = newline_terminate(buffer, sizeof(buffer), total_written);

        /* Write the buffer in hex */
		for (i = 0; i < len; i += 1) {
	        last_written = snprintf(&buffer[total_written], sizeof(buffer) - total_written, "%02X", *(buf + i));
	        if (last_written < 0) return; /*!< If something went horribly wrong, abort this log message */

	        total_written += last_written;
	        if (total_written >= (int) sizeof(buffer)) {
	            /* Message too long for buffer, add a note and print whatever fits */
	            snprintf(&buffer[sizeof(buffer) - 25], 25, "...(message too long)\n");
	            goto print_message;
	        }

			if ((i % 8) == 7) {
		        last_written = snprintf(&buffer[total_written], sizeof(buffer) - total_written, "\n");
		        if (last_written < 0) return; /*!< If something went horribly wrong, abort this log message */

		        total_written += last_written;
		        if (total_written >= (int) sizeof(buffer)) {
		            /* Message too long for buffer, add a note and print whatever fits */
		            snprintf(&buffer[sizeof(buffer) - 25], 25, "...(message too long)\n");
		            goto print_message;
		        }
			}
		}

		/* Make sure message ends with only one newline */
        total_written = newline_terminate(buffer, sizeof(buffer), total_written);
        goto print_message;
    }

    /* Didn't fall into the print message handler, abort function */
    return;

print_message:
    /* Need to print the message to the desired output */
    if (txei_log_dest == TXEI_LOG_DEST_ANDROID) {
#ifdef ANDROID
        /* Map TXEI log levels to Android log levels */
        switch (txei_log_level) {
        case TXEI_LOG_LEVEL_ERR:
            android_log_level = ANDROID_LOG_ERROR;
            break;
        case TXEI_LOG_LEVEL_INFO:
            android_log_level = ANDROID_LOG_INFO;
            break;
        case TXEI_LOG_LEVEL_DBG:
        default:
            android_log_level = ANDROID_LOG_DEBUG;
            break;
        }
        __android_log_print(android_log_level, tag, "%s", buffer);
#endif
    } else {
        /* Choose appropriate file descriptor to write to */
        if (txei_log_level < TXEI_LOG_LEVEL_ERR) {
            dest_fp = txei_log_out_fp;
        } else {
            dest_fp = txei_log_err_fp;
        }

        fprintf(dest_fp, "%s", buffer);
    }
}

