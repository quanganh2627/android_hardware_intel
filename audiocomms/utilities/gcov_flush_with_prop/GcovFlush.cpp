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

#include "GcovFlush.hpp"

extern "C" void __gcov_flush();

GcovFlush::GcovFlush()
{
    // On constructor start the thread
}

GcovFlush::~GcovFlush()
{
    // Force gcov flush at the end of the main or shared lib
    // But this code is never call in a system shared lib
    __gcov_flush();
}

GcovFlush &GcovFlush::getInstance()
{
    static GcovFlush instance;
    return instance;
}
