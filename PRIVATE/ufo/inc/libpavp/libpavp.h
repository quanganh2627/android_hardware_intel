/*===================== begin_copyright_notice ==================================

INTEL CONFIDENTIAL
Copyright 2013
Intel Corporation All Rights Reserved.

The source code contained or described herein and all documents related to the
source code ("Material") are owned by Intel Corporation or its suppliers or
licensors. Title to the Material remains with Intel Corporation or its suppliers
and licensors. The Material contains trade secrets and proprietary and confidential
information of Intel or its suppliers and licensors. The Material is protected by
worldwide copyright and trade secret laws and treaty provisions. No part of the
Material may be used, copied, reproduced, modified, published, uploaded, posted,
transmitted, distributed, or disclosed in any way without Intel's prior express
written permission.

No license under any patent, copyright, trade secret or other intellectual
property right is granted to or conferred upon you by disclosure or delivery
of the Materials, either expressly, by implication, inducement, estoppel or
otherwise. Any license under such intellectual property rights must be express
and approved by Intel in writing.

File Name: libpavp.h
Abstract: PAVP library for Android - header file for external use.

Environment: Linux/Android

Notes:  

======================= end_copyright_notice ==================================*/

#ifndef _LIBPAVP_H_
#define _LIBPAVP_H_

#ifndef _MSC_VER
#include <cstdint>
typedef uint8_t     UINT8;
typedef uint8_t     BYTE;
typedef uint16_t    UINT16;
typedef uint32_t    UINT32;
typedef uint32_t    ULONG;
typedef void*       PVOID;
typedef uint32_t*   PUINT;
typedef uint32_t    UINT;
typedef uint32_t    DWORD;
#endif

/// \def INTERFACE_VERSION
/// The interface version the library was compiled with.
#define INTERFACE_VERSION 0xb

class pavp_lib_session
{
public:

    /// \enum pavp_lib_session::pavp_lib_code
    /// Libpavp Error Codes
    /// @{
    typedef enum {
        status_ok                     = 0,
        invalid_arg                   = 1,
        session_already_initialized   = 2,
        session_uninitialized         = 3,
        insufficient_memory           = 4,
        internal_error                = 5,
        not_implemented               = 6,
        pavp_not_supported            = 7,
        hdcp1_not_supported           = 8,
        hdcp2_not_supported           = 9,
        hdcp2_timeout                 = 10,
        operation_failed              = 11,
        api_version_mismatch          = 12,
        signature_verify_failed       = 13,
        pavp_lib_status_count         = 14,
    } pavp_lib_code;
    ///@}

    /// \enum pavp_lib_session::hdcp2_content_stream_type
    /// HDCP2 Stream Types
    /// @{
    typedef enum {
        content_stream_type0     = 0,   //!< May be transmitted by the HDCP repeater to all HDCP devices.
        content_stream_type1     = 1,   //!< Must not be transmitted by the HDCP repeater to HDCP 1.x-compliant devices and HDCP 2.0-compliant repeaters.
        max_content_stream_types = 2
    } hdcp2_content_stream_type;
    ///@}

    /// \enum pavp_lib_session::key_type
    /// PAVP Key Types
    /// @{
    typedef enum
    {
        pavp_key_decrypt    = 1,    //!< Decrypt key.
        pavp_key_encrypt    = 2,    //!< Encrypt key.
        pavp_key_both       = 4,    //!< Decrypt and encrypt key.
    } key_type;
    ///@}

    /// \typedef pavp_lib_session::pavp_lib_version
    /// Libpavp Version Number
    typedef UINT32 pavp_lib_version;

    /// \enum pavp_lib_session::hdcp_capability
    /// HDCP capability of host and any connected display
    /// Adapted directly from Widevine MDRM Security Integration Guide for CENC
    /// @{
    typedef enum {
        HDCP_CAP_NO_HDCP_NO_SECURE_DATA_PATH = 0x0,  //!< HDCP is unsupported, local display is not secure
        HDCP_CAP_HDCP_10                     = 0x1,  //!< HDCP1.x is supported by host and display
        HDCP_CAP_HDCP_20                     = 0x2,  //!< HDCP2.0 is supported by host and display
        HDCP_CAP_HDCP_21                     = 0x3,  //!< HDCP2.1 is supported by host and display
        HDCP_CAP_HDCP_22                     = 0X4,  //!< HDCP2.2 is supported by host and display
        HDCP_CAP_NO_HDCP_SECURE_DATA_PATH    = 0xff, //!< HDCP is not supported, local display is secure
    } hdcp_capability_type;
    ///@}

    /// \typedef pavp_lib_session::PFNWIDIAGENT_HDCPSENDMESSAGE
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       A callback function used to send data to the wireless receiver.
    /// \param       hWiDiHandle    [in] Not used in Android, reserved.
    /// \param       pData          [in] a caller-allocated input buffer containing an HDCP2 message.
    /// \param       uiDataLen      [in] The size of the data in the buffer pointed to by pData. 
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    typedef pavp_lib_code (*PFNWIDIAGENT_HDCPSENDMESSAGE)
    (
        PVOID    hWiDiHandle,
        PVOID    pData,
        UINT32   uiDataLen
    );

    /// \typedef pavp_lib_session::PFNWIDIAGENT_HDCPRECEIVEMESSAGE
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       A callback function used to receive data from the wireless receiver.
    /// \param       hWiDiHandle        [in] Not used in Android, reserved.
    /// \param       pData              [in/out] a caller-allocated input buffer to be filled with the received HDCP2 message.
    /// \param       uiDataLen          [in] The size of the buffer pointed to by pData. 
    /// \param       pReceivedDataLen   [out] If pData was null, returns size required to receive the next message.
    ///                                       If pData was not null, returns the size of the data written to pData.
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    typedef pavp_lib_code (*PFNWIDIAGENT_HDCPRECEIVEMESSAGE)
    (
        PVOID   hWiDiHandle,
        PVOID   pData,
        UINT32  uiDataLen,
        PUINT   pReceivedDataLen
    );

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Initializes the libary.
    /// \param       interface_version          [in] Should be set to INTERFACE_VERSION.
    /// \param       pavp_lib_session_instance  [out] The library session object to be used for using this library.
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    static pavp_lib_code pavp_lib_init(
        pavp_lib_version    interface_version,
        pavp_lib_session    **pavp_lib_session_instance);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Destroy resources allocated for the libary use. Must be called if a successful pavp_lib_init call has been made.
    /// \param       pavp_lib_session_instance  [in] The library session object to be destroyed.
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    static pavp_lib_code pavp_lib_cleanup(pavp_lib_session *pavp_lib_session_instance);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Allows direct communication with the SEC FW. This function can be used for DRM initialization and key exchange.
    /// \par         Details:
    /// \li          This function can be called any time after pavp_lib_init().
    /// \param       pInput     [in] The SEC command to be sent.
    /// \param       ulInSize   [in] The size of the SEC command to be sent.
    /// \param       pOutput    [out] A buffer to contain the SEC response.
    /// \param       ulOutSize  [in] The SEC response buffer size.
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code sec_pass_through(
        BYTE* pInput,
        ULONG ulInSize,
        BYTE* pOutput,
        ULONG ulOutSize) = 0;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Provides an HDCP System Renewability Message.
    /// \par         Details:
    /// \li          The SRM's digital signature will be verified and revoked KSVs will be extracted to check during activation of HDCP1.
    /// \li          Can be called when using either HDCP1 or HDCP2, but must be called before activation or authentication.
    /// \param       srm_data       [in] Binary data that contains revoked KSVs formatted according to the HDCP specification.
    /// \param       srm_data_size  [in] The size of the binary data.
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code hdcp_set_srm(UINT8 *srmData, UINT32 srmDataSize) = 0;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Returns the HDCP support of the host and any connected display.
    /// \par         This method conforms to the return values specified by the Widevine MDRM Security Integration Guide for CENC. 
    ///              See pavp_lib_session::hdcp_capability_type for possible values.
    /// \param       current [out]  current returns the HDCP capability successfully negotioated by the host to a display
    /// \param       maximum [out]  maximum returns the maximum supported HDCP capibility of the host
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code hdcp_get_capability(UINT8 *current, UINT8 *maximum) = 0;
    
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Enables HDCP1 and creates a PAVP session and initializes the required private variables.
    /// \par         Details:
    /// \li          This must be called when the host process needs to create a session for protected decoding.
    /// \li          If any digital outputs are enabled, this function will also attempt to activate HDCP1.x on them.
    /// \li          If this is called, hdcp2_create() will be unavailable until pavp_destroy_session() is called.
    /// \li          This call will fail if hdcp2_create() was already called (PAVP and HDCP2 library usages are mutually exclusive).
    /// \param       is_heavy_mode          [in, optional] Indicates whether the session to be created should be a heavy mode session (default: true).
    /// \param       is_transcode_session   [in, optional] Indicates whether the session to be created should be a transcode session (default: false).
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code pavp_create_session(
        bool    is_heavy_mode           = true,
        bool    is_transcode_session    = false) = 0;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Returns the app ID of the PAVP session created by the library.
    /// \par         Details:
    /// \li          This call fails if no PAVP session has been created.
    /// \li          If the call failed, app_id value is undefined.
    /// \param       app_id [out] The created PAVP session app ID.
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code pavp_get_app_id(UINT& app_id) = 0;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Releases the PAVP session and all resources allocated in pavp_create_session().
    /// \par         Details:
    /// \li          Can be called if for any reason PAVP needs to be recreated without re-creating a pavp_lib_session object.
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code pavp_destroy_session() = 0;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Reports whether the PAVP session is alive at the hardware level.
    /// \par         Details:
    /// \li          If the session is not alive, the host application should immediately halt playback, destroy the current session and create a new one.
    /// \param       bIsAlive   [out] If the returned status is status_ok, bIsAlive will be true if the PAVP session is alive, false otherwise.
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code pavp_is_session_alive(bool *bIsAlive) = 0;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Sets the steam key to be used with the previouly created PAVP session.
    /// \par         Details:
    /// \li          If the session is not alive, the host application should immediately halt playback, destroy the current session and create a new one.
    /// \param       key_type               [in] The key type to set (decrypt/encrypt/both).
    /// \param       encrypted_decrypt_key  [in] Encrpted decrypt key to set (ignored if the requested key type is encrypt only).
    /// \param       encrypted_encrypt_key  [in] Encrpted encrypt key to set (ignored if the requested key type is decrypt only).
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code pavp_set_stream_key(
        key_type    key_type,
        DWORD       encrypted_decrypt_key[4],
        DWORD       encrypted_encrypt_key[4]) = 0;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Queries the device if HDCP2 for WiDi is suppored.
    /// \return      hdcp2_supported if supported, hdcp2_not_supported if not supported.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code hdcp2_is_support_available() = 0;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Releases HDCP2 usage. Frees all resources associated with HDCP2.
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code hdcp2_destroy() = 0;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       An optional call for HDCP2.1+. Allows the player to set certain restrictions on the HDCP topology.
    /// \param       content_stream_type    [in] A stream type for the content being played.
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code hdcp2_set_stream_types(hdcp2_content_stream_type content_stream_type) = 0;
    
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Rerforms the full HDCP2.x AKE between the wireless adapter and Widi Transmitter (SEC).
    /// \par         Details:
    /// \li          The send and receive callbacks will not be cached and will only be used within the scope of this function call.
    /// \li          Therefore the caller does not need to guarantee the validity of the function pointers afterwards.
    /// \li          This call will fail if pavp_create_session() was already called (PAVP and HDCP2 library usages are mutually exclusive).
    /// \param       pSend      [in] A callback function to send a message to the wireless adapter.
    /// \param       pReceive   [in] A callback function to receive a response from the wireless adapter.
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code hdcp2_create_and_authenticate(
        PFNWIDIAGENT_HDCPSENDMESSAGE    pSend,
        PFNWIDIAGENT_HDCPRECEIVEMESSAGE pReceive) = 0;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Recreates hdcp2 session without hdcp re-authorization
    /// \par         Details:
    /// \li          This API to recreate hdcp session when the rest of the hdcp states are already authorized.
    ///              This is a mechanism for app to recover hdcp session retaining authetication states.
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code hdcp2_recreate() = 0;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Decrypts an encrypted byte array and blts data to a surface.
    ///
    /// \param       pIV    [in]  The IV + counter that was used to encrypt the surface, must be little
    ///                           endian
    /// \param       src    [in]  The source resource which contains the AES encrypted data. 
    /// \param       dest   [out] The Destination resource. This should be allocated by the caller.
    /// \param       size   [in]  The size of the src buffer.
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code decryption_blt(
        const DWORD(&iv)[4],
        const BYTE* const src, 
        BYTE* const dest,
        const size_t size) = 0;
    
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Encrypts an uncompressed clear surface, to pass back to the application. 
    ///              Adding this function "declaration but disabling definition" in release driver will remove qualms for running P4 release-internal 
    ///              driver on release OTC build. 
    /// \param       src_resource   [in] The source resource which contains the clear data. 
    /// \param       dst_Resource   [out] The Destination resource. This resource will contain the encrypted data. It should be allocated by the caller.
    /// \param       width  [in] The width of the surface in Bytes.
    /// \param       height [in] The height of the surface in Bytes (pay attention that for NV12 the height(Bytes) = 1.5*height(Pixel)).
    /// \return     status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code encryption_blt(
        BYTE*   src_resource,
        BYTE*   dst_Resource,
        DWORD   width,
        DWORD   height) = 0;

    //HSW Android WideVine Stuff
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Start HSW WideVine connection status heart beat message.
    /// \par         Details:
    /// \li          This function should be called after set entitlement key and before first video process frame.
    ///              Algorithm -
    ///              - App -
    ///              check_connection_status_heart_beat()
    ///              first_video_process_frame()
    ///              while playback
    ///                 for i = 1 to 100
    ///                     do nothing
    ///                 check_connection_status_heart_beat()
    ///              end while
    ///              - Driver -
    ///              Call Get WideVine Nonce (It is different from regular gent nonce)
    ///              Program Nonce to GPU and Get Connection Status
    ///              Verify Connection Status and return T/F if HDCP alive 
    ///              - Firmware -
    ///              Wait for Connection Status before Xscripting first Video Frame
    ///              Receive request for connection status and verify
    ///              Start counter till 100 frames
    ///                 Process video frames
    ///              if on 100th frame and connection status is not received then stop doing Xscripting 
    ///              
    /// \param       bStartCSHeartBeat  [out] 
    /// 
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code check_connection_status_heart_beat(
        bool        *bCSHeartBeatStatus) = 0;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Start HSW WideVine crypto init dma message.
    /// \par         Details:
    /// \li          This function should be called to get handle to DMA message.
    /// \param       pMessage  [in] 
    /// \param       msg_length  [in] 
    /// \param       handle  [out] 
    /// 
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code oem_crypto_init_dma(
        UINT8       *pMessage,
        UINT32      msg_length,
        UINT32      *handle) = 0;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// \brief       Start HSW WideVine crypto uninit dma message.
    /// \par         Details:
    /// \li          This function should be called to free handle to DMA message.
    /// \param       handle  [in]
    ///
    /// \return      status_ok on success, error codes on failure.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual pavp_lib_code oem_crypto_uninit_dma(
        UINT32      *handle) = 0;

protected:

    pavp_lib_session() { }    // It is never called. Needed for compilation.
    virtual ~pavp_lib_session();

private:

    // Default constructor, copy constructor and assignment operator should not be used.
    pavp_lib_session& operator=(const pavp_lib_session& other);
    pavp_lib_session(const pavp_lib_session& other);
        
};

#endif
