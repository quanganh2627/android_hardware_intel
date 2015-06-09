/*
********************************************************************************
**    Intel Architecture Group 
**    Copyright (C) 2009-2010 Intel Corporation  
********************************************************************************
**                                                                            
**    INTEL CONFIDENTIAL                                                      
**    This file, software, or program is supplied under the terms of a        
**    license agreement and/or nondisclosure agreement with Intel Corporation 
**    and may not be copied or disclosed except in accordance with the        
**    terms of that agreement.  This file, software, or program contains      
**    copyrighted material and/or trade secret information of Intel           
**    Corporation, and must be treated as such.  Intel reserves all rights    
**    in this material, except as the license agreement or nondisclosure      
**    agreement specifically indicate.                                        
**                                                                            
**    All rights reserved.  No part of this program or publication            
**    may be reproduced, transmitted, transcribed, stored in a                
**    retrieval system, or translated into any language or computer           
**    language, in any form or by any means, electronic, mechanical,          
**    magnetic, optical, chemical, manual, or otherwise, without              
**    the prior written permission of Intel Corporation.                      
**                                                                            
**    Intel makes no warranty of any kind regarding this code.  This code     
**    is provided on an "As Is" basis and Intel will not provide any support, 
**    assistance, installation, training or other services.  Intel does not   
**    provide any updates, enhancements or extensions.  Intel specifically    
**    disclaims any warranty of merchantability, noninfringement, fitness     
**    for any particular purpose, or any other warranty.                      
**                                                                            
**    Intel disclaims all liability, including liability for infringement     
**    of any proprietary rights, relating to use of the code.  No license,    
**    express or implied, by estoppel or otherwise, to any intellectual       
**    property rights is granted herein.                                      
**/

package com.intel.host.ipt.iha;

import com.intel.host.ipt.iha.IhaJniException;
/**                                                                            
********************************************************************************
**
**    @file IhaJni.java
**
**    @brief  Defines the JNI version of the Intel IPT Host Agent (IHA) API 
**
**    @author Ranjit Narjala
**
********************************************************************************
*/   
public class IhaJni extends java.lang.Object
{

    static {
	System.load("libiha.so");
    }

/**************************************************************************//**
 * Initialize internal dependencies. Should be called first and only once
 * after loading the iha library. \n
 * Please see the IHA C interface documentation for error codes thrown via
 * IhaJniException.
 *
 * @param	none
 *
 * @return	void
 *****************************************************************************/
	public  native void IHAInit() throws IhaJniException;
	
	
/**************************************************************************//**
 * Deinitialize IHA library. Should be called before unloading the DLL. \n
 * Please see the IHA C interface documentation for error codes thrown via
 * IhaJniException.
 *
 * @param	none
 *
 * @return	void
 *****************************************************************************/
	public  native void IHADeInit() throws IhaJniException;
	
	
/**************************************************************************//**
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
	public  native int IHAStartProvisioning(
								String strAppName) 
								throws IhaJniException;
								
								
/**************************************************************************//**
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
	public  native byte[] IHAEndProvisioning(
								String strAppName,
								int iSessionHandle,
								short[] saExpectedDataLen)
								throws IhaJniException;
								

/**************************************************************************//**
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
	public  native void IHASendData(
								String strAppName, 
								int iSessionHandle, 
								short sDataType, 
								byte[] baInData)
								throws IhaJniException;
								

/**************************************************************************//**
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
	public  native byte[] IHAReceiveData(
								String strAppName, 
								int iSessionHandle,
								short sDataType,
								short[] saExpectedDataLen)
								throws IhaJniException;
								

/**************************************************************************//**
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
	public  native void IHAProcessSVPMessage(
								String strAppName, 
								int iSessionHandle, 
								byte[] baInData)
								throws IhaJniException;
								
								
/**************************************************************************//**
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
	public  native byte[] IHAGetSVPMessage(
								String strAppName, 
								int iSessionHandle,
								short[] saExpectedDataLen)
								throws IhaJniException;


/**************************************************************************//**
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
	public  native byte[] IHASendAndReceiveData(
								String strAppName, 
								int iSessionHandle,
								short sDataType,
								byte[] baInData,
								short[] saExpectedOutDataLen)
								throws IhaJniException;
								
								
/**************************************************************************//**
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
	public  native byte[] IHAGetCapabilities(
								String strAppName,
								short sType,
								short[] saExpectedDataLen)
								throws IhaJniException;
								
								
/**************************************************************************//**
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
	public  native int IHAGetVersion() 
								throws IhaJniException;


/**************************************************************************//**
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
	public  native void IHAInstall(
								String strAppName,
								String strSrcFile)
								throws IhaJniException;
								
				
/**************************************************************************//**
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
	public  native void IHAUninstall(
								String strAppName)
								throws IhaJniException;
								
		
/**************************************************************************//**
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
	public  native void IHADoFWUpdate()
								throws IhaJniException;


/**************************************************************************//**
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
	public  native byte[] IHAGetOTP(
								String strAppName,
								int iSessionHandle,
								byte[] baEncToken,
								byte[] baVendorData,
								short[] saExpectedOtpLength,
								short[] saExpectedEncTokenLength)
								throws IhaJniException;
								
			
/**************************************************************************//**
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
	public  native int IHAGetOTPSStatus(
								String strAppName,
								int iSessionHandle,
								short sType)
								throws IhaJniException;
								
								
/**************************************************************************//**
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
	public  native byte[] IHAGetOTPCapabilities(
								String strAppName,
								short sType,
								short[] saExpectedDataLen)
								throws IhaJniException;
								
								
/**************************************************************************//**
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
	public  native void IHAInstallOTPS(
								String strAppName,
								String strSrcFile)
								throws IhaJniException;
								
								
/**************************************************************************//**
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
	public  native void IHAUninstallOTPS(
								String strAppName)
								throws IhaJniException;
	
/**************************************************************************//**
 * This function is used to register a callback function with the embedded
 * fw to receive and process notifications from the embedded app. \n
 * Callbacks can be registered for multiple eApps and the same callback can 
 * service multiple eApps. Only restriction is an app can have just one 
 * callback registered. The callback gets deregistered when the app gets 
 * uninstalled and/or the IHA DLL gets deinitialized.\n
 * If notification support is required by the IHA application then this 
 * must be called before any other request to the embedded app, else
 * notification registration will fail.
 *
 *
 * @param [in]		strAppName: String identifying the embedded app in the 
 *					chipset.
 * @param [in]		callback: IhaJniCallback object with user implemented 
 * 					callback method
 *
 * @return			void
	 *****************************************************************************/
	public  native void IHARegisterEventCb(
					      String strAppName/*, IhaJniCallback callback*/)
	    throws IhaJniException;
	
/**************************************************************************//**
 * This function is used to unregister an existing callback function from 
 * the embedded fw. \n
 * This API should only be used if IHA_RegisterEventCb has been called previously
 * to register a callback function with the embedded FW. It returns silently with
 * success if used otherwise.\n
 * It is NOT mandatory to call this API to unregister callbacks, that will get
 * done internally at sw uninstall stage. One potential use case for the API 
 * would be if client application fails to complete any post-registration 
 * operation i.e. after IHA_RegisterEventCb() has returned successfully, and 
 * needs to re-do the registration from scratch by unregistering the callback 
 * first.\n
 * This API was added in IPT 2.1, and is only to be used by those apps that do
 * not need to run on 2011 platforms.
 *
 * @param [in]		strAppName: String identifying the embedded app in the 
 *					chipset.
 *
 * @return			void
 *****************************************************************************/
	public  native void IHAUnregisterEventCb(
								String strAppName)
								throws IhaJniException;
	
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
    public  native long IHA_GetAppInstId(
								String strAppName)
								throws IhaJniException;
	
} // class IhaJni
