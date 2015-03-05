#ifndef _ACD_CMD_STRUCTS_H_
#define _ACD_CMD_STRUCTS_H_

#include "acds_module_cmd_structs.h"

#define ACD_FIELD_LENGTH (512)

struct acd_read_cmd_from_host
{
    struct acds_hdr_req hdr_req;
    UINT32 index;
};

struct acd_read_cmd_to_host
{
    struct acds_hdr_resp hdr_resp;
    UINT16 bytes_read;
    UINT32 acd_status;
    UINT8 buf[ACD_FIELD_LENGTH];
};

struct acd_write_cmd_from_host
{
    struct acds_hdr_req hdr_req;
    UINT32 index;
    UINT32 actual_size;
    UINT32 max_size;
    UINT8 buf[ACD_FIELD_LENGTH];
};

struct acd_write_cmd_to_host
{
    struct acds_hdr_resp hdr_resp;
    UINT32 acd_status;
};

struct acd_lock_cmd_from_host 
{
    struct acds_hdr_req hdr_req;
    UINT32 reserved;
};

struct acd_lock_cmd_to_host
{
    struct acds_hdr_resp hdr_resp;
    UINT32 acd_status;
};

struct acd_generic_cmd_from_host
{
    struct acds_hdr_req hdr_req;
};

struct acd_generic_cmd_to_host
{
    struct acds_hdr_resp hdr_resp;
    UINT32 acd_status;
};

#endif
