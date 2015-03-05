#ifndef OMX_SYSDEPS_H_
#define OMX_SYSDEPS_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#ifdef ANDROID

#include <utils/Log.h>

#ifdef ANDROID_ALOG
#define omx_errorLog(...) ALOGE(__VA_ARGS__);
#define omx_infoLog(...) ALOGI(__VA_ARGS__);
#define omx_verboseLog(...) ALOGV(__VA_ARGS__);
#define omx_warnLog(...) ALOGV(__VA_ARGS__);
#define omx_debugLog(...) ALOGV(__VA_ARGS__);
#elif ANDROID_LOG
#define omx_errorLog(...) LOGE(__VA_ARGS__);
#define omx_infoLog(...) LOGI(__VA_ARGS__);
#define omx_verboseLog(...) LOGV(__VA_ARGS__);
#define omx_warnLog(...) LOGV(__VA_ARGS__);
#define omx_debugLog(...) LOGV(__VA_ARGS__);
#endif
#endif //ANDROID

#ifndef omx_errorLog
#define omx_errorLog(format, ...) do { \
	fprintf(stderr, "omxil-core error: "format"\n", ##__VA_ARGS__); \
}while (0)
#endif

#ifndef omx_infoLog
#define omx_infoLog(format, ...) do { \
	fprintf(stderr, "omxil-core info: "format"\n", ##__VA_ARGS__); \
}while (0)
#endif

#ifdef __ENABLE_DEBUG__
#ifndef omx_verboseLog
#define omx_verboseLog(format, ...) do { \
	fprintf(stderr, "omxil-core verbose: "format"\n", ##__VA_ARGS__); \
}while (0)
#endif

#ifndef omx_warnLog
#define omx_warnLog(format, ...) do { \
	fprintf(stderr, "omxil-core warning: "format"\n", ##__VA_ARGS__); \
}while (0)
#endif

#ifndef omx_debugLog
#define omx_debugLog(format, ...) do { \
	fprintf(stderr, "omxil-core debug: "format"\n", ##__VA_ARGS__); \
}while (0)
#endif
#else //__ENABLE_DEBUG__

#ifndef omx_verboseLog
#define omx_verboseLog(format, ...)
#endif

#ifndef omx_warnLog
#define omx_warnLog(format, ...)
#endif

#ifndef omx_debugLog
#define omx_debugLog(format, ...)
#endif

#endif //__ENABLE_DEBUG__

#endif//OMX_SYSDEPS_H_

