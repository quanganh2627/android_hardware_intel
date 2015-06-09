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

#include "serializer/framework/Polymorphism.hpp"

namespace audio_comms
{
namespace utilities
{
namespace xmlserializer
{

namespace detail
{

/** Travel through PolymorphismTrait's SuportedTypelist
 * to find a matching morphism and {de}serialize it.
 */
template <class Base, class SuportedTypelist>
class Morph;

/** Pattern match polymorphic trait and handle it. */
template <const char *tag, class Base, class SuportedTypelist>
class SerializerChildren<serializer::PolymorphismTrait<tag, Base, SuportedTypelist> >
{
public:
    static Result toXml(const Base *const &element, TiXmlNode &xmlElement)
    {
        if (element == NULL) {
            return Result(invalidArgument) << "NULL pointer are not supported";
        }
        return detail::Morph<Base, SuportedTypelist>::toXml(*element, xmlElement)
               << " (While serializing polymorphic pointer)";
    }

    static Result fromXml(const TiXmlNode &xmlElement, Base * &element)
    {
        const TiXmlNode *xmlMorph = xmlElement.FirstChildElement();
        if (xmlMorph == NULL) {
            return Result(childNotFound) << "Expected a child node.";
        }
        if (xmlMorph->NextSiblingElement() != NULL) {
            return Result(childNotFound) << "Expected only one child node.";
        }
        return detail::Morph<Base, SuportedTypelist>::fromXml(*xmlMorph, element)
               << " (While deserializing polymorphic pointer)";
    }
};

template <class Base>
class Morph<Base, TYPELIST0>
{
public:
    static Result toXml(const Base &, TiXmlNode &)
    {
        return Result(invalidArgument) << "Unsupported morph";
    }

    static Result fromXml(const TiXmlNode &xmlMorph, Base * &)
    {
        return Result(invalidArgument)
               << "Tag \"" << xmlMorph.Value() << "\" is not a supported morph";
    }
};

template <class Base, class H, class T>
class Morph<Base, TypeList<H, T> >
{
public:
    static Result toXml(const Base &element, TiXmlNode &xmlPolyMorph)
    {
        if (not H::isOf(element)) {
            // Try the next supported type
            return Morph<Base, T>::toXml(element, xmlPolyMorph);
        }
        TiXmlNode *xmlMorph = NULL;
        // Now that the type is determined, continue serialisation
        // as a Derived
        Result res = XmlTraitSerializer<typename H::DerivedTrait>::toXml(
            downCast(element), xmlMorph);
        if (res.isFailure()) {
            return res;
        }
        xmlPolyMorph.LinkEndChild(xmlMorph);
        return Result::success();
    }

    static Result fromXml(const TiXmlNode &xmlMorph, Base * &element)
    {
        if (strcmp(H::DerivedTrait::tag, xmlMorph.Value()) != 0) {
            // Try the next supported type
            return Morph<Base, T>::fromXml(xmlMorph, element);
        }
        // Now that the type is determined, continue deserialisation
        // as a Derived
        element = new Derived;
        return XmlTraitSerializer<typename H::DerivedTrait>::fromXml(
            xmlMorph, downCast(*element));
    }

private:
    typedef typename H::DerivedTrait::Element Derived;

    /* Convert a Base to derived.
     *
     * @see PolymorphismTrait
     */
    static Derived &downCast(Base &base)
    {
        return static_cast<Derived &>(base);
    }

    /** Const version of downCast. */
    static const Derived &downCast(const Base &base)
    {
        return static_cast<const Derived &>(base);
    }
};


/** Pattern match polymorphic trait to handle the ownership particularity. */
template <const char *tag, class Base, class SuportedTypelist, bool takeOwnership>
struct Deleter<serializer::PolymorphismTrait<tag, Base, SuportedTypelist>, takeOwnership>
{
    static void del(Base **element)
    {
        if (not takeOwnership) {
            delete *element;
        }
        delete element;
    }
};


} // namespace detail
} // namespace serializer
} // namespace utilities
} // namespace audio_comms
