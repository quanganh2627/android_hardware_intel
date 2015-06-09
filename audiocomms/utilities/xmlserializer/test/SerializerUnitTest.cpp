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
#include "xmlserializer/Serializer.hpp"

#include <gtest/gtest.h>
#include <string>
#include <list>

using namespace audio_comms::utilities::xmlserializer;
using namespace audio_comms::utilities::serializer;

template <class Result>
void ASSERT_RESULT_SUCCESS(Result res)
{
    ASSERT_TRUE(res.isSuccess()) << res.format();
}
template <class Result>
void ASSERT_RESULT_FAILURE(Result res)
{
    ASSERT_FALSE(res.isSuccess()) << res.format();
}

class ToXml : public testing::Test
{
public:
    template <class Trait, typename Elem>
    void toXml(Elem elem, bool success = true)
    {
        Result res = XmlTraitSerializer<Trait>::toXml(elem, _xmlNode);
        if (success) {
            ASSERT_RESULT_SUCCESS(res);
            _doc.LinkEndChild(_xmlNode);
            _stream << _doc;
            _xml = _stream.c_str();
        } else {
            ASSERT_RESULT_FAILURE(res);
        }
    }

protected:
    TiXmlNode *_xmlNode;
    TiXmlDocument _doc;
    TiXmlOutStream _stream;
    std::string _xml;
};
class FromXml : public ToXml
{
public:
    FromXml() : _handle(NULL) {}

    /** Parse a xml string and return a node to the root element */
    void parseXml(const std::string xml)
    {
        _doc.Parse(xml.c_str());
        EXPECT_FALSE(_doc.Error()) << _doc.ErrorDesc() << " While parsing: " << xml;
        _xmlNode = _doc.RootElement();
        ASSERT_TRUE(_xmlNode != NULL);
    }

    template <class Trait>
    void dererializeString(const std::string &refStr, bool success = true)
    {
        std::string tag = Trait::tag;
        parseXml("<" + tag + ">" + refStr + "</" + tag + ">");

        typename Trait::Element str;
        Result res = XmlTraitSerializer<Trait>::fromXml(*_xmlNode, str);
        if (success) {
            ASSERT_RESULT_SUCCESS(res);
        } else {
            ASSERT_RESULT_FAILURE(res);
        }
        EXPECT_EQ(refStr, str);
    }

protected:
    TiXmlText *_textXml;
    TiXmlHandle _handle;
};

// Empty node (de)serialisation
struct EmptyElem {};

struct EmptyElemTrait
{
    static const char *tag;
    typedef EmptyElem Element;
    typedef TYPELIST0 Children;
};
const char *EmptyElemTrait::tag = "Empty";

TEST_F(ToXml, EmptyElement)
{
    toXml<EmptyElemTrait>(EmptyElem());

    EXPECT_EQ("<Empty />", _xml);
}

TEST_F(FromXml, EmptyElement)
{
    EmptyElem emptyElem;
    ASSERT_RESULT_SUCCESS(XmlTraitSerializer<EmptyElemTrait>::fromXml(TiXmlElement("Empty"),
                                                                      emptyElem));
}

TEST_F(FromXml, WrongElement)
{
    EmptyElem emptyElem;
    ASSERT_RESULT_FAILURE(XmlTraitSerializer<EmptyElemTrait>::fromXml(TiXmlElement("WrongTag"),
                                                                      emptyElem));
}

// Empty node (de)serialisation
class NotSoEmptyElem
{
public:
    void setEmptyElem(EmptyElem emptyElem) { _emptyElem = emptyElem; }
    EmptyElem getEmptyElem() const { return _emptyElem; }

private:
    EmptyElem _emptyElem;
};

struct NotSoEmptyElemTrait
{
    static const char *tag;
    typedef NotSoEmptyElem Element;
    typedef Child<EmptyElemTrait,
                  typeof( &Element::getEmptyElem), &Element::getEmptyElem,
                  typeof( &Element::setEmptyElem), &Element::setEmptyElem, false> Child1;
    typedef TYPELIST1 (Child1) Children;
};
const char *NotSoEmptyElemTrait::tag = "NotSoEmpty";

TEST_F(ToXml, NotSoEmptyElement)
{
    toXml<NotSoEmptyElemTrait>(NotSoEmptyElem());
    EXPECT_EQ("<NotSoEmpty><Empty /></NotSoEmpty>", _xml);
}

TEST_F(FromXml, NotSoEmptyElement)
{
    NotSoEmptyElem emptyElem;
    parseXml("<NotSoEmpty><Empty /></NotSoEmpty>");

    ASSERT_RESULT_SUCCESS(XmlTraitSerializer<NotSoEmptyElemTrait>::fromXml(*_xmlNode,
                                                                           emptyElem));
}

// Text node (de)serialisation
struct IntElem {};

typedef TextTrait<int> IntTrait;

TEST_F(ToXml, textNode)
{
    toXml<IntTrait>(10);
    EXPECT_EQ("10", _xml);
}

TEST_F(FromXml, textNode)
{
    int result;
    ASSERT_RESULT_SUCCESS(XmlTraitSerializer<IntTrait>::fromXml(TiXmlText("10"), result));
    ASSERT_EQ(10, result);
}

// Element node with one text node (de)serialisation
struct Param
{
    explicit Param(int param) : _param(param) {}
    Param() : _param(66) {}
    virtual ~Param() {}
    const int &getValue() const { return _param; }
    void setValue(int param) { _param = param; }
    virtual bool isParam() const { return true; }
    int _param;
};

char paramDefaultTag[] = "Param";
template <char *_tag = paramDefaultTag>
struct ParamTrait
{
    static const char *tag;
    typedef Param Element;
    typedef Child<IntTrait,
                  typeof( &Element::getValue), &Element::getValue,
                  typeof( &Element::setValue), &Element::setValue, false> Child1;
    typedef TYPELIST1 (Child1) Children;
};

template <char *_tag>
const char *ParamTrait<_tag>::tag = _tag;

TEST_F(ToXml, Param)
{
    toXml<ParamTrait<> >(Param(20));
    EXPECT_EQ("<Param>20</Param>", _xml);
}

TEST_F(FromXml, EmptyParam)
{
    parseXml("<Param></Param>");

    Param param;
    ASSERT_RESULT_FAILURE(XmlTraitSerializer<ParamTrait<> >::fromXml(*_xmlNode, param));
}

TEST_F(FromXml, Param)
{
    parseXml("<Param>20</Param>");

    Param param;
    ASSERT_RESULT_SUCCESS(XmlTraitSerializer<ParamTrait<> >::fromXml(*_xmlNode, param));
    EXPECT_EQ(20, param.getValue());
}

// Tree (de)serialisation
struct BigElem
{
    BigElem(Param param1, Param param2) : _param1(param1), _param2(param2) {}
    BigElem() : _param1(66), _param2(99) {}
    const Param &getParam1() const { return _param1; }
    const Param &getParam2() const { return _param2; }
    void setParam1(Param param) { _param1 = param; }
    void setParam2(Param param) { _param2 = param; }
    Param _param1;
    Param _param2;
};

char param1Tag[] = "Param1";
char param2Tag[] = "Param2";

struct BigTrait
{
    static const char *tag;
    typedef BigElem Element;
    typedef Child<ParamTrait<param1Tag>,
                  typeof( &Element::getParam1), &Element::getParam1,
                  typeof( &Element::setParam1), &Element::setParam1, false> Child1;
    // Use a function accessor instead of the method
    static const Param &getChild2(const Element &elem) { return elem._param2; }
    static bool setChild2(Element &elem, const Param &val)
    {
        elem.setParam2(val);
        delete &val;
        return true;
    }
    typedef Child<ParamTrait<param2Tag>,
                  typeof( &getChild2), &getChild2,
                  typeof( &setChild2), &setChild2, true> Child2;

    typedef TYPELIST2 (Child1, Child2) Children;
};
const char *BigTrait::tag = "Big";

TEST_F(ToXml, BigElem)
{
    const BigElem big(Param(20), Param(30));
    toXml<BigTrait>(big);

    EXPECT_EQ("<Big>"
              "<Param1>20</Param1>"
              "<Param2>30</Param2>"
              "</Big>", _xml);
}

TEST_F(FromXml, BigElem)
{
    parseXml("<Big>"
             "<Param1>10</Param1>"
             "<Param2>20</Param2>"
             "</Big>");

    BigElem big;
    ASSERT_RESULT_SUCCESS(XmlTraitSerializer<BigTrait>::fromXml(*_xmlNode, big));
    EXPECT_EQ(10, big.getParam1().getValue());
    EXPECT_EQ(20, big.getParam2().getValue());
}

char StringTag[] = "String";
typedef NamedTextTrait<std::string, StringTag> StringTrait;

TEST_F(ToXml, String)
{
    const StringTrait::Element str(":/");
    toXml<StringTrait>(str);

    EXPECT_EQ("<String>:/</String>", _xml);
}

TEST_F(FromXml, String)
{
    dererializeString<StringTrait>(":/");
}

TEST_F(FromXml, Empty_String)
{
    dererializeString<StringTrait>("", false);
}

typedef NamedTextTrait<std::string, StringTag, true> OptionalStringTrait;

TEST_F(FromXml, Optional_String)
{
    dererializeString<OptionalStringTrait>(":/");
}

TEST_F(FromXml, Optional_Empty_String)
{
    dererializeString<OptionalStringTrait>("");
}

char CollectionTag[] = "IntColl";
typedef CollectionTrait<CollectionTag, std::list<int>, IntTrait> IntCollectionTrait;

TEST_F(ToXml, Collection)
{
    IntCollectionTrait::Element myList;
    myList.push_back(10);
    myList.push_back(-20);

    toXml<IntCollectionTrait>(myList);

    EXPECT_EQ("<IntColl>"
              "<e1>10</e1>"
              "<e2>-20</e2>"
              "</IntColl>", _xml);
}

TEST_F(ToXml, EmptyCollection)
{
    IntCollectionTrait::Element myList;

    toXml<IntCollectionTrait>(myList);

    EXPECT_EQ("<IntColl />", _xml);
}

TEST_F(FromXml, Collection)
{
    parseXml("<IntColl>"
             "<e1>10</e1>"
             "<e2>20</e2>"
             "<e3>-13</e3>"
             "</IntColl>");

    IntCollectionTrait::Element myList;
    ASSERT_RESULT_SUCCESS(XmlTraitSerializer<IntCollectionTrait>::fromXml(*_xmlNode, myList));

    IntCollectionTrait::Element myListRef;
    myListRef.push_back(10);
    myListRef.push_back(20);
    myListRef.push_back(-13);

    ASSERT_EQ(myListRef, myList);
}

TEST_F(FromXml, EmptyCollection)
{
    parseXml("<IntColl/>");

    IntCollectionTrait::Element myList;
    ASSERT_RESULT_SUCCESS(XmlTraitSerializer<IntCollectionTrait>::fromXml(*_xmlNode, myList));

    ASSERT_TRUE(myList.empty());
}


char PolymorphismTag[] = "Poly";
typedef PolymorphismTrait<PolymorphismTag, BigElem, TYPELIST0> EmptyPolyTrait;

TEST_F(ToXml, NullPolyTrait)
{
    toXml<EmptyPolyTrait>((BigElem *)NULL, false);
}

TEST_F(ToXml, EmptyPolyTrait)
{
    BigElem big;

    toXml<EmptyPolyTrait>(&big, false);
}

TEST_F(FromXml, EmptyPolyTrait)
{
    parseXml("<Poly/>");
    BigElem *big;
    ASSERT_RESULT_FAILURE(XmlTraitSerializer<EmptyPolyTrait>::fromXml(*_xmlNode, big));
}

struct ParamMorphTrait
{
    typedef ParamTrait<> DerivedTrait;
    static bool isOf(const Param &param) { return param.isParam(); }
};

struct Param2 : Param
{
    Param2(int param, bool param2) : Param(param), mParam2(param2) {}
    Param2() : mParam2(false) {}
    const bool &getValue2() const { return mParam2; }
    void setValue2(bool param) { mParam2 = param; }
    virtual bool isParam() const { return false; }
    bool mParam2;
};

char boolTag[] = "bool";
struct Param2Trait
{
    static const char *tag;
    typedef Param2 Element;
    typedef Child<NamedTextTrait<bool, boolTag>,
                  typeof( &Element::getValue2), &Element::getValue2,
                  typeof( &Element::setValue2), &Element::setValue2, false> Child2;
    typedef TYPELIST2 (ParamTrait<>::Child1, Child2) Children;
};
const char *Param2Trait::tag = "Param2";

struct Param2MorphTrait
{
    typedef Param2Trait DerivedTrait;
    static bool isOf(const Param &param) { return not param.isParam(); }
};

typedef PolymorphismTrait<PolymorphismTag, Param,
                          TYPELIST2(ParamMorphTrait, Param2MorphTrait)> PolyMorphTrait;

TEST_F(ToXml, PolyMorphTrait_base)
{
    Param param(22);

    toXml<PolyMorphTrait>(&param);

    EXPECT_EQ("<Poly><Param>22</Param></Poly>", _xml);
}


TEST_F(FromXml, PolyMorphTrait_base)
{
    parseXml("<Poly><Param>10</Param></Poly>");
    Param *param;
    ASSERT_RESULT_SUCCESS(XmlTraitSerializer<PolyMorphTrait>::fromXml(*_xmlNode, param));
    ASSERT_TRUE(param->isParam());
    ASSERT_EQ(10, param->getValue());
    delete param;
}

TEST_F(ToXml, PolyMorphTrait_derived)
{
    Param2 param2(33, true);
    Param *param = &param2;

    toXml<PolyMorphTrait>(param);

    EXPECT_EQ("<Poly><Param2>33<bool>true</bool></Param2></Poly>", _xml);
}


TEST_F(FromXml, PolyMorphTrait_derived)
{
    parseXml("<Poly><Param2>11<bool>true</bool></Param2></Poly>");
    Param *param2;
    ASSERT_RESULT_SUCCESS(XmlTraitSerializer<PolyMorphTrait>::fromXml(*_xmlNode, param2));
    ASSERT_FALSE(param2->isParam());
    ASSERT_EQ(11, param2->getValue());
    ASSERT_TRUE(static_cast<Param2 *>(param2)->getValue2());
    delete param2;
}
