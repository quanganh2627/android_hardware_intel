/**
 * @section License
 *
 * Copyright 2013-2014 Intel Corporation
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
 */
#pragma once

#include "utilities/DefaultDelete.hpp"
#include <NonCopyable.hpp>
#include <cstdlib>

namespace audio_comms
{
namespace utilities
{

/** Smart pointer with unique object ownership semantics.
 *
 * Smart pointer that retains sole ownership of an object through a pointer
 * and destroys that object when the unique_ptr goes out of scope.
 * No two unique_ptr instances can manage the same object.
 *
 * The object is destroyed and its memory deallocated when either of the following happens:
 *     - the UniquePtr managing the object is destroyed.
 *     - the reset() method is called
 *
 * @todo: The array specialisation
 */
template <class T, class D = DefaultDelete<T> >
class UniquePtr : private NonCopyable
{
public:
    typedef T Element;
    typedef T *Pointer;
    typedef const T *CPointer;
    typedef D Deleter;

    /** Constuct a constructs a new unique_ptr
     *
     * @param[in] pointer pointer to the object to manage.
     */
    explicit UniquePtr(Pointer pointer = Pointer()) : _ptr(pointer), _deleter() {}
    /** Constuct a constructs a new unique_ptr
     *
     * @param[in] p pointer to the object to manage.
     * @param[in] deleter to use to delete the object.
     */
    UniquePtr(Pointer p, const Deleter &deleter) : _ptr(p), _deleter(deleter) {}
    /** Destructs the managed object if such is present */
    ~UniquePtr() { reset(); }

    /** Dereferences pointer to the managed object. */
    Element &operator*() const { return *_ptr; }
    /** Dereferences pointer to the managed object. */
    Pointer operator->() const { return _ptr; }

    /** Returns a pointer to the managed object. */
    Pointer get() { return _ptr; }
    /** Return a const pointer to the managed object. */
    CPointer get() const { return _ptr; }

    /** Return a pointer reference to replace the managed object.
     *
     * Delete the managed object and return a pointer reference to set a new
     * managed object (abandoning ownership in the process).
     */
    Pointer &getRefToSet() { reset(); return _ptr; }

    Deleter &getDeleter() { return _deleter; }

    /** Delete the managed object. */
    void reset(Pointer ptr = Pointer()) { _deleter(_ptr); _ptr = ptr; }

    /** Returns a pointer to the managed object, releasing the ownership  */
    Pointer release()
    {
        Pointer tmp = _ptr;
        _ptr = Pointer();
        return tmp;
    }

private:
    /** Pointer to the managed object. */
    Pointer _ptr;
    /** The deleter used to delete the managed object. */
    Deleter _deleter;
};

#define UNIQUE_PTR_OPERATOR(oper)                                              \
    template <class T, class D1, class D2>                                     \
    bool operator oper(const UniquePtr<T, D1> &p1, const UniquePtr<T, D2> &p2) \
    {                                                                          \
        return p1.get() oper p2.get();                                         \
    }                                                                          \
    /** Implement comparison to void* for NULL handling. */                    \
    template <class T, class D2>                                               \
    bool operator oper(const void *p1, const UniquePtr<T, D2> &p2)             \
    {                                                                          \
        return p1 oper p2.get();                                               \
    }                                                                          \
    template <class T, class D1>                                               \
    bool operator oper(const UniquePtr<T, D1> &p1, const void *p2)             \
    {                                                                          \
        return p1.get() oper p2;                                               \
    }

UNIQUE_PTR_OPERATOR(==)
UNIQUE_PTR_OPERATOR(!=)
UNIQUE_PTR_OPERATOR(<)
UNIQUE_PTR_OPERATOR(<=)
UNIQUE_PTR_OPERATOR(>)
UNIQUE_PTR_OPERATOR(>=)

#undef UNIQUE_PTR_OPERATOR

} // namespace utilities
} // namespace audio_comms
