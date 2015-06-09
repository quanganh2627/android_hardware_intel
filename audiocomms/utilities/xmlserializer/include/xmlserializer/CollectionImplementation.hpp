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

#include "serializer/framework/Collection.hpp"
#include "serializer/framework/ConvertToString.hpp"

#include "utilities/TypeTraits.hpp"

namespace audio_comms
{
namespace utilities
{
namespace xmlserializer
{

namespace detail
{

template <const char *_tag, class Collection, class ItemTrait, bool takeOwnership>
class SerializerChildren<serializer::CollectionTrait<_tag, Collection, ItemTrait,
                                                     takeOwnership> >
{
private:
    template <class A, class B>
    class AssertSame;
    template <class A>
    class AssertSame<A, A>
    {
    };

    AssertSame<typename Collection::value_type, typename ItemTrait::Element>
    Error_CollectionValueType_and_ItemTraitElement_should_be_the_same;

public:
    typedef typename Collection::iterator Iterator;
    typedef typename Collection::const_iterator ConstIterator;
    typedef typename Collection::value_type Item;

    static Result toXml(const Collection &collection,
                        TiXmlNode &xmlCollection)
    {
        size_t nb = 1;
        for (ConstIterator it = collection.begin(); it != collection.end(); it++) {
            /** Construct a constant ItemNb.
             * As it is constant the Item reference member (given at construction)
             * will never be modified. Remove constness because the constructor
             * must take a non const reference in case of non const context
             * (as during deserialization).
             */
            const ItemNb itemNb(const_cast<Item &>(*it));

            // Create the <nb>...</nb> xml node
            TiXmlNode *xmlItemNb;
            // Serialize the ItemNb
            Result res = XmlTraitSerializer<ItemNbTrait>::toXml(itemNb, xmlItemNb,
                                                                formatIndex(nb).c_str());
            if (res.isFailure()) {
                return res << " (While deserializing collection item << " << nb << ")";
            }
            // Add serialized ItemNb to it's parent
            xmlCollection.LinkEndChild(xmlItemNb);
            ++nb;
        }
        return Result::success();
    }

    static Result fromXml(const TiXmlNode &xmlCollection, Collection &collection)
    {
        size_t nb = 1;
        Iterator it = collection.begin();

        // For each <nb>...</nb> node
        const TiXmlNode *xmlItemNb;
        while ((xmlItemNb = xmlCollection.FirstChildElement(formatIndex(nb).c_str())) != NULL) {
            Item item;
            ItemNb itemNb(item);
            Result res = XmlTraitSerializer<ItemNbTrait>::fromXml(*xmlItemNb, itemNb,
                                                                  formatIndex(nb).c_str());
            if (res.isFailure()) {
                return res << " (While deserializing collection item" << nb << ")";
            }
            collection.push_back(item);
            ++nb;
            ++it;
        }
        return Result::success();
    }

private:
    static std::string formatIndex(int nb)
    {
        std::string nbStr;
        serializer::toString(nb, nbStr);
        // Xml tag must not start with a number
        return "e" + nbStr;
    }

    class ItemNb
    {
    public:
        ItemNb(Item &item) : _item(item) {}

        typedef const Item & (ItemNb::*Get)() const;
        const Item &get() const { return _item; }

        typedef void (ItemNb::*Set)(Item &value) const;
        void set(Item &value) const { _item = value; }

    private:
        Item &_item;
    };

    struct ItemNbTrait
    {
        typedef ItemNb Element;
        typedef serializer::Child<ItemTrait,
                           typename Element::Get, &Element::get,
                           typename Element::Set, &Element::set, takeOwnership
                           > ItemChildTrait;
        typedef TYPELIST1 (ItemChildTrait) Children;
    };
};

} // namespace detail
} // namespace serializer
} // namespace utilities
} // namespace audio_comms
