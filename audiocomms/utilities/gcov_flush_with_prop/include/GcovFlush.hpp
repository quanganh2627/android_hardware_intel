/*
 * Copyright 2014 Intel Corporation
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

#pragma once

class GcovFlush
{
public:
    /** Destructor force a gcov flush but never call in system shared lib
     *
     * Normaly this destructor is called after the main and before unload the
     * code from memory by the OS, like others singletons destructors.
     * But this destructor is never called because it is inside an Android
     * shared system library and this kind of library is never killed cleanly.
     */
    ~GcovFlush();

    /**
     * Acces on the unique instance of this class
     *
     * @return the unique instance of this class
     */
    static GcovFlush &getInstance();

    /**
     * Constructor in private because is a singleton
     * Create the thread
     */
    GcovFlush();
};

/** A local file variable to force to instantiate the singleton
 *
 * This permits to the shared lib to start the thread without modify the source
 * code, just by adding the cflag <tt>-include %GcovFlush.h</tt> and link
 * with this static lib.
 */
static GcovFlush *gcovFlushInstance = &(GcovFlush::getInstance());
