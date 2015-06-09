/*
* Copyright (c) 2009-2011 Intel Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "JPEGParser.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

bool endOfBuffer(CJPEGParse* parser);

uint8_t readNextByte(CJPEGParse* parser) {
    uint8_t byte = 0;

    if (parser->parse_index < parser->buff_size) {
        byte = *( parser->stream_buff + parser->parse_index );
        parser->parse_index++;
    }

    if (parser->parse_index == parser->buff_size) {
        parser->end_of_buff = true;
    }

    return byte;
}

uint32_t readBytes( CJPEGParse* parser, uint32_t bytes_to_read ) {
    uint32_t bytes = 0;

    while (bytes_to_read-- && !endOfBuffer(parser)) {
        bytes |= ( (uint32_t)readNextByte(parser) << ( bytes_to_read * 8 ) );
    }

    return bytes;
}

void burnBytes( CJPEGParse* parser, uint32_t bytes_to_burn ) {
    parser->parse_index += bytes_to_burn;

    if (parser->parse_index >= parser->buff_size) {
        parser->parse_index = parser->buff_size - 1;
        parser->end_of_buff = true;
    }
}

uint8_t getNextMarker(CJPEGParse* parser) {
    while (!endOfBuffer(parser)) {
        if (readNextByte(parser) == 0xff) {
            break;
        }
    }
    /* check the next byte to make sure we don't miss the real marker*/
    uint8_t tempNextByte = readNextByte(parser);
    if (tempNextByte == 0xff)
        return readNextByte(parser);
    else
        return tempNextByte;
}

bool setByteOffset(CJPEGParse* parser, uint32_t byte_offset)
{
    bool offset_found = false;

    if (byte_offset < parser->buff_size) {
        parser->parse_index = byte_offset;
        offset_found = true;
//      end_of_buff = false;
    }

    return offset_found;
}

uint32_t getByteOffset(CJPEGParse* parser) {
    return parser->parse_index;
}

bool endOfBuffer(CJPEGParse* parser) {
    return parser->end_of_buff;
}

const uint8_t* getCurrentIndex(CJPEGParse* parser) {
    return parser->stream_buff + parser->parse_index;
}

uint32_t getRemainingBytes(CJPEGParse* parser) {
    return parser->buff_size - parser->parse_index - 1;
}

bool endOfBufferStr(CJPEGParse* parser);

uint8_t readNextByteStr(CJPEGParse* parser) {
    uint8_t byte = 0;

    if (parser->parse_index < parser->inputs->size()) {
        byte = parser->inputs->itemAt(parser->parse_index);
        parser->parse_index++;
    }

    if (parser->parse_index == parser->inputs->size()) {
        parser->end_of_buff = true;
    }

    return byte;
}

uint32_t readBytesStr( CJPEGParse* parser, uint32_t bytes_to_read ) {
    uint32_t bytes = 0;

    while (bytes_to_read-- && !endOfBufferStr(parser)) {
        bytes |= ( (uint32_t)readNextByteStr(parser) << ( bytes_to_read * 8 ) );
    }

    return bytes;
}

void burnBytesStr( CJPEGParse* parser, uint32_t bytes_to_burn ) {
    parser->parse_index += bytes_to_burn;

    if (parser->parse_index >= parser->inputs->size()) {
        parser->parse_index = parser->inputs->size() - 1;
        parser->end_of_buff = true;
    }
}

uint8_t getNextMarkerStr(CJPEGParse* parser) {
    while (!endOfBufferStr(parser)) {
        if (readNextByteStr(parser) == 0xff) {
            break;
        }
    }
    /* check the next byte to make sure we don't miss the real marker*/
    uint8_t tempNextByte = readNextByteStr(parser);
    if (tempNextByte == 0xff)
        return readNextByteStr(parser);
    else
        return tempNextByte;
}

bool setByteOffsetStr(CJPEGParse* parser, uint32_t byte_offset)
{
    bool offset_found = false;

    if (byte_offset < parser->inputs->size()) {
        parser->parse_index = byte_offset;
        offset_found = true;
//      end_of_buff = false;
    }

    return offset_found;
}

uint32_t getByteOffsetStr(CJPEGParse* parser) {
    return parser->parse_index;
}

bool endOfBufferStr(CJPEGParse* parser) {
    return parser->end_of_buff;
}

const uint8_t* getCurrentIndexStr(CJPEGParse* parser) {
    return parser->inputs->array() + parser->parse_index;
}

uint32_t getRemainingBytesStr(CJPEGParse* parser) {
    return parser->inputs->size() - parser->parse_index - 1;
}

void parserInitialize(CJPEGParse* parser,  const uint8_t* stream_buff, uint32_t buff_size) {
    parser->parse_index = 0;
    parser->buff_size = buff_size;
    parser->stream_buff = stream_buff;
    parser->end_of_buff = false;
    parser->readNextByte = readNextByte;
    parser->readBytes = readBytes;
    parser->burnBytes = burnBytes;
    parser->getNextMarker = getNextMarker;
    parser->getByteOffset = getByteOffset;
    parser->endOfBuffer = endOfBuffer;
    parser->getCurrentIndex = getCurrentIndex;
    parser->setByteOffset= setByteOffset;
    parser->getRemainingBytes = getRemainingBytes;
}

void parserInitialize(CJPEGParse* parser, android::Vector<uint8_t> *inputs) {
    parser->parse_index = 0;
    parser->buff_size = inputs->size();
    parser->inputs = inputs;
    parser->end_of_buff = false;
    parser->readNextByte = readNextByteStr;
    parser->readBytes = readBytesStr;
    parser->burnBytes = burnBytesStr;
    parser->getNextMarker = getNextMarkerStr;
    parser->getByteOffset = getByteOffsetStr;
    parser->endOfBuffer = endOfBufferStr;
    parser->getCurrentIndex = getCurrentIndexStr;
    parser->setByteOffset= setByteOffsetStr;
    parser->getRemainingBytes = getRemainingBytesStr;
}

