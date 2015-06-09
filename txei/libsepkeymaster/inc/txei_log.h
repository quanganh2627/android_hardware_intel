#ifndef __TXEI_LOG_H__
#define __TXEI_LOG_H__

#include <stdio.h>

/** Represents where logging output should be directed */
typedef enum {
	TXEI_LOG_DEST_ANDROID,	/*!< Redirect messages to the Android logging daemon */
	TXEI_LOG_DEST_PRINTF,	/*!< Print messages to stdout / stderr */
	TXEI_LOG_DEST_FILE	/*!< Print messages to specific file handles */
} TXEI_LOG_DEST;

/** Represents what urgency is associated with a log message */
typedef enum {
	TXEI_LOG_LEVEL_DBG,	/*!< Low priority message, useful for debugging */
	TXEI_LOG_LEVEL_INFO,	/*!< Informational message a user might care about */
	TXEI_LOG_LEVEL_ERR,	/*!< Error message that indicates a problem */
	TXEI_LOG_LEVEL_NONE	/*!< When used as the minimum logging level, no messages are printed regardless of severity */
} TXEI_LOG_LEVEL;


/*
 * Before including this header, you should define LOG_TAG to be something reasonable, like:
 * #define LOG_TAG "libtxei-log". If you don't set it, it will be set to "txei-generic"
 */
#ifndef LOG_TAG
#define LOG_TAG "libtxei-generic"
#endif

#define Enter LOGDBG

/** Print a debug message with printf-style syntax. Level = TXEI_LOG_LEVEL_DBG */
#define LOGDBG(fmt, arg...) \
	txei_log(TXEI_LOG_LEVEL_DBG, LOG_TAG, "DEBUG [%s():%d] - " fmt, __func__, __LINE__, ##arg)

/** Print an info message with printf-style syntax. Level = TXEI_LOG_LEVEL_INFO */
#define LOGINFO(fmt, arg...) \
	txei_log(TXEI_LOG_LEVEL_INFO, LOG_TAG, "INFO [%s():%d] - " fmt, __func__, __LINE__, ##arg)

/** Print an error message with printf-style syntax. Level = TXEI_LOG_LEVEL_ERROR */
#define LOGERR(fmt, arg...) \
	txei_log(TXEI_LOG_LEVEL_ERR, LOG_TAG, "ERROR [%s():%d] - " fmt, __func__, __LINE__, ##arg)

/** Print a debug buffer in hex, with an identifying label. Level = TXEI_LOG_LEVEL_DBG */
#define LOGDBGBUF(label, buf, len) \
	txei_log_buf(TXEI_LOG_LEVEL_DBG, LOG_TAG, "DEBUG BUFFER [%s():%d] - " label, __func__, __LINE__, (unsigned char*)buf, len)

/** Print an info buffer in hex, with an identifying label. Level = TXEI_LOG_LEVEL_INFO */
#define LOGINFOBUF(label, buf, len) \
	txei_log_buf(TXEI_LOG_LEVEL_INFO, LOG_TAG, "INFO BUFFER [%s():%d] - " label, __func__, __LINE__, (unsigned char*)buf, len)

/** Print an error buffer in hex, with an identifying label. Level = TXEI_LOG_LEVEL_ERR */
#define LOGERRBUF(label, buf, len) \
		txei_log_buf(TXEI_LOG_LEVEL_ERR, LOG_TAG, "ERROR BUFFER [%s():%d] - " label, __func__, __LINE__, (unsigned char*)buf, len)

/**
 * Set the destination for log messages.
 * @param[in] dest
 * 		TXEI_LOG_DEST log destination. Default is TXEI_LOG_DEST_PRINTF.
 * @param[in] out_fp
 * 		File descriptor to print non-error messages to.
 * 		Only used for dest == TXEI_LOG_DEST_FILE. User must open this file
 * 		prior to calling this function, and close it at program termination.
 * @param[in] err_fp
 * 		File descriptor to print error messages to. Can be same as out_fp.
 * 		Only used for dest == TXEI_LOG_DEST_FILE. User must open this file
 * 		prior to calling this function, and close it at program termination.
 */
void txei_log_set_dest(TXEI_LOG_DEST dest, FILE * out_fp, FILE * err_fp);

/**
 * Set the minimum level for log messages to actually be printed.
 * @param level
 * 		Minimum required level. Anything of lower priority will be discarded.
 */
void txei_log_set_level(TXEI_LOG_LEVEL level);

/**
 * This is the function is invoked internally by the LOGDBG, LOGINFO, and LOGERR macros.
 * Use those instead of invoking this directly.
 */
void txei_log(int level, const char *tag, const char *format, ...);

/**
 * This is the function is invoked internally by the LOGDBGBUF, LOGINFOBUF, and LOGERRBUF macros.
 * Use those instead of invoking this directly.
 */
void txei_log_buf(int level, const char *tag, const char *label,
		  const char *func, int line, unsigned char *buf, int len);


#endif				/* __TXEI_LOG_H__ */
