/*
 * Copyright © 2012 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#ifndef LOG_H_
#define LOG_H_

#include <utils/Singleton.h>

namespace android {
namespace intel {

class Log : public Singleton<Log> {
public:
    enum {
        MODULE = 0,
        HWC,
    };
public:
    Log();

    void e(int comp, const char *fmt, ...);
    void e(const char*fmt, ...);
    void w(int comp, const char *fmt, ...);
    void w(const char *fmt, ...);
    void d(int comp, const char *fmt, ...);
    void d(const char *fmt, ...);
    void i(int comp, const char *fmt, ...);
    void i(const char *fmt, ...);
    void v(int comp, const char *fmt, ...);
    void v(const char *fmt, ...);
};

static inline uint32_t align_to(uint32_t arg, uint32_t align)
{
    return ((arg + (align - 1)) & (~(align - 1)));
}

}; // namespace merrifield
}; // namespace android

#endif /* LOG_H_ */
