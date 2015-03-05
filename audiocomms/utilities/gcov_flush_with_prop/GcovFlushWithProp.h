/*
 * Copyright 2013 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/** @mainpage Force the gcov flush manually
 *
 * @section why Why?
 *
 * Normally to enable a gcov code coverage on a component we just need:
 * - Compile with flags <tt>O0 -fprofile-arcs -ftest-coverage</tt>
 * - Link with flags <tt>-fprofile-arcs -lgcov</tt>
 *
 * At the end of the component the coverage counter (.gcda) are written into
 * files by an automatic gcov flush.
 *
 * But for a system shared library, this is not enough. The automatic call of
 * the gcov flush is never done (like the destructor for a singleton) because
 * these libraries are never killed cleanly but kill by the OS.
 *
 * @section how How?
 *
 * @subsection install Installing lcov
 * If lcov is already installed on your system, please uninstall it, because
 * it’s not compatible with the new Android gcc.
 *
 * Then download lcov here: http://ltp.cvs.sourceforge.net/viewvc/ltp/utils/analysis/lcov/?view=tar&pathrev=MAIN
 *
 * Unpack the archive and install it:
 * @code make install PREFIX=/home/lab/tools/lcov @endcode
 * Then add @c /home/lab/tools/lcov/usr/bin to you @c $PATH variable in your
 * .bashrc file.
 *
 * @subsection before_compile Before compile your component
 *
 * - Link with this static library
 * - Force instantiation of this singleton by including this file at least one
 * instrumented source file.
 *
 * Example of Android.mk modification using this library and a compilation
 * switch:
 * @code
 * ifeq ($($(LOCAL_MODULE).gcov),true)
 *     LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/gcov_flush_with_prop
 *     LOCAL_CFLAGS += -O0 -fprofile-arcs -ftest-coverage -include GcovFlushWithProp.h
 *     LOCAL_LDFLAGS += -fprofile-arcs -lgcov
 *     LOCAL_STATIC_LIBRARIES += gcov_flush_with_prop
 * endif
 * @endcode
 *
 * @subsection before_test Before the test
 *
 * @subsubsection environment Check special environment variable are sets
 *
 * In a shell run:
 * @code adb shell "echo $GCOV_PREFIX; echo $GCOV_PREFIX_STRIP" @endcode
 * Check the output is:
 * @code /data
 * 1 @endcode
 *
 * @subsubsection thread Check gcov flush thread is started
 *
 * In a new shell run: @code logcat | grep GcovFlushWithProp @endcode
 * Restart your instrumented component, to reset RAM coverage
 * Example for some Audio-Comms components:
 * @code
 * adb shell start media
 * adb shell stop media
 * @endcode
 * Check in the adb logcat the line below is displayed:
 * @code GcovFlushWithProp wait property "gcov.flush.force" is "true" !!!
 * @endcode
 *
 * Clean previous coverage:
 * @code adb shell "find / -name '*.gcda' -exec rm {} \;" @endcode
 *
 * @subsection test Make the test
 *
 * The instrumented component must not be restart during all tests.
 * Because all coverage counters are in RAM memory with the data of the
 * component. If the component restarts you lose a part of your coverage test.
 *
 * @subsection after_test End of the test
 *
 * @subsubsection gcov_flush Force the gcov flush
 *
 * Force a gcov flush by setting the property @c gcov.flush.force to @c true
 * @code adb shell setprop gcov.flush.force true @endcode
 * Check in the adb logcat lines below are displayed:
 * @code
 * GcovFlushWithProp __gcov_flush begin !!!
 * GcovFlushWithProp __gcov_flush end !!!
 * @endcode
 *
 * @subsubsection gcov_copy Copy gcov counter
 *
 * Copy gcda file from mobile to Linux:
 * @code
 * adb shell "find /data -name '*.gcda'" | sed "s/\/data\([a-zA-Z0-9_\/\.\-]*\)/adb pull \/data\1 \/home\1/"
 * @endcode
 *
 * @subsubsection generation Generate coverage
 *
 * @code
 * TEST_COMPONENT_GCDA_DIR=$ANDROID_PRODUCT_OUT/obj/SHARED_LIBRARIES/<componentName>_intermediates
 * lcov --quiet --capture --gcov-tool $ANDROID_TOOLCHAIN/i686-linux-android-gcov --base-directory $ANDROID_BUILD_TOP --directory $TEST_COMPONENT_GCDA_DIR -output-file all.info
 * TEST_COMPONENT_SOURCE_FOLDER_FILTER="*?/<componentSourceFolderName>/?*"
 * lcov --quiet --extract all.info $TEST_COMPONENT_SOURCE_FOLDER_FILTER -output-file cov.info
 * genhtml cov.info -o whole_report
 * @endcode
 */

/** @file
 * Header file to auto instantiate the manual gcov flush thread.
 *
 * @see GcovFlushWithProp
 * @see gcovFlushWithPropInstance
 */

#pragma once

#include <pthread.h>

/** Singleton thread to force a gcov flush on an external event.
 *
 * When this singleton is instantiate a new thread is started.
 * This thread read regulary the Android property @c gcov.flush.force.
 * When this properties become @c true a gcov flush is run.
 */
class GcovFlushWithProp
{
public:
    /** Destructor force a gcov flush but never call in system shared lib
     *
     * Normaly this destructor is called after the main and before unload the
     * code from memory by the OS, like others singletons destructors.
     * But this destructor is never called because it is inside an Android
     * shared system library and this kind of library is never killed cleanly.
     */
    ~GcovFlushWithProp();

    /** Acces on the unique instance of this class
     *
     * @return the unique instance of this class
     */
    static GcovFlushWithProp &getInstance();

private:
    /** Constructor in private because is a singleton
     *
     * Create the thread
     * @see run
     */
    GcovFlushWithProp();

    /** Function for create the thread
     *
     * The function pthread_create can not be take a class member function.
     * We need to have a C function or a static class function.
     * This function just call the run member function.
     *
     * @param[in] that The @c this pointer to be able to call the run function
     * @return The return value of the thread, not used return @c NULL
     */
    static void *staticThread(void *that);

    /** thread function, detecting external event and make the flush
     *
     * This thread read the property @c gcov.flush.force every second
     * If the value become @c true force the gcov flush.
     * You need to modify the property to @c false or restart the executable or
     * shared system library to be able to force a gcov again.
     */
    void run();

    /** The pthread struct of the thread of this class */
    pthread_t thread;

    /** Variable to ask to stop the thread when the destructor is called
     *
     * If @c true the thread will be stop
     */
    bool stopThread;
};

/** A local file variable to force to instantiate the singleton
 *
 * This permits to the shared lib to start the thread without modify the source
 * code, just by adding the cflag <tt>-include %GcovFlushWithProp.h</tt> and link
 * with this static lib.
 */
static GcovFlushWithProp *gcovFlushWithPropInstance = &(GcovFlushWithProp::getInstance());

