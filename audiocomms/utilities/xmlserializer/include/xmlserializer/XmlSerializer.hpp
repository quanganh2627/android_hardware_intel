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

#include "xmlserializer/Result.hpp"
#include "serializer/framework/Child.hpp"
#include "utilities/TypeList.hpp"
#include <tinyxml.h>

namespace audio_comms
{
namespace utilities
{
namespace xmlserializer
{

namespace detail
{
/** (De)Serialize element children. */
template <class Parent, class ChildList>
class SerializerChildList;

template <class Trait>
class SerializerChildren;
} // namespace detail


template <class Trait>
class XmlTraitSerializer
{
public:
    typedef typename Trait::Element Element;

    /** A toXml version using Trait::tag as xml tag for the serialized element.
     *
     * @return toXml(element, xmlElement, Trait::tag)
     */
    static Result toXml(const Element &element, TiXmlNode * &xmlElement)
    {
        return toXml(element, xmlElement, Trait::tag);
    }

    /** Serialize to xml the given element according to Trait. */
    static Result toXml(const Element &element, TiXmlNode * &xmlElement, const char *tag)
    {
        xmlElement = new TiXmlElement(tag);

        Result res = detail::SerializerChildren<Trait>::toXml(element, *xmlElement);
        if (res.isFailure()) {
            delete xmlElement;
            xmlElement = NULL;
            return res << " (While serializing \"" << tag << "\")";
        }
        return res;
    }

    /** A fromXml version using Trait::tag as xml tag for the serialized element.
     *
     * @return fromXml(xmlElement, element, Trait::tag)
     */
    static Result fromXml(const TiXmlNode &xmlElement, Element &element)
    {
        return fromXml(xmlElement, element, Trait::tag);
    }

    /** Deserialize from xml the given element according to Trait. */
    static Result fromXml(const TiXmlNode &xmlElement, Element &element, const char *tag)
    {
        if (strcmp(tag, xmlElement.Value()) != 0) {
            return Result(wrongXmlNode)
                   << "Trying to deserialize the wrong element."
                   << "Found \"" << xmlElement.Value() << "\" expected " << tag;
        }
        return detail::SerializerChildren<Trait>::fromXml(xmlElement, element)
               << " (While desserializing \"" << tag << "\")";
    }
};

namespace detail
{
template <class Trait>
class SerializerChildren
{
public:
    static Result toXml(const typename Trait::Element &element,
                        TiXmlNode &xmlElement)
    {
        return detail::SerializerChildList<typename Trait::Element, typename Trait::Children>
               ::toXml(element, xmlElement);
    }

    static Result fromXml(const TiXmlNode &xmlElement, typename Trait::Element &element)
    {
        return detail::SerializerChildList<typename Trait::Element, typename Trait::Children>::
               fromXml(xmlElement, element);
    }
};

/** Do nothing in case of empty list. */
template <class Parent>
class SerializerChildList<Parent, TYPELIST0>
{
public:
    static Result toXml(const Parent &, TiXmlNode &)
    {
        return Result::success();
    }

    static Result fromXml(const TiXmlNode &, const Parent &)
    {
        return Result::success();
    }
};

/** Class to access to specific xml child of an xml node. */
template <class ChildTrait>
struct ChildAccess
{
    /** Get the xml child corresponding to the Child trait.
     *
     * @param[in] xmlParent the parent xml node.
     * @param[out] xmlChild the found child xml node, NULL if an error occured.
     *
     * @return Result(childNotFound) if the xml node child could not be found.
     *         success otherwise.
     */
    static Result get(const TiXmlNode &xmlParent, const TiXmlNode * &xmlChild)
    {
        xmlChild = xmlParent.FirstChildElement(ChildTrait::tag);
        if (xmlChild == NULL) {
            return Result(childNotFound) << "No element node \"" << ChildTrait::tag
                                         << "\" found in node \"" << xmlParent.Value()
                                         << '"';
        }
        return Result::success();
    }
};

template <class Trait, bool takeOwnership>
struct Deleter
{
    static void del(typename Trait::Element *element)
    {
        if (not takeOwnership) {
            delete element;
        }
    }
};

template <class Parent, class H, class T>
class SerializerChildList<Parent, TypeList<H, T> >
{
public:
    static Result toXml(const Parent &parent, TiXmlNode &xmlParent)
    {
        TiXmlNode *xmlChild = NULL;
        const typename H::ChildTrait::Element &child = H::Getter::function(parent);
        Result res = XmlTraitSerializer<typename H::ChildTrait>::toXml(child, xmlChild);
        if (res.isFailure()) {
            return res;
        }
        xmlParent.LinkEndChild(xmlChild);
        return SerializerChildList<Parent, T>::toXml(parent, xmlParent);
    }

    static Result fromXml(const TiXmlNode &xmlParent, Parent &parent)
    {
        const TiXmlNode *xmlChild;
        bool optional = H::optional;

        // Get the corresponding xml child
        Result res = ChildAccess<typename H::ChildTrait>::get(xmlParent, xmlChild);
        if (res.isFailure()) {
            if (not optional) {
                return res;
            }
            xmlChild = NULL;
        }

        res = setChild(xmlChild, parent);
        if (res.isFailure()) {
            return res;
        }

        // Deserialize sibling child
        return SerializerChildList<Parent, T>::fromXml(xmlParent, parent);
    }

private:
    /** Instanciate the head child, deserialize it, and set it in the parent. */
    static Result setChild(const TiXmlNode *xmlChild, Parent &parent)
    {
        typedef typename H::ChildTrait::Element ChildType;
        // Create a empty child
        /** @todo: template specialize in case of H::takeOwnership == false
         *        to instanciate the child on the stack
         */
        ChildType *child = new ChildType;
        // Fill it
        if (xmlChild != NULL) {
            Result res = XmlTraitSerializer<typename H::ChildTrait>::fromXml(*xmlChild, *child);
            if (res.isFailure()) {
                delete child;
                return res;
            }
        }
        H::Setter::function(parent, *child);
        detail::Deleter<typename H::ChildTrait, H::takeOwnership>::del(child);
        return Result::success();
    }
};

} // namespace detail
} // namespace serializer
} // namespace utilities
} // namespace audio_comms

#include "xmlserializer/TextNodeImplementation.hpp"
#include "xmlserializer/CollectionImplementation.hpp"
#include "xmlserializer/PolymorphismImplementation.hpp"
