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
#include "serializer/framework/GetterHelper.hpp"
#include "serializer/framework/SetterHelper.hpp"
#include <utilities/TypeTraits.hpp>
#include <gtest/gtest.h>

using namespace audio_comms::utilities;
using namespace audio_comms::utilities::serializer;

// An abstract containor with serveral accessors
template <typename T>
struct Cont
{
    Cont(T val) : _val(val) {}
    Cont() : _val((T)66) {}

    // Different member getter
    T getCopy() { return _val; }
    T getConst() const { return _val; }
    T &getRef() { return _val; }
    const T &getRefConst() const { return _val; }

    // Different member getter
    bool setCopy(T val) { _val = val; return true; }
    bool setConst(const T val) { _val = val; return true; }
    bool setRef(T &val) { _val = val; return true; }
    bool setRefConst(const T &val) { _val = val; return true; }
    bool setDummy(const T &val) const
    {   // Const cast because "this" is const
        const_cast<Cont<T> *>(this)->_val = val;
        return true;
    }

    bool operator==(const Cont<T> comp) const
    {
        return _val == comp._val;
    }
    T _val;
};

/** Invoking this fonction will check if the 2 types F and T are the same*/
template <class F, class T>
bool fis_same(T)
{
    return is_same<F, T>::value;
}

/** Test that:
 *    - the type of GetterHelper<M, m>::function is GetterHelper<M, m>::type
 *    - GetterHelper<M, m>::function can be cast in F
 *    - the GetterHelper<M, m>::function(E(e)) returns e
 */
template <class F, class M, M m, class E>
void getCheck(E e)
{
    ASSERT_TRUE((fis_same<typename GetterHelper<M, m>::type>(
                     GetterHelper<M, m>::function)));
    F get = GetterHelper<M, m>::function;
    Cont<E> cont(e);
    E val = get(cont);
    ASSERT_EQ(e, val);
}

/** Call @Check on different accessors types. */
template <class Access, class T>
void getterCheck(T value)
{
    typedef Cont<T> TCont;

    // Copy
    {
        typedef T (*Get)(TCont &);
        getCheck<Get, typeof( &Access::getCopy), &Access::getCopy, T>(value);
    }

    // Const
    {
        typedef T (*Get)(const TCont &);
        getCheck<Get, typeof( &Access::getConst), &Access::getConst, T>(value);
    }

    // Ref
    {
        typedef T & (*Get)(TCont &);
        getCheck<Get, typeof( &Access::getRef), &Access::getRef, T>(value);
    }

    // ConstRef
    {
        typedef const T & (*Get)(const TCont &);
        getCheck<Get, typeof( &Access::getRefConst), &Access::getRefConst, T>(
            value);
    }
}

// Member getter on basic type
TEST(MethodGetter, Int)
{
    getterCheck<Cont<int> >((int)13);
}

typedef Cont<Cont<int> > ContClass;

// Member getter on class type
TEST(MethodGetter, Class)
{
    getterCheck<Cont<ContClass> >(ContClass(12));
}

// Function getter on base type

// Function accessor
template <class T>
struct ContAccess
{
    // Getters
    static T getCopy(Cont<T> &cont)
    {
        return cont.getCopy();
    }
    static T getConst(const Cont<T> &cont)
    {
        return cont.getConst();
    }
    static T &getRef(Cont<T> &cont)
    {
        return cont.getRef();
    }
    static const T &getRefConst(const Cont<T> &cont)
    {
        return cont.getRefConst();
    }

    // Setters
    static bool setCopy(Cont<T> &cont, T t)
    {
        return cont.setCopy(t);
    }
    static bool setConst(Cont<T> &cont, const T t)
    {
        return cont.setConst(t);
    }
    static bool setRef(Cont<T> &cont, T &t)
    {
        return cont.setRef(t);
    }
    static bool setRefConst(Cont<T> &cont, const T &t)
    {
        return cont.setRefConst(t);
    }
    static bool setDummy(const Cont<T> &cont, const T &t)
    {
        return cont.setDummy(t);
    }
};

// Fonction getter on basic type
TEST(FunctionGetter, ptr)
{
    getterCheck<ContAccess<int *> >((int *)66);
}

// Fonction getter on class type
TEST(FunctionGetter, Class)
{
    getterCheck<ContAccess<ContClass> >(ContClass(10));
}

///////////////////////////////
// Test setter

/** Test that:
 *    - SetterHelper<M, m>::function 's type is SetterHelper<M, m>::type
 *    - SetterHelper<M, m>::function can be cast in F
 *    - SetterHelper<M, m>::function realy sets the objects.
 */
template <class F, class M, M m, class E>
void setCheck(E e)
{
    ASSERT_TRUE((fis_same<typename SetterHelper<M, m>::type>(
                     SetterHelper<M, m>::function)));
    F set = SetterHelper<M, m>::function;
    // Build invalid objet
    Cont<E> cont;
    // Sets valid value
    ASSERT_TRUE(set(cont, e));
    // Check that value is set
    ASSERT_TRUE(e == cont.getConst());
}

template <class Access, class T>
void setterCheck(T value)
{
    typedef Cont<T> TCont;

    // Copy
    {
        typedef bool (*Set)(TCont &, T);
        setCheck<Set, typeof( &Access::setCopy), &Access::setCopy, T>(value);
    }

    // Const
    {
        typedef bool (*Set)(TCont &, const T);
        setCheck<Set, typeof( &Access::setConst), &Access::setConst, T>(value);
    }

    // Ref
    {
        typedef bool (*Set)(TCont &, T &);
        setCheck<Set, typeof( &Access::setRef), &Access::setRef, T>(value);
    }

    // ConstRef
    {
        typedef bool (*Set)(TCont &, const T &);
        setCheck<Set, typeof( &Access::setRefConst), &Access::setRefConst, T>(
            value);
    }

    // Const setter (strange)
    {
        typedef bool (*Set)(const TCont &, const T &);
        setCheck<Set, typeof( &Access::setDummy), &Access::setDummy, T>(value);
    }
}

// Fonction getter on basic type
TEST(FunctionSetter, ptr)
{
    setterCheck<ContAccess<char *> >((char *)2);
}

// Fonction getter on class type
TEST(FunctionSetter, Class)
{
    setterCheck<ContAccess<ContClass> >(ContClass(10));
}


// Member setter on basic type
TEST(MethodSetter, Int)
{
    setterCheck<Cont<int> >((int)13);
}

// Member getter on class type
TEST(MethodSetter, Class)
{
    setterCheck<Cont<ContClass> >(ContClass(12));
}
