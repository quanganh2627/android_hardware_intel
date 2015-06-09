/**
 * @section License
 *
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
 */
#pragma once

#include <utilities/TypeList.hpp>

/** Gtest macro encapsulation.
 *  Create and launch a TypedTest.
 *
 *  @param[in] myTest the test function functor.
 *  @param[in] typeList the TypeList used for tests.
 */
#define AUDIOCOMMS_TYPED_TEST(myTest, typeList) \
    TEST(myTest, typeList)                      \
    {                                           \
        TypedTest<myTest, typeList>::run();     \
    }


namespace audio_comms
{
namespace utilities
{

/** This class allows to create a Gtest test which
 *  launches a template test function on a template class.
 *  The test function must be wrapped in a template functor.
 *
 * @tparam T the type of the functor used to wrap the test
 * function.
 * @tparam TL the TypeList containing types that must be tested.
 */
template <template <class> class T, class TL>
class TypedTest;

/** First specialisation of TypedTest class (stop case).
 *  This specialisation is used to stop the test when the TypeList
 *  is empty.
 *
 * @tparam T the type of the functor used to wrap the test
 * function.
 */
template <template <class> class T>
class TypedTest<T, TYPELIST0>
{
public:
    /** End of Recursion, nothing to do  */
    static void run() {}
};

/** Second specialisation of TypedTest class.
 *  This specialisation is used to launch the test with the
 *  first type of the TypeList and to call recursively the
 *  run function on the remaining types list.
 *
 * @tparam T the type of the functor used to wrap the test
 * function.
 * @tparam HeadType the type to be used in the test.
 * @tparam RemainingTypes the list of remaining types.
 */
template <template <class> class T, class HeadType, class RemainingTypes>
class TypedTest<T, TypeList<HeadType, RemainingTypes> >
{
public:
    /** Launch the test function with the current type  */
    static void run()
    {
        // Functor instanciation and call with HeadType as template argument
        T<HeadType>() ();

        // Recursive call on remaining Type
        TypedTest<T, RemainingTypes>::run();
    }
};

}
}
