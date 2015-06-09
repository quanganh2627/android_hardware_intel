/*************************************************************************
 **     Copyright (c) 2012 Intel Corporation. All rights reserved.      **
 **                                                                     **
 ** This Software is licensed pursuant to the terms of the INTEL        **
 ** MOBILE COMPUTING PLATFORM SOFTWARE LIMITED LICENSE AGREEMENT        **
 ** (OEM / IHV / ISV Distribution & End User)                           **
 **                                                                     **
 ** The above copyright notice and this permission notice shall be      **
 ** included in all copies or substantial portions of the Software.     **
 ** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,     **
 ** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF  **
 ** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND               **
 ** NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS **
 ** BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN  **
 ** ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN   **
 ** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE    **
 ** SOFTWARE.                                                           **
 **                                                                     **
 *************************************************************************/
package com.intel.security.service;

/**
 * System-private API for commicating with Chaabi.
 *
 * A serive can only have a single interface (AIDL) so this file will
 * contain all the API for all libraries that access Chaabi and are
 * linked to the SEPService.  If a service can support multiple
 * interfaces this file can be split up.
 *
 * {@hide}
 */
interface ISEPService {

    /*
     * DRM Library Initialization
     * Description:
     * 	Initializes the SEP driver for DRM library use.
     *
     *      Return value of DRM_SUCCESSFUL means the SEP driver was initialized.
     */
    void initDrmLibrary();
    /**
     * Test for conneciton to Chaabi.
     */
    long getRandomNumber();
    /** TODO: Add
     * sec_result_t Drm_GetRandom( uint8_t * const pRandom, const
     * uint32_t bufLen )
     */


    /** TODO: add the drm-lib.c APIs */


    /**
     * Initialize internal dependencies. Should be called first and only once
     * after loading the iha library. \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param	none
     *
     * @return	void
     *****************************************************************************/
    void IHAInit();

    /**
     * Deinitialize IHA library. Should be called before unloading the DLL. \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param	none
     *
     * @return	void
     *****************************************************************************/
    void IHADeInit();

    /**
     * Initiate a provisioning session. This call will initialize provisioning
     * in the embedded app, and the session handle returned must be used in
     * subsequent provisioning-related calls. \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the 
     *					chipset.
     *
     * @return			Session handle.
     *****************************************************************************/
    int IHAStartProvisioning( in String strAppName);

    /**
     * Once provisioning is complete, this function must be called to ensure 
     * state is cleaned up, and to obtain the encrypted Token Record from the 
     * embedded app. The encrypted Token Record that is received must be stored
     * by the application, and supplied again in a call such as IHA_GetOTP(). \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the 
     *					chipset.
     * @param [in]		iSessionHandle: SessionHandle identifying a provisioning
     *					sequence. 
     * @param [in, out]	saExpectedDataLen: Short 1 byte array holding the expected
     *					length of the output encrypted token record.  This field 
     *					holds the actual length of the data returned if expected 
     *					length is sufficient, and required length if expected 
     *					length is insufficient.
     *
     * @return			Byte array containing the encrypted token record.
     *****************************************************************************/
    byte[] IHAEndProvisioning( in String strAppName,
			       in int iSessionHandle,
			       inout int[] /*short[]*/ saExpectedDataLen);


    /**
     * This function is used to send data to the embedded app in the FW. \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the 
     *					chipset.
     * @param [in]		iSessionHandle: Session handle identifying a provisioning
     *					sequence. Optional.
     * @param [in]		sDataType: Type of data being sent.
     * @param [in]		baInData: Input data bugger to be sent to the embedded app
     *					in the FW.
     *
     * @return			void
     *****************************************************************************/
    void IHASendData( in String strAppName, 
		      in int iSessionHandle, 
		      in int /*short*/ sDataType, 
		      in byte[] baInData);
								

    /**
     * This function is used to obtain data from the embedded app in the FW. \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the 
     *					chipset.
     * @param [in]		iSessionHandle: Session handle identifying a provisioning
     *					sequence. Optional.
     * @param [in]		sDataType: Type of data being requested.
     * @param [in, out]	saExpectedDataLen: Short 1 byte array holding the expected
     *					length of the output data.  This field holds the actual 
     *					length of the data returned if expected length is 
     *					sufficient, and required length if expected length is 
     *					insufficient. 
     *
     * @return			Byte array containing the data received from the FW.
     *****************************************************************************/
    byte[] IHAReceiveData( in String strAppName, 
			   in int iSessionHandle,
			   in int /*short*/ sDataType,
			   in int[] /*short[]*/ saExpectedDataLen);
								

    /**
     * This function is used to send IPT-specific provisioning data to the 
     * embedded app in the FW. \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the 
     *					chipset.
     * @param [in]		iSessionHandle: Session handle identifying a provisioning
     *					sequence. 
     * @param [in]		baInData: Byte array of data to be sent to the FW.
     *
     * @return			void
     *****************************************************************************/
    void IHAProcessSVPMessage( in String strAppName, 
			       in int iSessionHandle, 
			       in byte[] baInData);
								
								
    /**
     * This function is used to obtain IPT-specific provisioning data from the 
     * embedded app in the FW. \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the 
     *					chipset.
     * @param [in]		iSessionHandle: Session handle identifying a provisioning
     *					sequence. 
     * @param [in, out]	saExpectedDataLen: Short 1 byte array holding the expected 
     *					length of the output data.  This field holds the actual 
     *					length of the data returned if expected length is 
     *					sufficient, and required length if expected length is 
     *					insufficient.
     *
     * @return			Byte array containing the data returned from the FW.
     *****************************************************************************/
    byte[] IHAGetSVPMessage( in String strAppName, 
			     in int iSessionHandle,
			     inout int[] /*short[]*/ saExpectedDataLen);


    /**
     * This function is used to send and receive data to and from the embedded app 
     * in the FW. \n
     * This API was added in IPT 2.0, and is only to be used by those apps that do
     * not need to run on 2011 platforms. \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the 
     *					chipset.
     * @param [in]		iSessionHandle: Session handle identifying a provisioning
     *					sequence. Optional.
     * @param [in]		sDataType: Type of action that is being requested.
     * @param [in]		baInData: Input data bugger to be sent to the embedded app
     *					in the FW.
     * @param [in, out]	saExpectedOutDataLen: Short 1 byte array holding the 
     *					expected length of the output data.  This field holds the 
     *					actual length of the data returned if expected length is 
     *					sufficient, and required length if expected length is 
     *					insufficient.
     *
     * @return			Byte array containing the data returned from the FW.
     *****************************************************************************/
    byte[] IHASendAndReceiveData( in String strAppName, 
				  in int iSessionHandle,
				  in int /*short*/ sDataType,
				  in byte[] baInData,
				  inout int[] /*short[]*/ saExpectedOutDataLen);
								
								
    /**
     * This function is used to obtain capabilities of embedded app, including 
     * version information. \n
     * This API was added in IPT 2.0, and is only to be used by those apps that do
     * not need to run on 2011 platforms. \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the 
     *					chipset.
     * @param [in]		sType: Type of capability information requested. Can be: \n
     1: Embedded App Version \n
     2: Embedded App Security Version.
     * @param [in, out]	saExpectedDataLen: Short 1 byte array holding the 
     *					expected length of the output data.  This field holds the 
     *					actual length of the data returned if expected length is 
     *					sufficient, and required length if expected length is 
     *					insufficient.
     *					For sType 1: Output is a string containing the version.
     *						saExpectedDataLen[0] should be set to 6.
     *					For sType 2. Output is a string containing the version.
     *						saExpectedDataLen[0]should be set to 6.
     *
     * @return			Byte array containing the data returned from the FW.
     *****************************************************************************/
    byte[] IHAGetCapabilities( in String strAppName,
			       in int /*short*/ sType,
			       inout int[] /*short[]*/ saExpectedDataLen);
								
								
    /**
     * This function is used to retrieve the version of the IHA DLL. \n
     * This API was added in IPT 2.0, and is only to be used by those apps that do
     * not need to run on 2011 platforms. \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param 	none
     *
     * @return	Version of the IHA DLL. Please see the IHA C interface 
     *			documentation for details on the parsing this int.
     *****************************************************************************/
    int IHAGetVersion();


    /**
     * This function is used to load the embedded app into the FW. This is 
     * typically called the very first time the embedded app is used, if it has not
     * been loaded yet. It is also used every time the embedded app needs to be 
     * updated. \n
     * This API was added in IPT 2.0, and is only to be used by those apps that do
     * not need to run on 2011 platforms. \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the 
     *					chipset.
     * @param [in]		strSrcFile: Unicode string containing the full path, 
     *					including filename of the embedded app that needs to be 
     *					installed in the chipset.  Once this command returns 
     *					successfully, the folder and file that this path points to 
     *					can be deleted.
     *
     * @return			void
     *****************************************************************************/
    void IHAInstall( in String strAppName,
		     in String strSrcFile);
								
				
    /**
     * This function is used to unload the embedded app from the FW. \n
     * This API was added in IPT 2.0, and is only to be used by those apps that do
     * not need to run on 2011 platforms. \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the 
     *					chipset.
     *
     * @return			void
     *****************************************************************************/
    void IHAUninstall( in String strAppName);
								
		
    /**
     * This function is used to trigger a FW update when an EPID group key 
     * revocation is detected. \n
     * This API was added in IPT 2.0, and is only to be used by those apps that do
     * not need to run on 2011 platforms. \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param 	none
     *
     * @return	void
     *****************************************************************************/
    void IHADoFWUpdate();


    /**
     * This function is used to retrieve an OTP from the embedded OTP App. \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the 
     *					chipset.
     * @param [in]		iSessionHandle: Session handle identifying a provisioning
     *					sequence. Must be 0 if called post-provisioning. If a valid 
     *					session handle is passed in, then this is to be used in the 
     *					context of a provisioning sequence, and no encrypted Token 
     *					should be passed in.
     * @param [in]		baEncToken: Byte array containing the encrypted Token.  
     *					Must be NULL if session handle is used.
     * @param [in]		baVendorData: Byte array containing the vendor data.  
     *					Optional.
     * @param [in, out]	saExpectedOtpLength: Short 1 byte array holding the 
     *					expected length of the OTP.  This field holds the actual 
     *					length of the data returned if the call is successful, 
     *					and required length if expected length is insufficient.  
     * @param [in, out]	saExpectedEncTokenLength: Short 1 byte array holding the 
     *					expected length of the encrypted token record. This is 
     *					optional; if the caller does not require the encrypted 
     *					token to be sent back, this 1st byte can be set to 0.  If 
     * 					it isset, and if this buffer is insufficient, the call will 
     *					return with an error indicating the same and this field 
     *					will specify the required buffer length.  The function will 
     *					need to be called again with the correct buffer size.  This 
     *					field must be 0 if session handle is used.
     *
     * @return			Byte array containing the data returned by the FW.  The 
     *					first part contains the OTP; if saOutEncTokenLength[0] is 
     *					greater than 0, the rest of the data in this buffer is the 
     *					output encrypted token.
     *****************************************************************************/
    byte[] IHAGetOTP(  in String strAppName,
		       in int iSessionHandle,
		       in byte[] baEncToken,
		       in byte[] baVendorData,
		       inout int[] /*short[]*/ saExpectedOtpLength,
		       inout int[] /*short[]*/ saExpectedEncTokenLength);								
			
    /**
     * This function is used to obtain status from the embedded app. \n
     * This is a legacy API, kept for backward compatibility with version 1.x. 
     * Should not be used from version 2.0 onwards. \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the 
     *					chipset.
     * @param [in]		iSessionHandle: Session handle identifying a provisioning
     *					sequence. Can be NULL if session-specific status is not 
     *					required.
     * @param [in]		sType: Type of status requested. 
     *
     * @return			Status returned by the FW.
     *****************************************************************************/
    int IHAGetOTPSStatus( in String strAppName,
			  in int iSessionHandle,
			  in int /* short*/ sType);
								
								
    /**
     * This function is used to obtain capabilities of embedded app, including 
     * version information. \n
     * This is a legacy API, kept for backward compatibility with version 1.x.
     * Has been replaced by IHA_GetCapabilities() in version 2.0 onwards. \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the 
     *					chipset.
     * @param [in]		sType: Type of capability information requested. Can be: \n
     1: Embedded App Version \n
     2: Embedded App Security Version.
     * @param [in, out]	saExpectedDataLen: Short 1 byte array holding the 
     *					expected length of the output data.  This field holds the 
     *					actual length of the data returned if expected length is 
     *					sufficient, and required length if expected length is 
     *					insufficient.
     *					For sType 1: Output is a string containing the version.
     *						saExpectedDataLen[0] should be set to 6.
     *					For sType 2. Output is a string containing the version.
     *						saExpectedDataLen[0]should be set to 6.
     *
     * @return			Byte array containing the data returned from the FW.
     *****************************************************************************/
    byte[] IHAGetOTPCapabilities( in String strAppName,
				  in int /*short*/ sType,
				  inout int[] /*short[]*/ saExpectedDataLen);
								
								
    /**
     * This function is used to load the embedded app into the FW. This is 
     * typically called the very first time the embedded app is used, if it has not
     * been loaded yet. It is also used every time the embedded app needs to be 
     * updated. \n
     * This is a legacy API, kept for backward compatibility with version 1.x.
     * Has been replaced by IHA_GetCapabilities() in version 2.0 onwards.  \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the 
     *					chipset.
     * @param [in]		strSrcFile: Unicode string containing the full path, 
     *					including filename of the embedded app that needs to be 
     *					installed in the chipset.  Once this command returns 
     *					successfully, the folder and file that this path points to 
     *					can be deleted.
     *
     * @return			void
     *****************************************************************************/
    void IHAInstallOTPS( in String strAppName,
			 in String strSrcFile);
								
								
    /**
     * This function is used to unload the embedded app from the FW. \n
     * This is a legacy API, kept for backward compatibility with version 1.x.
     * Has been replaced by IHA_GetCapabilities() in version 2.0 onwards.  \n
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the 
     *					chipset.
     *
     * @return			void
     *****************************************************************************/
    void IHAUninstallOTPS( in String strAppName);

    /*****************************************************************************
     * This function is used retrieve the embedded app's instance id. \n
     * This API was added in IPT 2.1, and is meant to be used by client applications
     * that need to reuse a non-shared embedded app instance id between multiple
     * IPT dll-s. Non-shared app instances are only created when the 
     * IHA_RegisterEventCb() API is used to register an event callback with the app.
     *
     * @param [in]		strAppName: String identifying the embedded app in the 
     *					chipset.
     *
     * @return			Instance Id to use for communication with embedded app
     *****************************************************************************/
    long IHA_GetAppInstId( in String strAppName );
 /*****************************************************************************
     * This function is used retrieve the last error occured in IHA library 
     *
     * @return          Error value in integer 
     *****************************************************************************/
	
	int IHAGetLastError();
}
