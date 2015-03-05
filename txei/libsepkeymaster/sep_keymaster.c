/**********************************************************************
 * Copyright 2014 (c) Intel Corporation. All rights reserved.
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
 **********************************************************************/

/* API for keymaster HAL */
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define LOG_TAG "SEP_KEYMASTER"
#include "txei_drv.h"
#include "txei_log.h"
#include "sep_keymaster.h"

//Keymaster response id is command id with msb changed to 1
#define KEYMASTER_RSP_FLAG  0x80000000

static uint32_t caps_obtained = 0;
static uint32_t key_opaque_size = 0;

//In-place byte swap
void swap_byte_order(uint8_t * buf, uint32_t buf_len) {
    uint32_t i = 0;

    for (i = 0; i < (buf_len / 2); i++) {
        buf[i] ^= buf[(buf_len - 1) - i];
        buf[(buf_len - 1) - i] ^= buf[i];
        buf[i] ^= buf[(buf_len - 1) - i];
    }
}

void print_buf(char *prompt, uint8_t buf[], uint32_t size) {
    char s[128];
    int len;
    uint32_t i, j;

    LOGERR("%s", prompt);
    for (i = 0; i < size; i += 16) {
        if (i + 15 < size) {
            LOGERR(
                    "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                    buf[i], buf[i + 1], buf[i + 2], buf[i + 3], buf[i + 4],
                    buf[i + 5], buf[i + 6], buf[i + 7], buf[i + 8], buf[i + 9],
                    buf[i + 10], buf[i + 11], buf[i + 12], buf[i + 13],
                    buf[i + 14], buf[i + 15]);
        } else {
            len = 0;
            for (j = i; j < size; j++) {
                len += snprintf(s + len, 128 - len, "%02x ",
                        buf[j]);
            }
            LOGERR("%s", s);
        }
    }
}

sep_keymaster_return_t generate_keypair_cmd_buf(
        intel_keymaster_firmware_cmd_t * lib_cmd,
        ANDROID_HECI_KEYMASTER_CMD_RSA_GEN_KEY_REQUEST * fw_cmd) {
    sep_keymaster_return_t result = SEP_KEYMASTER_FAILURE;

    if (!(lib_cmd && fw_cmd)) {
        result = SEP_KEYMASTER_BAD_PARAMETER;
        LOGERR("Invalid parameters while creating keypair buffer\n");
        goto exit;
    }
    //Assuming that key type is RSA for now. TODO: change later
    if (lib_cmd->cmd_data_length
            < sizeof(intel_keymaster_keygen_params_t)
                    + sizeof(intel_keymaster_rsa_keygen_params_t)) {
        result = SEP_KEYMASTER_BAD_PARAMETER;
        LOGERR("Invalid input parameters while creating keypair buffer\n");
        goto exit;
    }

    intel_keymaster_keygen_params_t *keygen_params =
            (intel_keymaster_keygen_params_t *) lib_cmd->cmd_data;

    if (keygen_params->key_type != KEY_TYPE_RSA) //TODO: has to change later when other key types are supported
            {
        result = SEP_KEYMASTER_BAD_PARAMETER;
        LOGERR("Invalid key type while creating keypair buffer\n");
        goto exit;
    }

    intel_keymaster_rsa_keygen_params_t *rsa_keygen_params_t =
            (intel_keymaster_rsa_keygen_params_t *) keygen_params->key_params;

    fw_cmd->KeySize = rsa_keygen_params_t->modulus_size / 8;
    //Note, only copying ANDROID_HECI_KEYMASTER_PUBLIC_EXPONENT_MAX_SIZE bytes in
    memcpy(fw_cmd->PublicExponent, &rsa_keygen_params_t->public_exponent,
            ANDROID_HECI_KEYMASTER_PUBLIC_EXPONENT_MAX_SIZE);
    swap_byte_order(fw_cmd->PublicExponent,
            ANDROID_HECI_KEYMASTER_PUBLIC_EXPONENT_MAX_SIZE); //Exponent needs to be big-endian

    fw_cmd->Header.InputSize =
            sizeof(ANDROID_HECI_KEYMASTER_CMD_RSA_GEN_KEY_REQUEST)
                    - sizeof(ANDROID_HECI_AGENT_REQ_HEADER);

    result = SEP_KEYMASTER_SUCCESS;
    exit: return result;
}

sep_keymaster_return_t import_keypair_cmd_buf(
        intel_keymaster_firmware_cmd_t * lib_cmd,
        ANDROID_HECI_KEYMASTER_CMD_RSA_IMPORT_KEY_REQUEST * fw_cmd) {
    sep_keymaster_return_t result = SEP_KEYMASTER_FAILURE;

    if (!(lib_cmd && fw_cmd)) {
        result = SEP_KEYMASTER_BAD_PARAMETER;
        LOGERR(
                "Invalid parameters while creating buffer for importing keypair\n");
        goto exit;
    }
    //Assuming that key type is RSA for now. TODO: change later
    if (lib_cmd->cmd_data_length
            < sizeof(intel_keymaster_import_key_t)
                    + sizeof(intel_keymaster_rsa_key_t)) {
        result = SEP_KEYMASTER_BAD_PARAMETER;
        LOGERR(
                "Invalid input parameters while creating import keypair buffer\n");
        goto exit;
    }

    intel_keymaster_import_key_t *key =
            (intel_keymaster_import_key_t *) lib_cmd->cmd_data;
    intel_keymaster_rsa_key_t *rsa_key =
            (intel_keymaster_rsa_key_t *) key->key_data;

    if (!(rsa_key->public_exponent_length
            <= ANDROID_HECI_KEYMASTER_PUBLIC_EXPONENT_MAX_SIZE
            && rsa_key->modulus_length <= ANDROID_HECI_KEYMASTER_MAX_KEY_SIZE
            && rsa_key->private_exponent_length
                    <= ANDROID_HECI_KEYMASTER_MAX_KEY_SIZE)) {
        result = SEP_KEYMASTER_BAD_PARAMETER;
        LOGERR(
                "Invalid parameter length while creating buffer for importing keypair\n");
        goto exit;
    }

    fw_cmd->KeySize = rsa_key->modulus_length;
    memcpy(fw_cmd->Modulus, rsa_key->bytes, rsa_key->modulus_length);
    memset(fw_cmd->PublicExponent, 0, ANDROID_HECI_KEYMASTER_PUBLIC_EXPONENT_MAX_SIZE);
    memcpy(fw_cmd->PublicExponent + ANDROID_HECI_KEYMASTER_PUBLIC_EXPONENT_MAX_SIZE - rsa_key->public_exponent_length,
            rsa_key->bytes + rsa_key->modulus_length,
            rsa_key->public_exponent_length);
    memcpy(fw_cmd->PrivateExponent,
            rsa_key->bytes + rsa_key->modulus_length
                    + rsa_key->public_exponent_length,
            rsa_key->private_exponent_length);

    fw_cmd->Header.InputSize =
            sizeof(ANDROID_HECI_KEYMASTER_CMD_RSA_IMPORT_KEY_REQUEST)
                    - sizeof(ANDROID_HECI_AGENT_REQ_HEADER);

    result = SEP_KEYMASTER_SUCCESS;
    exit: return result;
}

sep_keymaster_return_t get_public_keypair_cmd_buf(
        intel_keymaster_firmware_cmd_t* lib_cmd,
        ANDROID_HECI_KEYMASTER_CMD_RSA_GET_PUBLIC_KEY_REQUEST* fw_cmd) {
    sep_keymaster_return_t result = SEP_KEYMASTER_FAILURE;

    if (!(lib_cmd && fw_cmd)) {
        result = SEP_KEYMASTER_BAD_PARAMETER;
        LOGERR(
                "Invalid parameters while creating buffer for getting public keypair\n");
        goto exit;
    }

    if (lib_cmd->cmd_data_length
            < sizeof(intel_keymaster_key_blob_t) + key_opaque_size) //TODO: is this check correct?
                    {
        result = SEP_KEYMASTER_BAD_PARAMETER;
        LOGERR(
                "Invalid input parameters while creating get public keypair buffer\n");
        goto exit;
    }

    intel_keymaster_key_blob_t *key_blob_t =
            (intel_keymaster_key_blob_t *) lib_cmd->cmd_data;
    memcpy(fw_cmd->KeyOpaque, key_blob_t->key_blob, key_opaque_size);

    fw_cmd->Header.InputSize = key_opaque_size;

    result = SEP_KEYMASTER_SUCCESS;
    exit: return result;
}

sep_keymaster_return_t sign_data_cmd_buf(
        intel_keymaster_firmware_cmd_t * lib_cmd,
        ANDROID_HECI_KEYMASTER_CMD_RSA_SIGN_DATA_NOPAD_REQUEST * fw_cmd) {
    sep_keymaster_return_t result = SEP_KEYMASTER_FAILURE;

    if (!(lib_cmd && fw_cmd)) {
        result = SEP_KEYMASTER_BAD_PARAMETER;
        LOGERR("Invalid parameters while creating buffer for signing data\n");
        goto exit;
    }

    if (lib_cmd->cmd_data_length < sizeof(intel_keymaster_signing_cmd_data_t)) //TODO: is this correct? No way to know how long key blob or data is
            {
        result = SEP_KEYMASTER_BAD_PARAMETER;
        LOGERR("Invalid input parameters while creating sign data buffer\n");
        goto exit;
    }

    intel_keymaster_signing_cmd_data_t *sign_data =
            (intel_keymaster_signing_cmd_data_t *) lib_cmd->cmd_data;
    fw_cmd->DataSize = sign_data->data_length;

    if (!(sign_data->data_length <= ANDROID_HECI_KEYMASTER_MAX_KEY_SIZE
            && sign_data->key_blob_length <= key_opaque_size)) {
        result = SEP_KEYMASTER_CMD_BUFFER_TOO_BIG;
        LOGERR(
                "Invalid input parameters while creating sign data buffer. Data is too big.\n");
        goto exit;
    }

    memcpy(fw_cmd->KeyOpaque, sign_data->buffer, sign_data->key_blob_length);
    memcpy(fw_cmd->Data, sign_data->buffer + sign_data->key_blob_length,
            sign_data->data_length);

    fw_cmd->Header.InputSize =
            sizeof(ANDROID_HECI_KEYMASTER_CMD_RSA_SIGN_DATA_NOPAD_REQUEST)
                    + key_opaque_size - sizeof(ANDROID_HECI_AGENT_REQ_HEADER);

    result = SEP_KEYMASTER_SUCCESS;
    exit: return result;
}

sep_keymaster_return_t verify_data_cmd_buf(
        intel_keymaster_firmware_cmd_t * lib_cmd,
        ANDROID_HECI_KEYMASTER_CMD_RSA_VERIFY_DATA_NOPAD_REQUEST * fw_cmd) {
    sep_keymaster_return_t result = SEP_KEYMASTER_FAILURE;

    if (!(lib_cmd && fw_cmd)) {
        result = SEP_KEYMASTER_BAD_PARAMETER;
        LOGERR("Invalid parameters while creating buffer for verifying data\n");
        goto exit;
    }

    if (lib_cmd->cmd_data_length < sizeof(intel_keymaster_verification_data_t)) //TODO: is this correct? What is the correct value to compare against?
            {
        result = SEP_KEYMASTER_BAD_PARAMETER;
        LOGERR("Invalid input parameters while creating verify data buffer\n");
        goto exit;
    }

    intel_keymaster_verification_data_t *verification_data =
            (intel_keymaster_verification_data_t *) lib_cmd->cmd_data;

    fw_cmd->DataSize = verification_data->data_length;
    if (verification_data->data_length > ANDROID_HECI_KEYMASTER_MAX_KEY_SIZE) {
        result = SEP_KEYMASTER_CMD_BUFFER_TOO_BIG;
        LOGERR(
                "Invalid input parameters while creating verify data buffer. Data is too big.\n");
        goto exit;
    }

    fw_cmd->SignatureSize = verification_data->signature_length;
    if (verification_data->signature_length
            > ANDROID_HECI_KEYMASTER_MAX_KEY_SIZE) {
        result = SEP_KEYMASTER_CMD_BUFFER_TOO_BIG;
        LOGERR(
                "Invalid input parameters while creating verify data buffer. Signature is too big.\n");
        goto exit;
    }

    if (verification_data->key_blob_length > key_opaque_size) {
        result = SEP_KEYMASTER_CMD_BUFFER_TOO_BIG;
        LOGERR(
                "Invalid input parameters while creating verify data buffer. Key blob is too big.\n");
        goto exit;
    }

    memcpy(fw_cmd->KeyOpaque, verification_data->buffer,
            verification_data->key_blob_length);
    memcpy(fw_cmd->Data,
            verification_data->buffer + verification_data->key_blob_length,
            verification_data->data_length);
    memcpy(fw_cmd->Signature,
            verification_data->buffer + verification_data->key_blob_length
                    + verification_data->data_length,
            verification_data->signature_length);

    fw_cmd->Header.InputSize =
            sizeof(ANDROID_HECI_KEYMASTER_CMD_RSA_VERIFY_DATA_NOPAD_REQUEST)
                    + key_opaque_size - sizeof(ANDROID_HECI_AGENT_REQ_HEADER);

    result = SEP_KEYMASTER_SUCCESS;
    exit: return result;
}

sep_keymaster_return_t set_response_cmd_id(const uint32_t rsp_id,
        uint32_t * fw_rsp_id) {
    sep_keymaster_return_t result = SEP_KEYMASTER_FAILURE;

    if (!fw_rsp_id) {
        result = SEP_KEYMASTER_BAD_PARAMETER;
        LOGERR("Invalid parameters while setting response command id\n");
        goto exit;
    }

    switch (rsp_id) {
    case ANDROID_HECI_KEYMASTER_CMD_ID_RSA_GEN_KEY:
        *fw_rsp_id = KEYMASTER_CMD_GENERATE_KEYPAIR | KEYMASTER_RSP_FLAG; //Don't know if FW OR's with RSP_FLAG
        break;
    case ANDROID_HECI_KEYMASTER_CMD_ID_RSA_IMPORT_KEY:
        *fw_rsp_id = KEYMASTER_CMD_IMPORT_KEYPAIR | KEYMASTER_RSP_FLAG;
        break;
    case ANDROID_HECI_KEYMASTER_CMD_ID_RSA_GET_PUBLIC_KEY:
        *fw_rsp_id = KEYMASTER_CMD_GET_KEYPAIR_PUBLIC | KEYMASTER_RSP_FLAG;
        break;
    case ANDROID_HECI_KEYMASTER_CMD_ID_RSA_SIGN_DATA_NOPAD:
        *fw_rsp_id = KEYMASTER_CMD_SIGN_DATA | KEYMASTER_RSP_FLAG;
        break;
    case ANDROID_HECI_KEYMASTER_CMD_ID_RSA_VERIFY_DATA_NOPAD:
        *fw_rsp_id = KEYMASTER_CMD_VERIFY_DATA | KEYMASTER_RSP_FLAG;
        break;
    default:
        *fw_rsp_id = 0; //TODO: cleanup
        break;
    }

    result = SEP_KEYMASTER_SUCCESS;

    exit: return result;
}

//Responses can be of different types
sep_keymaster_return_t set_response_status_and_data(const uint8_t * response,
        intel_keymaster_firmware_rsp_t * caller_rsp, uint32_t * rsp_length,
        const uint32_t caller_rsp_data_buf_size) {
    sep_keymaster_return_t result = SEP_KEYMASTER_FAILURE;

    if (!(response && caller_rsp && rsp_length)) {
        result = SEP_KEYMASTER_BAD_PARAMETER;
        LOGERR("Invalid parameters while setting response command id\n");
        goto exit;
    }
    //For now cast response to type ANDROID_HECI_AGENT_RESP_HEADER to get the value of fields in the header
    ANDROID_HECI_AGENT_RESP_HEADER *fw_rsp_hdr =
            (ANDROID_HECI_AGENT_RESP_HEADER *) response;

    switch (fw_rsp_hdr->ResponseCode) {
    case ANDROID_HECI_AGENT_RESPONSE_CODE_SUCCESS:
        caller_rsp->result = KEYMASTER_RESULT_SUCCESS;
        switch (fw_rsp_hdr->CmdId) {
        case ANDROID_HECI_KEYMASTER_CMD_ID_RSA_GEN_KEY:
        case ANDROID_HECI_KEYMASTER_CMD_ID_RSA_IMPORT_KEY: {
            ANDROID_HECI_KEYMASTER_CMD_RSA_GEN_KEY_RESPONSE *fw_rsp =
                    (ANDROID_HECI_KEYMASTER_CMD_RSA_GEN_KEY_RESPONSE *) response;

            //Set the length
            caller_rsp->rsp_result_and_data_length =
                    sizeof(intel_keymaster_key_blob_t)
                            + fw_rsp->Header.OutputSize
                            + sizeof(intel_keymaster_result_t);

            //Set the response length
            *rsp_length = sizeof(intel_keymaster_firmware_rsp_t)
                    + caller_rsp->rsp_result_and_data_length
                    - sizeof(intel_keymaster_result_t);
            if (caller_rsp_data_buf_size < *rsp_length) {
                LOGERR(
                        "Caller data buffer too small while generating/importing RSA key");
                result = SEP_KEYMASTER_RSP_BUFFER_TOO_SMALL;
                goto exit;
            }
            //Set the data
            intel_keymaster_key_blob_t *key_blob =
                    (intel_keymaster_key_blob_t *) caller_rsp->rsp_data;
            key_blob->key_blob_length = fw_rsp->Header.OutputSize;
            memcpy(key_blob->key_blob, fw_rsp->KeyOpaque,
                    key_blob->key_blob_length); //TODO: find secure version of memcpy
        }
            break;

        case ANDROID_HECI_KEYMASTER_CMD_ID_RSA_GET_PUBLIC_KEY: {
            ANDROID_HECI_KEYMASTER_CMD_RSA_GET_PUBLIC_KEY_RESPONSE *fw_rsp =
                    (ANDROID_HECI_KEYMASTER_CMD_RSA_GET_PUBLIC_KEY_RESPONSE *) response;

            //Set the length
            caller_rsp->rsp_result_and_data_length =
                    sizeof(intel_keymaster_public_key_t)
                            + sizeof(intel_keymaster_rsa_public_key_t)
                            + ANDROID_HECI_KEYMASTER_PUBLIC_EXPONENT_MAX_SIZE
                            + fw_rsp->KeySize
                            + sizeof(intel_keymaster_result_t);

            //Set the response length
            *rsp_length = sizeof(intel_keymaster_firmware_rsp_t)
                    + caller_rsp->rsp_result_and_data_length
                    - sizeof(intel_keymaster_result_t);
            if (caller_rsp_data_buf_size < *rsp_length) {
                LOGERR("Caller data buffer too small while getting RSA key");
                result = SEP_KEYMASTER_RSP_BUFFER_TOO_SMALL;
                goto exit;
            }
            //Set the data
            intel_keymaster_public_key_t *public_key =
                    (intel_keymaster_public_key_t *) caller_rsp->rsp_data;
            public_key->key_type = KEY_TYPE_RSA; //FW doesn't really set key type in its response. TODO: what's the point of returning RSA key size in the response? Use it to copy modulus below??

            intel_keymaster_rsa_public_key_t *rsa_public_key =
                    (intel_keymaster_rsa_public_key_t *) public_key->public_key_data;
            rsa_public_key->modulus_length = fw_rsp->KeySize;
            rsa_public_key->public_exponent_length =
                    ANDROID_HECI_KEYMASTER_PUBLIC_EXPONENT_MAX_SIZE;
            memcpy(rsa_public_key->bytes, fw_rsp->Modulus, fw_rsp->KeySize);
            memcpy(rsa_public_key->bytes + fw_rsp->KeySize,
                    fw_rsp->PublicExponent,
                    ANDROID_HECI_KEYMASTER_PUBLIC_EXPONENT_MAX_SIZE);
        }
            break;

        case ANDROID_HECI_KEYMASTER_CMD_ID_RSA_SIGN_DATA_NOPAD: {
            ANDROID_HECI_KEYMASTER_CMD_RSA_SIGN_DATA_NOPAD_RESPONSE *fw_rsp =
                    (ANDROID_HECI_KEYMASTER_CMD_RSA_SIGN_DATA_NOPAD_RESPONSE *) response;

            //Set the length
            caller_rsp->rsp_result_and_data_length =
                    sizeof(intel_keymaster_signature_t) + fw_rsp->SignatureSize
                            + sizeof(intel_keymaster_result_t);

            //Set the response length
            *rsp_length = sizeof(intel_keymaster_firmware_rsp_t)
                    + caller_rsp->rsp_result_and_data_length
                    - sizeof(intel_keymaster_result_t);
            if (caller_rsp_data_buf_size < *rsp_length) {
                LOGERR("Caller data buffer too small while signing data");
                result = SEP_KEYMASTER_RSP_BUFFER_TOO_SMALL;
                goto exit;
            }
            //Set the data
            intel_keymaster_signature_t *signature =
                    (intel_keymaster_signature_t *) caller_rsp->rsp_data;
            signature->signature_length = fw_rsp->SignatureSize;
            memcpy(signature->signature, fw_rsp->Signature,
                    fw_rsp->SignatureSize);
        }
            break;

        case ANDROID_HECI_KEYMASTER_CMD_ID_RSA_VERIFY_DATA_NOPAD: {
            ANDROID_HECI_KEYMASTER_CMD_RSA_VERIFY_DATA_NOPAD_RESPONSE *fw_rsp =
                    (ANDROID_HECI_KEYMASTER_CMD_RSA_VERIFY_DATA_NOPAD_RESPONSE *) response;

            //Set the length
            caller_rsp->rsp_result_and_data_length =
                    sizeof(intel_keymaster_result_t);

            //Set the verification result
            caller_rsp->result = fw_rsp->Verified ? KEYMASTER_RESULT_SUCCESS : KEYMASTER_RESULT_FAILURE;

            //Set the response length
            *rsp_length = sizeof(intel_keymaster_firmware_rsp_t);
        }
            break;

        default: //TODO: We should never get here? Invalid commands are caught at the beginning
            break;
        }
        break;
    case ANDROID_HECI_AGENT_RESPONSE_CODE_FAILURE:
        caller_rsp->result = KEYMASTER_RESULT_FAILURE;
        caller_rsp->rsp_result_and_data_length = sizeof(intel_keymaster_result_t);
        *rsp_length = sizeof(intel_keymaster_firmware_rsp_t);
        break;
    case ANDROID_HECI_AGENT_RESPONSE_CODE_INVALID_PARAMS:
        caller_rsp->result = KEYMASTER_RESULT_INVALID_INPUT;
        caller_rsp->rsp_result_and_data_length = sizeof(intel_keymaster_result_t);
        *rsp_length = sizeof(intel_keymaster_firmware_rsp_t);
        break;
    case ANDROID_HECI_AGENT_RESPONSE_CODE_NOT_SUPPORTED:
        caller_rsp->result = KEYMASTER_RESULT_NOT_SUPPORTED;
        caller_rsp->rsp_result_and_data_length = sizeof(intel_keymaster_result_t);
        *rsp_length = sizeof(intel_keymaster_firmware_rsp_t);
        break;
    case ANDROID_HECI_AGENT_RESPONSE_CODE_UNKNOWN_CMD:
        caller_rsp->result = KEYMASTER_RESULT_FAILURE;
        caller_rsp->rsp_result_and_data_length = sizeof(intel_keymaster_result_t);
        *rsp_length = sizeof(intel_keymaster_firmware_rsp_t);
        break;
    case ANDROID_HECI_AGENT_RESPONSE_INVALID_MSG_FORMAT:
        caller_rsp->result = KEYMASTER_RESULT_INVALID_INPUT;
        caller_rsp->rsp_result_and_data_length = sizeof(intel_keymaster_result_t);
        *rsp_length = sizeof(intel_keymaster_firmware_rsp_t);
        break;
    default:
        caller_rsp->result = KEYMASTER_RESULT_FAILURE;
        caller_rsp->rsp_result_and_data_length = sizeof(intel_keymaster_result_t);
        *rsp_length = sizeof(intel_keymaster_firmware_rsp_t);
        break;
    }

    result = SEP_KEYMASTER_SUCCESS;

    exit: return result;
}

sep_keymaster_return_t generate_cmd_buf(
        intel_keymaster_firmware_cmd_t * lib_cmd, uint8_t ** request,
        uint32_t * fw_request_len) {
    sep_keymaster_return_t result = SEP_KEYMASTER_FAILURE;

    switch (lib_cmd->cmd_id) {
    case KEYMASTER_CMD_GENERATE_KEYPAIR: {
        *fw_request_len = sizeof(ANDROID_HECI_KEYMASTER_CMD_RSA_GEN_KEY_REQUEST)
                + key_opaque_size;

        //Allocate request buffer
        ANDROID_HECI_KEYMASTER_CMD_RSA_GEN_KEY_REQUEST *request_cmd =
                (ANDROID_HECI_KEYMASTER_CMD_RSA_GEN_KEY_REQUEST *) malloc(
                        *fw_request_len);
        if (!request_cmd) {
            result = SEP_KEYMASTER_OUT_OF_MEMORY;
            goto exit;
        }
        *request = (uint8_t *) request_cmd;
        memset(*request, 0, *fw_request_len);

        request_cmd->Header.CmdClass = ANDROID_HECI_AGENT_CMD_CLASS_KEY_MASTER;
        request_cmd->Header.CmdId = ANDROID_HECI_KEYMASTER_CMD_ID_RSA_GEN_KEY;
        result = generate_keypair_cmd_buf(lib_cmd,
                (ANDROID_HECI_KEYMASTER_CMD_RSA_GEN_KEY_REQUEST *) request_cmd);
        if (result != SEP_KEYMASTER_SUCCESS) {
            LOGERR("Generating command buffer for keypair failed. Bailing..\n");
            goto exit;
        }
    }
        break;

    case KEYMASTER_CMD_IMPORT_KEYPAIR: {
        *fw_request_len =
                sizeof(ANDROID_HECI_KEYMASTER_CMD_RSA_IMPORT_KEY_REQUEST);

        //Allocate request buffer
        ANDROID_HECI_KEYMASTER_CMD_RSA_IMPORT_KEY_REQUEST *request_cmd =
                (ANDROID_HECI_KEYMASTER_CMD_RSA_IMPORT_KEY_REQUEST *) malloc(
                        *fw_request_len);
        if (!request_cmd) {
            result = SEP_KEYMASTER_OUT_OF_MEMORY;
            goto exit;
        }
        *request = (uint8_t *) request_cmd;
        memset(*request, 0, *fw_request_len);

        request_cmd->Header.CmdClass = ANDROID_HECI_AGENT_CMD_CLASS_KEY_MASTER;
        request_cmd->Header.CmdId =
                ANDROID_HECI_KEYMASTER_CMD_ID_RSA_IMPORT_KEY;
        result =
                import_keypair_cmd_buf(lib_cmd,
                        (ANDROID_HECI_KEYMASTER_CMD_RSA_IMPORT_KEY_REQUEST *) request_cmd);
        if (result != SEP_KEYMASTER_SUCCESS) {
            LOGERR(
                    "Generating command buffer for importing keypair failed. Bailing..\n");
            goto exit;
        }
    }
        break;

    case KEYMASTER_CMD_GET_KEYPAIR_PUBLIC: {

        *fw_request_len =
                sizeof(ANDROID_HECI_KEYMASTER_CMD_RSA_GET_PUBLIC_KEY_REQUEST)
                        + key_opaque_size;

        //Allocate request buffer
        ANDROID_HECI_KEYMASTER_CMD_RSA_GET_PUBLIC_KEY_REQUEST *request_cmd =
                (ANDROID_HECI_KEYMASTER_CMD_RSA_GET_PUBLIC_KEY_REQUEST *) malloc(
                        *fw_request_len);
        if (!request_cmd) {
            result = SEP_KEYMASTER_OUT_OF_MEMORY;
            goto exit;
        }
        *request = (uint8_t *) request_cmd;
        memset(*request, 0, *fw_request_len);

        request_cmd->Header.CmdClass = ANDROID_HECI_AGENT_CMD_CLASS_KEY_MASTER;
        request_cmd->Header.CmdId =
                ANDROID_HECI_KEYMASTER_CMD_ID_RSA_GET_PUBLIC_KEY;
        result =
                get_public_keypair_cmd_buf(lib_cmd,
                        (ANDROID_HECI_KEYMASTER_CMD_RSA_GET_PUBLIC_KEY_REQUEST *) request_cmd);
        if (result != SEP_KEYMASTER_SUCCESS) {
            LOGERR(
                    "Generating command buffer for getting public key failed. Bailing..\n");
            goto exit;
        }
    }
        break;

    case KEYMASTER_CMD_SIGN_DATA: {
        *fw_request_len =
                sizeof(ANDROID_HECI_KEYMASTER_CMD_RSA_SIGN_DATA_NOPAD_REQUEST)
                        + key_opaque_size;

        ANDROID_HECI_KEYMASTER_CMD_RSA_SIGN_DATA_NOPAD_REQUEST *request_cmd =
                (ANDROID_HECI_KEYMASTER_CMD_RSA_SIGN_DATA_NOPAD_REQUEST *) malloc(
                        *fw_request_len);
        if (!request_cmd) {
            result = SEP_KEYMASTER_OUT_OF_MEMORY;
            goto exit;
        }
        *request = (uint8_t *) request_cmd;
        memset(*request, 0, *fw_request_len);

        request_cmd->Header.CmdClass = ANDROID_HECI_AGENT_CMD_CLASS_KEY_MASTER;
        request_cmd->Header.CmdId =
                ANDROID_HECI_KEYMASTER_CMD_ID_RSA_SIGN_DATA_NOPAD;
        result =
                sign_data_cmd_buf(lib_cmd,
                        (ANDROID_HECI_KEYMASTER_CMD_RSA_SIGN_DATA_NOPAD_REQUEST *) request_cmd);
        if (result != SEP_KEYMASTER_SUCCESS) {
            LOGERR(
                    "Generating command buffer for signing data failed. Bailing..\n");
            goto exit;
        }
    }
        break;

    case KEYMASTER_CMD_VERIFY_DATA: {
        *fw_request_len =
                sizeof(ANDROID_HECI_KEYMASTER_CMD_RSA_VERIFY_DATA_NOPAD_REQUEST)
                        + key_opaque_size;

        ANDROID_HECI_KEYMASTER_CMD_RSA_VERIFY_DATA_NOPAD_REQUEST *request_cmd =
                (ANDROID_HECI_KEYMASTER_CMD_RSA_VERIFY_DATA_NOPAD_REQUEST *) malloc(
                        *fw_request_len);
        if (!request_cmd) {
            result = SEP_KEYMASTER_OUT_OF_MEMORY;
            goto exit;
        }
        *request = (uint8_t *) request_cmd;
        memset(*request, 0, *fw_request_len);

        request_cmd->Header.CmdClass = ANDROID_HECI_AGENT_CMD_CLASS_KEY_MASTER;
        request_cmd->Header.CmdId =
                ANDROID_HECI_KEYMASTER_CMD_ID_RSA_VERIFY_DATA_NOPAD;
        result =
                verify_data_cmd_buf(lib_cmd,
                        (ANDROID_HECI_KEYMASTER_CMD_RSA_VERIFY_DATA_NOPAD_REQUEST *) request_cmd);
        if (result != SEP_KEYMASTER_SUCCESS) {
            LOGERR(
                    "Generating command buffer for verifying data failed. Bailing..\n");
            goto exit;
        }
    }
        break;

    default:
        LOGERR("Unknown command id\n");
        result = SEP_KEYMASTER_FAILURE;
        goto exit;
    }

    result = SEP_KEYMASTER_SUCCESS;

    exit: return result;
}

sep_keymaster_return_t get_caps() {
    sep_keymaster_return_t result = SEP_KEYMASTER_FAILURE;
    ANDROID_HECI_KEYMASTER_CMD_GET_CAPS_REQUEST req;
    ANDROID_HECI_KEYMASTER_CMD_GET_CAPS_RESPONSE *resp = NULL;
    uint32_t i = 0;

    resp = malloc(ANDROID_HECI_AGENT_MAX_MTU);
    if (!resp) {
        result = SEP_KEYMASTER_OUT_OF_MEMORY;
        goto exit;
    }
    memset(resp, 0, ANDROID_HECI_AGENT_MAX_MTU);

    req.Header.CmdClass = ANDROID_HECI_AGENT_CMD_CLASS_KEY_MASTER;
    req.Header.CmdId = ANDROID_HECI_KEYMASTER_CMD_ID_GET_CAPS;
    req.Header.InputSize = 0;

    result = send_req_to_fw((uint8_t *) &req, sizeof(req), (uint8_t *) resp,
            ANDROID_HECI_AGENT_MAX_MTU);
    if (result != SEP_KEYMASTER_SUCCESS)
        goto exit;

    if (resp->Header.ResponseCode != ANDROID_HECI_AGENT_RESPONSE_CODE_SUCCESS) {
        LOGERR("FW failure while getting capabilities");
        result = SEP_KEYMASTER_FAILURE;
        goto exit;
    }

    ANDROID_HECI_KEYMASTER_KEY_CAPS *key_caps = resp->KeyCapabilities;
    for (i = 0; i < resp->NumAlgs; i++) {
        if (key_caps->Type == ANDROID_HECI_KEYMASTER_KEY_TYPE_RSA) {
            ANDROID_HECI_KEYMASTER_RSA_CAPS_PARAMS *params =
                    (ANDROID_HECI_KEYMASTER_RSA_CAPS_PARAMS *) key_caps->Value;
            key_opaque_size = params->KeyOpaqueSize;
            LOGERR("Key opaque size is %u", key_opaque_size);
            result = SEP_KEYMASTER_SUCCESS;
        }
    }

    exit: if (resp) {
        free(resp);
        resp = NULL;
    }
    return result;
}

sep_keymaster_return_t send_req_to_fw(const uint8_t * req,
        const uint32_t req_len, const uint8_t * resp, const uint32_t resp_len) {
    sep_keymaster_return_t result = SEP_KEYMASTER_FAILURE;
    int Status = -1;
    MEI_HANDLE *mei_handle = NULL;

    //Connect to the TXEI driver
    mei_handle = mei_connect(&ANDROID_HECI_AGENT_GUID);
    if (!mei_handle) {
        LOGERR("mei_connect failed\n");
        result = SEP_KEYMASTER_HECI_CONNECT_FAILED;
        goto exit;
    }
    //Send data to the FWout/target/product/byt_t_ffrd8/system/lib
    Status = mei_snd_rcv(mei_handle, (void *) req, (uint32_t) req_len,
            (void *) resp, (uint32_t) resp_len);
    if (Status < 0) {
        LOGERR("mei_snd_rcv failed\n");
        result = SEP_KEYMASTER_HECI_SNDRCV_FAILED;
        goto exit;
    }

    result = SEP_KEYMASTER_SUCCESS;

    exit: if (mei_handle) {
        mei_disconnect(mei_handle); //TODO: What happens if disconnect fails? Memory leak?
        mei_handle = NULL;
    }
    return result;
}

/**
 * Send a keymaster command to sep, and waits for a response
 * @param cmd_buffer :       Pointer to command buffer
 * @param buffer_length :    Number of command bytes
 * @param rsp_buffer :       Pointer to response buffer
 * @param rsp_length :       Upon input, size of response buffer
 *                          Upon return, actual size of response
 * @return SEP_KEYMASTER_CMD_SUCCESS if command was sent and
 * response received; non zero value if there was a failure
 */
sep_keymaster_return_t sep_keymaster_send_cmd(const uint8_t * cmd_buffer,
        uint32_t cmd_length, uint8_t * rsp_buffer, uint32_t * rsp_length) {
    sep_keymaster_return_t result = SEP_KEYMASTER_FAILURE;

    uint8_t *request = NULL;
    uint8_t *response = NULL;
    uint32_t fw_request_len = 0;

    MEI_HANDLE *mei_handle = NULL;

    //Redirect logger output to logcat
    txei_log_set_dest(TXEI_LOG_DEST_ANDROID, NULL, NULL);

    //Validate input parameters
    if (!(cmd_buffer != NULL && rsp_buffer != NULL && rsp_length != NULL)) {
        result = SEP_KEYMASTER_BAD_PARAMETER;
        goto exit;
    }
    //Command buffer should contain at least a command id and length field
    if (cmd_length < sizeof(intel_keymaster_firmware_cmd_t)) {
        result = SEP_KEYMASTER_BAD_PARAMETER;
        goto exit;
    }
    //Response buffer should at least have intel_keymaster_firmware_rsp_t bytes
    if (*rsp_length < sizeof(intel_keymaster_firmware_rsp_t)) {
        result = SEP_KEYMASTER_RSP_BUFFER_TOO_SMALL;
        goto exit;
    }
    //If capabilities have not been obtained before, do so now
    if (!caps_obtained) {
        result = get_caps();
        if (result != SEP_KEYMASTER_SUCCESS) {
            LOGERR("Obtaining capabilites failed. Bailing");
            goto exit;
        }
        caps_obtained = 1;
    }
#define LOGGING
#ifdef LOGGING
    {
#define KEYMASTER_NUM_CMDS  8
        char *cmd_name[KEYMASTER_NUM_CMDS] = { "Invalid command",
                "KEYMASTER_CMD_GENERATE_KEYPAIR",
                "KEYMASTER_CMD_IMPORT_KEYPAIR",
                "KEYMASTER_CMD_GET_KEYPAIR_PUBLIC",
                "KEYMASTER_CMD_DELETE_KEYPAIR", "KEYMASTER_CMD_DELETE_ALL",
                "KEYMASTER_CMD_SIGN_DATA", "KEYMASTER_CMD_VERIFY_DATA" };

        if (*(uint32_t *) cmd_buffer < KEYMASTER_NUM_CMDS) {
            LOGINFO("%s", cmd_name[*(uint32_t *) cmd_buffer]);
        } else {
            LOGERR("unknown command");
        }
    }
#endif

    //Translate the command buffer
    intel_keymaster_firmware_cmd_t *lib_cmd =
            (intel_keymaster_firmware_cmd_t *) cmd_buffer;

    result = generate_cmd_buf(lib_cmd, &request, &fw_request_len);
    if (result != SEP_KEYMASTER_SUCCESS) {
        LOGERR("Generating command buffer failed");
        goto exit;
    }

    if (fw_request_len > MAX_HECI_MSG_SIZE) {
        LOGERR("Command buffer is too big\n");
        result = SEP_KEYMASTER_CMD_BUFFER_TOO_BIG;
        goto exit;
    }
    //Allocate the response buffer
    //Caller allocated a response buffer that includes a header (= sizeof(intel_keymaster_firmware_rsp_t)) and data (= *rsp_length - sizeof(header))
    //So the total data we can pass back is approx. (*rsp_length - sizeof(header)) bytes long
    uint32_t fw_response_length = sizeof(ANDROID_HECI_AGENT_RESP_HEADER)
            + *rsp_length - sizeof(intel_keymaster_firmware_rsp_t); //TODO: Is this check sufficient?
    response = (uint8_t *) malloc(fw_response_length);
    if (!response) {
        result = SEP_KEYMASTER_OUT_OF_MEMORY;
        goto exit;
    }
    memset(response, 0, fw_response_length);

    //print_buf("Request Data", request, ((ANDROID_HECI_AGENT_REQ_HEADER *)request)->InputSize + sizeof(ANDROID_HECI_AGENT_REQ_HEADER));

    result = send_req_to_fw(request, fw_request_len, response,
            fw_response_length);
    if (result != SEP_KEYMASTER_SUCCESS) {
        LOGERR("Failed to send request to the MEI driver");
        goto exit;
    }
    //print_buf("Response Data", response, ((ANDROID_HECI_AGENT_RESP_HEADER *)response)->OutputSize + sizeof(ANDROID_HECI_AGENT_RESP_HEADER));

    //For now cast response to type ANDROID_HECI_AGENT_RESP_HEADER to get the value of fields in the header
    ANDROID_HECI_AGENT_RESP_HEADER *fw_rsp_hdr =
            (ANDROID_HECI_AGENT_RESP_HEADER *) response;

    //Set the correct command id. We have to do this irrespective of the status of the command.
    intel_keymaster_firmware_rsp_t *firmware_rsp =
            (intel_keymaster_firmware_rsp_t *) rsp_buffer;
    result = set_response_cmd_id(fw_rsp_hdr->ResponseCode,
            &firmware_rsp->rsp_id);
    if (result != SEP_KEYMASTER_SUCCESS) {
        LOGERR("Setting the response command id failed\n");
        goto exit;
    }

    //Set the status of the command and length in the response to the caller.
    //If it was successful, additionally return the data
    //Also set response length
    result = set_response_status_and_data(response, firmware_rsp, rsp_length,
            *rsp_length);
    if (result != SEP_KEYMASTER_SUCCESS) {
        LOGERR("Setting the status and data failed\n");
        goto exit;
    }
    //Finally, everything looks good
    result = SEP_KEYMASTER_SUCCESS;

    exit: if (request) {
        free(request);
        request = NULL;
    }

    if (response) {
        free(response);
        response = NULL;
    }

    return result;
}
