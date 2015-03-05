/*************************************************************************
 **     Copyright (c) 2012 Intel Corporation. All rights reserved.      **
 **                                                                     **
 ** This Software is licensed pursuant to the terms of the INTEL        **
 ** MOBILE COMPUTING PLATFORM SOFTWARE LIMITED LICENSE AGREEMENT        **
 ** (OEM / IHV / ISV Distribution & End User)                           **
 **                                                                     **
 ** The above copyright notice and this permission notice shall be      **
 ** included in all copies or substantial portions of the Software.     **
 ** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,     **
 ** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF  **
 ** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND               **
 ** NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS **
 ** BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN  **
 ** ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN   **
 ** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE    **
 ** SOFTWARE.                                                           **
 **                                                                     **
 *************************************************************************/
#include <jni.h>

#include <android/log.h>
#define LOG_TAG "sepdrmJNI"
#include "cutils/log.h"
#include "com_intel_security_lib_sepdrmJNI.h"

/* todo: sepdrm.h (exported by libsepdrm) is not supported by all platforms.
 * As it is for testing purpose only, we simply skip it and may re-enable it later.
 */
//extern "C" {
//#include "sepdrm.h"
//}

namespace com_intel_security_lib {

	static jint libraryInit(JNIEnv *env, jclass clazz, jint priority) {

	uint32_t status = SERVICE_FAIL_UNSUPPORTED;

	LOGD("ENTER: %s\n",__func__);

	/* todo: Drm_Library_Init (in libsepdrm) is not supported by all platforms.
	 * As it is for testing purpose only, we simply skip it and may re-enable it later.
	 */
	LOGD("Drm_Library_Init not supported\n");
	status = SERVICE_SUCCESSFUL;
	/* Initialize the Driver*/
//	status = Drm_Library_Init();
//	if (DRM_SUCCESSFUL != status) {
//		LOGE( "DRM library initialization failed; Drm_Library_init() "
//				"returned error value of 0x%X.\n", status);
//
//		jclass clazz = env->FindClass("java/lang/UnsupportedOperationException");
//		if (clazz != NULL) {
//			env->ThrowNew(clazz,
//					"DRM library initialization failed; Drm_Library_init()");
//			env->DeleteLocalRef(clazz);
//		}
//
//		return -status;
//	}
	return status;
	}

	static jlong getRandomNumber(JNIEnv *env, jclass clazz, jint priority) {
	uint32_t randomValue = 0;
	uint32_t status = SERVICE_FAIL_UNSUPPORTED;

	LOGD("ENTER: %s\n",__func__);

	/* todo: Drm_GetRandom (in libsepdrm) is not supported by all platforms.
	 * As it is for testing purpose only, we simply skip it and may re-enable it later.
	 */
	LOGD("Drm_GetRandom not supported\n");
//	status = Drm_GetRandom((uint8_t*) &randomValue, sizeof(randomValue));
//	if (DRM_SUCCESSFUL != status) {
//		LOGE("Chaabi returned error value of 0x%X when reading "
//				"server clock value.\n", status);
//
//		return -status;
//	}

	__android_log_print(ANDROID_LOG_DEBUG, "app_sepdrmJNI",
			"EXIT: %s random=%ul 0x%08x\n", __func__, randomValue, randomValue);
	return randomValue;
	}

	static JNINativeMethod method_table[] = { { "libraryInit", "()I",
			(void *) libraryInit }, { "getRandomNumber", "()J",
					(void *) getRandomNumber }, };

}

using namespace com_intel_security_lib;

extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved) {
	JNIEnv* env;
	if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
		return -1;
	} else {
		jclass clazz = env->FindClass("com/intel/security/lib/sepdrmJNI");
		if (clazz) {
			env->RegisterNatives(clazz, method_table,
					sizeof(method_table) / sizeof(method_table[0]));
			env->DeleteLocalRef(clazz);
			return JNI_VERSION_1_6;
		} else {
			return -1;
		}
	}
}
