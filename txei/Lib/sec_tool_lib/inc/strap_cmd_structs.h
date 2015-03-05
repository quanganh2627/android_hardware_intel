#ifndef _STRAP_CMD_STRUCTS_H_
#define _STRAP_CMD_STRUCTS_H_

typedef uint32_t UINT32;
typedef uint16_t UINT16;
typedef uint8_t UINT8;
typedef int32_t INT32;

struct strap_hdr_req
{
    //UINT32 version;
    UINT16  main_opcode;
    UINT16 sub_opcode;
};

struct strap_hdr_resp
{
    //UINT32 version;
    UINT16  main_opcode;
    UINT16 sub_opcode;
    INT32 status;
};

#endif
