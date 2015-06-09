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

namespace audio_comms
{
namespace utilities
{
namespace serializer
{

/** Trait to serialize a polymorphic pointer.
 *
 * NULL pointers are NOT supported.
 *
 * A polymorphic pointer is a pointer with a dynamic type of Base but a
 * static type of one of Base's derived class (or itself).
 * To serialize it, it's static type must be identified. As it can not be found
 * implicitly, the user must provide the expected possible morphisms.
 *
 * @tparam tag is a string identifying the polymorphism pointer described in this trait.
 * @tparam Base* is the dynamic type of the polymorphic pointer.
 * @tparam SuportedClasslist is a trait list (TypeList) describing
 *         the supported pointer morphisms.
 *         Each element of this list (the morphism traits) must contain the
 *         following static fields:
 *           - Derived is a class that can be static_cast to and from Base.
 *           - DerivedTrait is the relevant trait to serialize the Derived class.
 *           - bool isOf (const Base&) function that,
 *                                     given the polymorphism pointer,
 *                                     returns true if the pointer can be
 *                                     safely static_cast to Derived and
 *                                     serialized using the DerivedTrait.
 *                                     I.e. that serializing the pointer as
 *                                     a class Derived is lossless.
 *
 * The meaning of the Child::takeOwnership changes when Child::ChildTrait is a
 * PolymorphismTrait. Usually it is true if the setter takes the Trait::Element's
 * ownership, false otherwise.
 * A setter receives an Trait::Element&, aka Base *& for a PolymorphismTrait.
 * Nevertheless, a polymorphic setter must NOT take the ownership of the
 * Trait::Element, aka Base *, aka the pointer, but can only take the ownership
 * of the pointee.
 * If the pointee ownership is taken, Child::takeOwnership must be true.
 *
 * Example:
 *   - Given a base class B and two derived class D1 and D2.
 *   - Given the BTrait, D1Trait, D2Trait, the traits to serialize those objects.
 *
 *   The corresponding BRefTrait to serialize a polymorphic pointer to B would be:
 *
 * @code{.cpp}
 *
 *   // Starting by defining the morphism traits.
 *   class BMorphTrait {
 *       typedef BTrait DerivedTrait;
 *       bool isOf(const B &b) {
 *           // isOfTypeB is an arbitrary function that should return true
 *           // if b is really (static type) a B.
 *           // It can be implemented using rtti, enum or any other way.
 *           return isOfTypeB(b);
 *       }
 *   class D1MorphTrait {
 *       typedef D1Trait DerivedTrait;
 *       bool isOf(const B &b) { return isOfTypeD1(b); }
 *   }
 *   class D2MorphTrait {
 *       typedef D2Trait DerivedTrait;
 *       bool isOf(const B &b) { return isOfTypeD2(b); }
 *   }
 *
 *   // Define a tag to identify our polymorphic pointer
 *   const char BPolyTag[] = "BOrDerived";
 *
 *   // Define the Trait
 *   typedef PolymorphismTrait<BPolyTag, TYPELIST3(D1,D2,B) > BRefTrait;
 *
 * @endcode
 */
template <const char *mTag, class Base, class SuportedTypelist>
struct PolymorphismTrait
{
    /** Polymorphic pointer tag (unique identifier within siblings). */
    static const char *tag;
    /** Dynamic type of the polymorphic pointer. */
    typedef Base *Element;
};

template <const char *mTag, class B, class S>
const char *PolymorphismTrait<mTag, B, S>::tag = mTag;

} // namespace serializer
} // namespace utilities
} // namespace audio_comms
