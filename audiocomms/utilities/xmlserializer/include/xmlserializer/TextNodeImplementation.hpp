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

#include "serializer/framework/TextNode.hpp"

namespace audio_comms
{
namespace utilities
{
namespace xmlserializer
{

namespace detail
{
/** Specialisation when accessing to a text node. */
template <class TextType, class Convertor>
struct ChildAccess<serializer::TextTrait<TextType, Convertor> >
{
    static Result get(const TiXmlNode &xmlParent, const TiXmlNode * &xmlChild)
    {
        xmlChild = xmlParent.FirstChild();
        if (xmlChild == NULL) {
            return Result(childNotFound) << "Node \"" << xmlParent.Value()
                                         << "\" is empy, expected a text node child";
        }
        return Result::success();
    }
};

} // namespace detail

/** Specialisation for Text nodes. */
template <class TextType, class Convertor>
class XmlTraitSerializer<serializer::TextTrait<TextType, Convertor> >
{
public:
    static Result toXml(const TextType &rawtext, TiXmlNode * &xmlText)
    {
        std::string text;
        if (not Convertor::toString(rawtext, text)) {
            return Result(conversionFailed) << "Unable to convert \"" << &rawtext
                                            << "\" to string.";
        }
        xmlText = new TiXmlText(text.c_str());
        return Result::success();
    }

    static Result fromXml(const TiXmlNode &xmlText, TextType &rawText)
    {
        const TiXmlText *textNode = xmlText.ToText();
        if (textNode == NULL) {
            return Result(unexpectedNodeType) << "Expected an xml text node";
        }
        std::string text(textNode->Value());
        if (not Convertor::fromString(text, rawText)) {
            return Result(conversionFailed) << "Unable to convert from string \""
                                            << text << "\".";
        }
        return Result::success();
    }
};

} // namespace xmlserializer
} // namespace utilities
} // namespace audio_comms
