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

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.RemoteException;
import android.util.Log;
import com.intel.security.service.ISEPService;
import com.intel.security.lib.sepdrmJNI;
import com.intel.host.ipt.iha.IhaJni;
import com.intel.host.ipt.iha.IhaJniException;

class ISEPServiceImpl extends ISEPService.Stub {
	private static final String TAG = ISEPServiceImpl.class.getSimpleName();
	private final Context context;
	private static final int DRM_SUCCESS = 0;
	private final static int IHA_RET_S_OK = 0;
	private static int IHAError = IHA_RET_S_OK;
	private IhaJni iha;

	ISEPServiceImpl(Context context) {
		this.context = context;

		iha = new IhaJni();
	}

	public void initDrmLibrary() {
		Log.d(TAG, "initDrmLibrary()");
		/*
		  if (this.context.checkCallingOrSelfPermission(Manifest.permission.SEP_ACCESS) !=
		  PackageManager.PERMISSION_GRANTED) {
		  throw new SecurityException("Requires SEP_ACCESS permission");
		  }
		*/
		int status = sepdrmJNI.libraryInit();
		if (status != DRM_SUCCESS) {
			//FIXME: Exceptions cannot be thrown across process boundaries!
			throw new RuntimeException("Library Initialization failed (0x" +
						   Integer.toHexString(status) + ")");
		}
	}

	public long getRandomNumber() throws RemoteException {
		Log.d(TAG, "getRandomNumber()");

		return sepdrmJNI.getRandomNumber();
	}



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
	public void IHAInit() {
		Log.d(TAG, "IHAInit");
		try {
			iha.IHAInit();
		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}
			Log.e(TAG, "IHAInit failed" + IHAError);
		}
	}

	/**
	 * Deinitialize IHA library. Should be called before unloading the DLL. \n
	 * Please see the IHA C interface documentation for error codes thrown via
	 * IhaJniException.
	 *
	 * @param	none
	 *
	 * @return	void
	 *****************************************************************************/
	public void IHADeInit() {
		Log.d(TAG, "IHADeInit");
		try {
			iha.IHADeInit();
		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}
			Log.e(TAG, "IHADeInit failed" + IHAError);
		}
	}

	public int IHAStartProvisioning( String strAppName ) {
		Log.d(TAG, "IHAStartProvisioning");
		try {
			return iha.IHAStartProvisioning( strAppName );
		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}
			Log.e(TAG, "IHAStartProvisioning failed" + IHAError );
		}
		return -1;
	}


	public byte[] IHAEndProvisioning(String strAppName, int iSessionHandle,
					 int[] iaExpectedTokenLen) {
		Log.d(TAG, "IHAEndProvisioning");
		try {

			int len = iaExpectedTokenLen.length;
			short saExpectedTokenLen[] = new short[ len ];
			for (int i = 0; i < len; i++ ) {
				saExpectedTokenLen[i] = (short)iaExpectedTokenLen[i];
			}
			byte[] orig_encrToken = iha.IHAEndProvisioning( strAppName, iSessionHandle, saExpectedTokenLen);
			for (int i = 0; i < len; i++ ) {
				iaExpectedTokenLen[i] = saExpectedTokenLen[i];
			}

			Log.v(TAG, "IHAEndProvisioing returned Token Len: " + iaExpectedTokenLen[0]);
			return orig_encrToken;

		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}
			Log.e(TAG, "IHAEndProvisioning failed" + IHAError );
			return null;
		}
	}

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
	public void IHASendData( String strAppName,
				 int iSessionHandle,
				 int sDataType,
				 byte[] baInData) {


		Log.d(TAG, "IHASendData");
		try {

			iha.IHASendData( strAppName, iSessionHandle, (short)sDataType, baInData );

		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}
			Log.e(TAG, "IHASendData failed" + IHAError );
		}

	}


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
	public byte[] IHAReceiveData( String strAppName, int iSessionHandle,
				      int sDataType, int[] iaExpectedDataLen) {

		Log.d(TAG, "IHAReceiveData");
		int len = iaExpectedDataLen.length;
		short saExpectedDataLen[] = new short[ len ];
		try {

			for (int i = 0; i < len; i++ ) {
				saExpectedDataLen[i] = (short)iaExpectedDataLen[i];
			}

			byte[] recvData =  iha.IHAReceiveData( strAppName, iSessionHandle, (short)sDataType, saExpectedDataLen );
			for (int i = 0; i < len; i++ ) {
				iaExpectedDataLen[i] = saExpectedDataLen[i];
			}
			return recvData;

		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}
			Log.e(TAG, "IHASendData failed" + IHAError );
			return null;
		}
	}


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
	public void IHAProcessSVPMessage(String strAppName, int iSessionHandle,
					 byte[] baInData) {
		Log.d(TAG, "IHAProcessSVPMessage");
		try {

			iha.IHAProcessSVPMessage( strAppName, iSessionHandle, baInData);

		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}

			Log.e(TAG, "IHAProcessSVPMessage failed" + IHAError );
		}
	}


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
	public byte[] IHAGetSVPMessage( String strAppName, int iSessionHandle,
					int[] iaExpectedDataLen) {

		Log.d(TAG, "IHAGetSVPMessage");

		int len = iaExpectedDataLen.length;
		short saExpectedDataLen[] = new short[ len ];
		for (int i = 0; i < len; i++ ) {
			saExpectedDataLen[i] = (short)iaExpectedDataLen[i];
		}

		try {
			byte[] getSVPMsg = iha.IHAGetSVPMessage( strAppName, iSessionHandle, saExpectedDataLen);
			for (int i = 0; i < len; i++ ) {
				iaExpectedDataLen[i] = saExpectedDataLen[i];
			}
			return getSVPMsg;

		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}
			Log.e(TAG, "IHAGetSVPMessage failed" + IHAError );
			return null;
		}
	}

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
	public byte[] IHASendAndReceiveData(String strAppName,
					    int iSessionHandle, int sDataType, byte[] baInData,
					    int[] iaExpectedOutDataLen) {

		Log.d(TAG, "IHASendAndReceiveData");

		int len = iaExpectedOutDataLen.length;
		short saExpectedOutDataLen[] = new short[ len ];
		for (int i = 0; i < len; i++ ) {
			saExpectedOutDataLen[i] = (short)iaExpectedOutDataLen[i];
		}

		try {
			byte[] sendRecvData = iha.IHASendAndReceiveData( strAppName, iSessionHandle, (short)sDataType,
									 baInData, saExpectedOutDataLen);
			for (int i = 0; i < len; i++ ) {
				iaExpectedOutDataLen[i] = saExpectedOutDataLen[i];
			}
			return sendRecvData;


		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}

			Log.e(TAG, "IHASendAndReceiveData failed" + IHAError );
			return null;
		}
	}

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
	 *				1: Embedded App Version \n
	 *				2: Embedded App Security Version.
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
	public byte[] IHAGetCapabilities( String strAppName, int sType, int[] iaExpectedDataLen) {

		Log.d(TAG, "IHAGetCapabilities");

		int len = iaExpectedDataLen.length;
		short saExpectedDataLen[] = new short[ len ];
		for (int i = 0; i < len; i++ ) {
			saExpectedDataLen[i] = (short)iaExpectedDataLen[i];
		}

		try {
			byte[] getCaps =  iha.IHAGetCapabilities( strAppName, (short)sType, saExpectedDataLen);
			for (int i = 0; i < len; i++ ) {
				iaExpectedDataLen[i] = saExpectedDataLen[i];
			}
			return getCaps;

		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}

			Log.e(TAG, "IHAGetCapabilities failed" + IHAError );
		}
		return null;
	}


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
	public int IHAGetVersion() {

		Log.d(TAG, "IHAGetVersion");
		try {

			return iha.IHAGetVersion();

		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}

			Log.e(TAG, "IHAGetOTP failed" + IHAError );
			return -1;
		}
	}

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
	public void IHAInstall( String strAppName, String strSrcFile) {

		Log.d(TAG, "IHAInstall");
		try {

			iha.IHAInstall( strAppName, strSrcFile);

		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}
			Log.e(TAG, "IHAInstall failed" + IHAError );
		}
	}


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
	public void IHAUninstall( String strAppName) {
		Log.d(TAG, "IHAUninstall");
		try {

			iha.IHAUninstall( strAppName );

		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}

			Log.e(TAG, "IHAUninstall failed" + IHAError );
		}
	}

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
	public void IHADoFWUpdate() {
		Log.d(TAG, "IHADoFWUpdate");
		try {

			iha.IHADoFWUpdate( );

		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}

			Log.e(TAG, "IHADoFWUpdate failed" + IHAError );
		}
	}


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
	public byte[] IHAGetOTP(  String strAppName, int iSessionHandle, byte[] baEncToken,
				  byte[] baVendorData, int[] iaExpectedOtpLength,
				  int[] iaExpectedEncTokenLength) {

		Log.d(TAG, "Interface2 IHAGetOTP");

		int i = 0;
		int len = iaExpectedOtpLength.length;
		int len1 = 0;
		short saExpectedOtpLength[] = new short[ len ];
		for (i = 0; i < len; i++ ) {
			saExpectedOtpLength[i] = (short)iaExpectedOtpLength[i];
		}
		Log.d(TAG, "After created ExpectedOtpLength array");
		len1 = iaExpectedEncTokenLength.length;
		short saExpectedEncTokenLen[] = new short[ len1 ];
		i = 0;
		for (i = 0; i < len1; i++ ) {
			saExpectedEncTokenLen[i] = (short)iaExpectedEncTokenLength[i];
		}
		Log.d(TAG, "After created ExpectedEncTokenLen array");


		try {

			byte[] getOtpBytes = iha.IHAGetOTP( strAppName, iSessionHandle, baEncToken,
							    baVendorData, saExpectedOtpLength,
							    saExpectedEncTokenLen);
			for (i = 0; i < len; i++ ) {
				iaExpectedOtpLength[i] = saExpectedOtpLength[i];
			}
			for (i = 0; i < len1; i++ ) {
				iaExpectedEncTokenLength[i] = saExpectedEncTokenLen[i];
			}
			return getOtpBytes;


		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}
			Log.e(TAG, "Interface2 IHAGetOTP failed " + IHAError + ", copying expected len anyway");
			Log.d(TAG, "after ll call: OTP array len = " + len);
			for (i = 0; i < len; i++ ) {
				Log.d(TAG, "OTP(short)["+i+"]=" + saExpectedOtpLength[i]);
				iaExpectedOtpLength[i] = saExpectedOtpLength[i];
				Log.d(TAG, "OTP(int)["+i+"]=" + iaExpectedOtpLength[i]);
			}
			Log.d(TAG, "after ll call: EETL array len = " + len1);
			for (i = 0; i < len1; i++ ) {
				Log.d(TAG, "EETL(short)["+i+"]=" + saExpectedEncTokenLen[i]);
				iaExpectedEncTokenLength[i] = saExpectedEncTokenLen[i];
				Log.d(TAG, "EETL(int)["+i+"]=" + iaExpectedEncTokenLength[i]);
			}
			return null;
		}
	}

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
	public int IHAGetOTPSStatus( String strAppName, int iSessionHandle, int sType) {

		Log.d(TAG, "IHAGetOTPSStatus");
		try {

			return iha.IHAGetOTPSStatus( strAppName, iSessionHandle, (short)sType );

		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}
			Log.e(TAG, "IHAGetOTPSStatus failed" + IHAError );
			return -1;
		}
	}

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
	public byte[] IHAGetOTPCapabilities( String strAppName, int sType, int[] iaExpectedDataLen) {

		Log.d(TAG, "IHAGetOTPCapabilities");

		int len = iaExpectedDataLen.length;
		byte[] getOtpCaps = null;
		short saExpectedDataLen[] = new short[ len ];
		for (int i = 0; i < len; i++ ) {
			saExpectedDataLen[i] = (short)iaExpectedDataLen[i];
		}

		try {
			getOtpCaps = iha.IHAGetOTPCapabilities( strAppName, (short)sType, saExpectedDataLen);
			for (int i = 0; i < len; i++ ) {
				iaExpectedDataLen[i] = saExpectedDataLen[i];
			}

		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}
			Log.e(TAG, "IHAGetOTP failed" + IHAError );
		}

		return getOtpCaps;
	}

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
	public void IHAInstallOTPS( String strAppName, String strSrcFile) {
		Log.d(TAG, "IHAInstallOTPS");
		try {

			iha.IHAInstallOTPS( strAppName, strSrcFile);

		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}

			Log.e(TAG, "IHAInstallOTPS failed" + IHAError );
		}
	}

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
	public void IHAUninstallOTPS( String strAppName) {
		Log.d(TAG, "IHAUninstallOTPS");
		try {

			iha.IHAUninstallOTPS( strAppName );

		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}
			Log.e(TAG, "IHAUninstallOTPS failed" + IHAError );
		}
	}

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
	public long IHA_GetAppInstId( String strAppName ) {
		Log.d(TAG, "IHA_GetAppInstId");
		try {

			return iha.IHA_GetAppInstId( strAppName );

		} catch (IhaJniException e) {
			e.printStackTrace();
			if (e instanceof IhaJniException) {
				SetLastError( (IhaJniException) e );
			}
			Log.e(TAG, "IHA_GetAppInstId failed" + IHAError);
			return -1;
		}
	}

	/*****************************************************************************
	 * This function is used retrieve the last error that occred proecessing IHA
	 * function.
	 * @return		 Error value in integer type
	 *****************************************************************************/
	public int IHAGetLastError() {
		int Error = IHAError;
		IHAError = IHA_RET_S_OK;
		return Error;
	}

	private void SetLastError( IhaJniException e ) {
		IHAError = e.GetError();
	}

}
