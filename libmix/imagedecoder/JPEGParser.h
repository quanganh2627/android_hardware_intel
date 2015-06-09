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

#ifndef _JPEG_PARSE_H_
#define _JPEG_PARSE_H_

#include <stdint.h>
#include <utils/Vector.h>
using namespace std;
// Marker Codes
#define CODE_SOF_BASELINE 0xC0
#define CODE_SOF1         0xC1
#define CODE_SOF2         0xC2
#define CODE_SOF3         0xC3
#define CODE_SOF5         0xC5
#define CODE_SOF6         0xC6
#define CODE_SOF7         0xC7
#define CODE_SOF8         0xC8
#define CODE_SOF9         0xC9
#define CODE_SOF10        0xCA
#define CODE_SOF11        0xCB
#define CODE_SOF13        0xCD
#define CODE_SOF14        0xCE
#define CODE_SOF15        0xCF
#define CODE_DHT          0xC4
#define CODE_RST0         0xD0
#define CODE_RST1         0xD1
#define CODE_RST2         0xD2
#define CODE_RST3         0xD3
#define CODE_RST4         0xD4
#define CODE_RST5         0xD5
#define CODE_RST6         0xD6
#define CODE_RST7         0xD7
#define CODE_SOI          0xD8
#define CODE_EOI          0xD9
#define CODE_SOS          0xDA
#define CODE_DQT          0xDB
#define CODE_DRI          0xDD
#define CODE_APP0         0xE0
#define CODE_APP1         0xE1
#define CODE_APP2         0xE2
#define CODE_APP3         0xE3
#define CODE_APP4         0xE4
#define CODE_APP5         0xE5
#define CODE_APP6         0xE6
#define CODE_APP7         0xE7
#define CODE_APP8         0xE8
#define CODE_APP9         0xE9
#define CODE_APP10        0xEA
#define CODE_APP11        0xEB
#define CODE_APP12        0xEC
#define CODE_APP13        0xED
#define CODE_APP14        0xEE
#define CODE_APP15        0xEF

struct CJPEGParse {
    const uint8_t* stream_buff;
    uint32_t parse_index;
    uint32_t buff_size;
    android::Vector<uint8_t> *inputs;
    bool end_of_buff;
    uint8_t (*readNextByte)(CJPEGParse* parser);
    uint32_t (*readBytes)( CJPEGParse* parser, uint32_t bytes_to_read );
    void (*burnBytes)( CJPEGParse* parser, uint32_t bytes_to_burn );
    uint8_t (*getNextMarker)(CJPEGParse* parser);
    uint32_t (*getByteOffset)(CJPEGParse* parser);
    bool (*endOfBuffer)(CJPEGParse* parser);
    const uint8_t* (*getCurrentIndex)(CJPEGParse* parser);
    bool (*setByteOffset)( CJPEGParse* parser, uint32_t byte_offset );
    uint32_t (*getRemainingBytes)(CJPEGParse* parser);
};

void parserInitialize(CJPEGParse* parser, const uint8_t* stream_buff, uint32_t buff_size);
void parserInitialize(CJPEGParse* parser, android::Vector<uint8_t> *inputs);

class JpegBitstreamParser
{
public:
    void set(android::Vector<uint8_t>* inputs)
    {
        parserInitialize(&parser, inputs);
        use_vector = true;
    }
    void set(const uint8_t *buf, uint32_t bufsize)
    {
        parserInitialize(&parser, buf, bufsize);
        use_vector = false;
    }
    bool tryReadNextByte(uint8_t *byte)
    {
        if (parser.getRemainingBytes(&parser) >= 1) {
            *byte = parser.readNextByte(&parser);
            return true;
        }
        return false;
    }
    bool tryReadBytes(uint32_t *bytes, uint32_t bytes_to_read)
    {
        if (parser.getRemainingBytes(&parser) >= bytes_to_read) {
            *bytes = parser.readBytes(&parser, bytes_to_read);
            return true;
        }
        return false;
    }
    bool tryBurnBytes(uint32_t bytes_to_burn)
    {
        if (parser.getRemainingBytes(&parser) >= bytes_to_burn) {
            parser.burnBytes(&parser, bytes_to_burn);
            return true;
        }
        return false;
    }
    bool tryGetNextMarker(uint8_t *marker)
    {
        uint32_t rollbackoff = parser.getByteOffset(&parser);
        while (!parser.endOfBuffer(&parser)) {
            if (tryReadNextByte(marker)) {
                if (*marker == 0xff) {
                    //rollbackoff = parser.parse_index - 1;
                    break;
                }
            } else {
                goto rollback;
            }
        }
        /* check the next byte to make sure we don't miss the real marker*/
        if (tryReadNextByte(marker)) {
            if (*marker == 0xff) {
                if (tryReadNextByte(marker)) {
                    return true;
                }
                else
                    goto rollback;
            }
            else {
                return true;
            }
        }
        else goto rollback;
rollback:
        parser.parse_index = rollbackoff;
        return false;
    }
    uint32_t getByteOffset()
    {
        return parser.getByteOffset(&parser);
    }
    bool endOfBuffer()
    {
        return parser.endOfBuffer(&parser);
    }
    const uint8_t* getCurrentIndex()
    {
        return parser.getCurrentIndex(&parser);
    }
    bool trySetByteOffset(uint32_t byte_offset)
    {
        uint32_t bufsize;
        if (use_vector)
            bufsize = parser.inputs->size();
        else
            bufsize= parser.buff_size;
        if (bufsize > byte_offset) {
            parser.setByteOffset(&parser, byte_offset);
            return true;
        }
        return false;
    }
    uint32_t getRemainingBytes()
    {
        return parser.getRemainingBytes(&parser);
    }
    uint8_t itemAt(uint32_t index)
    {
        if (use_vector)
            return parser.inputs->itemAt(index);
        else
            return parser.stream_buff[index];
    }
    void reset()
    {
        parser.parse_index = 0;
        parser.inputs = NULL;
        parser.stream_buff = NULL;
        parser.buff_size = 0;
        use_vector = false;
    }
private:
    CJPEGParse parser;
    bool use_vector;
};
#endif // _JPEG_PARSE_H_

