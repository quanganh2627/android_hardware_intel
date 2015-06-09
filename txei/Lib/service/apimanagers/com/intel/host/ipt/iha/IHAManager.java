/*************************************************************************
 **     Copyright (c) 2013 Intel Corporation. All rights reserved.      **
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
package com.intel.host.ipt.iha;

import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;
import com.intel.security.service.ISEPService;
import com.intel.host.ipt.iha.IhaException;

/**
 * @author nshuster
 *
 */
public class IHAManager {
    private static final String TAG = IHAManager.class.getSimpleName();
    private static final String REMOTE_SERVICE_NAME = ISEPService.class.getName();
    private static boolean isInitialized = false;
    private final static int MAX_APPNAME_LENGTH = 300;
    private final static int MAX_SEND_RECV_DATA_LENGTH = 5000;
    private final static int MAX_ENCR_TOKEN_LENGTH = 300;
    private final static int MAX_SRC_FILE_LENGTH = 300;
    private final static int MAX_GET_CAPS_STRING_LENGTH = 300;
    private final static int MAX_SVP_MESSAGE_LENGTH = 5000;
    private final static int IHA_RET_S_OK = 0;
    private static int IHAManagerError = IHA_RET_S_OK;
    private final static int INVALID_INPUT = 13;
    ISEPService service;

    public static IHAManager getInstance() {
	Log.i(TAG, "IHAManager.getInstance()");
	return new IHAManager();
    }


    private IHAManager() {
	Log.d(TAG, "Connecting to ISEPService by name [" + REMOTE_SERVICE_NAME + "]");
	this.service = ISEPService.Stub.asInterface(ServiceManager.getService(REMOTE_SERVICE_NAME));
	if (this.service == null) {
	    throw new IllegalStateException("Failed to find ISEPService by name [" + REMOTE_SERVICE_NAME + "]");
	}
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
	IHAManagerError = IHA_RET_S_OK;
	try {
	    this.service.IHAInit();
	} catch (RemoteException e) {
	    e.printStackTrace();
	    Log.d(TAG, "IHAInit failed");
	    throw new RuntimeException("IHAInit failed", e);
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
	    this.service.IHADeInit();
	} catch (RemoteException e) {
	    e.printStackTrace();
	    Log.d(TAG, "IHADeInit failed");
	    throw new RuntimeException("IHADeInit failed", e);
	}
    }

    public int IHAStartProvisioning(String strAppName) {
	Log.d(TAG, "IHAStartProvisioning");
	int handle = 0;
	if (strAppName.length() == 0 || strAppName.length() > MAX_APPNAME_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid App Name argument");
	    throw new RuntimeException("Invalid App Name argument");
	}

	try {
	    Log.d(TAG, "IHAStartProvisioning handle="+handle);

	    handle = this.service.IHAStartProvisioning( strAppName );
	    Log.d(TAG, "IHAStartProvisioning handle="+handle);
	    int err = IHAGetLastError();
	    if (err != IHA_RET_S_OK) {
		throw new IhaException("IHAStartProvisioning failed", err);
	    }
	} catch (RemoteException e) {
	    e.printStackTrace();
	    Log.d(TAG, "IHAStartProvisioning failed");
	    throw new RuntimeException("IHAStartProvisioning failed", e);
	}

	return handle;
    }


    public byte[] IHAEndProvisioning(String strAppName, int iSessionHandle,
				     short[] saExpectedTokenLen) {
	Log.d(TAG, "IHAEndProvisioning");

	if (strAppName.length() == 0 || strAppName.length() > MAX_APPNAME_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid App Name argument");
	    throw new RuntimeException("Invalid App Name argument");
	}
	if (saExpectedTokenLen.length == 0 || saExpectedTokenLen.length > MAX_ENCR_TOKEN_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid ExpectedTokenApp array length");
	    throw new RuntimeException("Invalid ExpectedTokenLenApp array length");
	}
	/*
	 * AIDL does not support the 'short' data type so convert
	 * short's to int's.
	 */
	byte[] orig_encrToken;
	int len = saExpectedTokenLen.length;
	int iaExpectedTokenLen[] = new int[ len ];
	for (int i = 0; i < len; i++ ) {
	    iaExpectedTokenLen[i] = saExpectedTokenLen[i];
	}

	try {

	    orig_encrToken = this.service.IHAEndProvisioning( strAppName, iSessionHandle, iaExpectedTokenLen);
	    Log.v(TAG, "IHAEndProvisioning returning token len: " + iaExpectedTokenLen[0]);
	    for (int i = 0; i < len; i++ ) {
		saExpectedTokenLen[i] = (short) iaExpectedTokenLen[i];
	    }
	    int err = IHAGetLastError();
	    if (err != 0) {
		throw new IhaException("IHAEndProvisioning failed", err);
	    }

	} catch (RemoteException e) {
	    e.printStackTrace();
	    Log.d(TAG, "IHAEnadProvisioning failed");
	    throw new RuntimeException("IHAEndProvisioning failed", e);
	}

	return orig_encrToken;
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
			     short sDataType,
			     byte[] baInData) {


	Log.d(TAG, "IHASendData");

	if (strAppName.length() == 0 || strAppName.length() > MAX_APPNAME_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid App Name argument");
	    throw new RuntimeException("Invalid App Name argument");
	}
	if (sDataType < 0) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid DataType value");
	    throw new RuntimeException("Invalid DataType value");
	}
	if (baInData.length == 0 || baInData.length > MAX_SEND_RECV_DATA_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid InData array");
	    throw new RuntimeException("Invalid InData array length");
	}

	try {

	    this.service.IHASendData( strAppName, iSessionHandle, (int)sDataType, baInData );
	    // Check for IHA Error from last IHA ISepService call.
	    int err = IHAGetLastError();
	    if (err != IHA_RET_S_OK) {
		throw new IhaException( "IHASendData Failed", err);
	    }

	} catch (RemoteException e) {
	    e.printStackTrace();
	    Log.d(TAG, "IHASendData failed");
	    throw new RuntimeException("IHASendData failed", e);
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
				  short sDataType, short[] saExpectedDataLen) {

	Log.d(TAG, "IHAReceiveData");

	if (strAppName.length() == 0 || strAppName.length() > MAX_APPNAME_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    throw new RuntimeException("Invalid App Name argument");
	}
	if (sDataType < 0) {
	    IHAManagerError = INVALID_INPUT;
	    throw new RuntimeException("Invalid DataType value");
	}
	if (saExpectedDataLen.length == 0 || saExpectedDataLen.length > MAX_SEND_RECV_DATA_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    throw new RuntimeException("Invalid ExpectedDataLen array length");
	}
	/*
	 * AIDL does not support the 'short' data type so convert
	 * short's to int's.
	 */
	int len = saExpectedDataLen.length;
	int iaExpectedDataLen[] = new int[ len ];
	for (int i = 0; i < len; i++ ) {
	    iaExpectedDataLen[i] = saExpectedDataLen[i];
	}

	try {
	    byte[] recvData =  this.service.IHAReceiveData( strAppName, iSessionHandle, (int)sDataType, iaExpectedDataLen );
	    for (int i = 0; i < len; i++ ) {
		saExpectedDataLen[i] = (short) iaExpectedDataLen[i];
	    }
	    // Check for IHA Error from last IHA ISepService call.
	    int err = IHAGetLastError();
	    if (err != IHA_RET_S_OK) {
		throw new IhaException( "IHAReceiveData Failed", err);
	    }

	    return recvData;

	} catch (RemoteException e) {
	    e.printStackTrace();
	    throw new RuntimeException("IHASendData failed", e);
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

	if (strAppName.length() == 0 || strAppName.length() > MAX_APPNAME_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid App Name argument");
	    throw new RuntimeException("Invalid App Name argument");
	}

	if (baInData.length == 0 || baInData.length > MAX_SVP_MESSAGE_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid Indata array");
	    throw new RuntimeException("Invalid InData array length");
	}

	try {

	    this.service.IHAProcessSVPMessage( strAppName, iSessionHandle, baInData);
	    // Check for IHA Error from last IHA ISepService call.
	    int err = IHAGetLastError();
	    if (err != IHA_RET_S_OK) {
		throw new IhaException( "IHAProcessSVPMessage failed", err);
	    }

	} catch (RemoteException e) {
	    e.printStackTrace();
	    throw new RuntimeException("IHAProcessSVPMessage failed", e);
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
				    short[] saExpectedDataLen) {

	Log.d(TAG, "IHAGetSVPMessage");

	if (strAppName.length() == 0 || strAppName.length() > MAX_APPNAME_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid App Name argument");
	    throw new RuntimeException("Invalid App Name argument");
	}
	if (saExpectedDataLen.length == 0 || saExpectedDataLen.length > MAX_SVP_MESSAGE_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid ExpectedDataLen array");
	    throw new RuntimeException("Invalid ExpectedDataLen array length");
	}

	/*
	 * AIDL does not support the 'short' data type so convert
	 * short's to int's.
	 */
	int len = saExpectedDataLen.length;
	int iaExpectedDataLen[] = new int[ len ];
	for (int i = 0; i < len; i++ ) {
	    iaExpectedDataLen[i] = saExpectedDataLen[i];
	}

	try {
	    byte[] getSVPMsg =  this.service.IHAGetSVPMessage( strAppName, iSessionHandle, iaExpectedDataLen);
	    for (int i = 0; i < len; i++ ) {
        	saExpectedDataLen[i] = (short)iaExpectedDataLen[i];
	    }

	    // Check for IHA Error from last IHA ISepService call.
	    int err = IHAGetLastError();
	    if (err != IHA_RET_S_OK) {
		throw new IhaException( "IHAGetSVPMessage Failed", err);
	    }

	    return getSVPMsg;
	} catch (RemoteException e) {
	    e.printStackTrace();
	    throw new RuntimeException("IHAGetSVPMessage failed", e);
	}
    }

    /**
     * This function is used to send and receive data to and from the embedded app
     * in the FW.
     * This API was added in IPT 2.0, and is only to be used by those apps that do
     * not need to run on 2011 platforms.
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
					int iSessionHandle, short sDataType, byte[] baInData,
					short[] saExpectedOutDataLen) {

	Log.d(TAG, "IHASendAndReceiveData");

	if (strAppName.length() == 0 || strAppName.length() > MAX_APPNAME_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid App Name");
	    throw new RuntimeException("Invalid App Name argument");
	}
	if (sDataType < 0) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid DataType");
	    throw new RuntimeException("Invalid DataType value");
	}
	if (baInData.length == 0 || baInData.length > MAX_SEND_RECV_DATA_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid InData array");
	    throw new RuntimeException("Invalid InData array length");
	}
	if (saExpectedOutDataLen.length == 0 || saExpectedOutDataLen. length > MAX_SEND_RECV_DATA_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid ExpectedOutDataLen array length");
	    throw new RuntimeException("Invalid ExpectedOutDataLen array length");
	}

	/*
	 * AIDL does not support the 'short' data type so convert
	 * short's to int's.
	 */
	int len = saExpectedOutDataLen.length;
	int iaExpectedOutDataLen[] = new int[ len ];
	for (int i = 0; i < len; i++ ) {
	    iaExpectedOutDataLen[i] = saExpectedOutDataLen[i];
	}

	try {
	    byte[] sendRecvData = this.service.IHASendAndReceiveData( strAppName, iSessionHandle, (int)sDataType,
								      baInData, iaExpectedOutDataLen);

	    // Check for IHA Error from last IHA ISepService call.
	    int err = IHAGetLastError();
	    Log.d(TAG, "IHAGetLastError returned " + err);
	    if (err != IHA_RET_S_OK) {
		throw new IhaException( "IHASendAndReceiveData Failed", err);
	    }

	    for (int i = 0; i < len; i++ ) {
	        saExpectedOutDataLen[i] = (short) iaExpectedOutDataLen[i];
	    }
	    return sendRecvData;

	} catch (RemoteException e) {
	    Log.d(TAG, "IHASendAndReceiveData caught RemoteException " + e);
	    e.printStackTrace();
	    throw new RuntimeException("IHASendAndReceiveData failed", e);
	}
    }

    /**
     * This function is used to obtain capabilities of embedded app, including
     * version information.
     * This API was added in IPT 2.0, and is only to be used by those apps that do
     * not need to run on 2011 platforms.
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the
     *					chipset.
     * @param [in]		sType: Type of capability information requested. Can be: \n
     *					1: Embedded App Version
     *                                  2: Embedded App Security Version.
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
    public byte[] IHAGetCapabilities( String strAppName, short sType, short[] saExpectedDataLen) {

	Log.d(TAG, "IHAGetCapabilities");

	if (strAppName.length() == 0 || strAppName.length() > MAX_APPNAME_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid App Name");
	    throw new RuntimeException("Invalid App Name argument");
	}
	if (sType < 0) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid Type");
	    throw new RuntimeException("Invalid Type value");
	}
	if (saExpectedDataLen.length == 0 || saExpectedDataLen.length > MAX_GET_CAPS_STRING_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid ExpectedDataLen array length");
	    throw new RuntimeException("Invalid ExpectedDataLen array length");
	}
	/*
	 * AIDL does not support the 'short' data type so convert
	 * short's to int's.
	 */
	int len = saExpectedDataLen.length;
	int iaExpectedDataLen[] = new int[ len ];
	for (int i = 0; i < len; i++ ) {
	    iaExpectedDataLen[i] = saExpectedDataLen[i];
	}

	try {
	    byte[] getCaps =  this.service.IHAGetCapabilities( strAppName, (int)sType, iaExpectedDataLen);
	    for (int i = 0; i < len; i++ ) {
		saExpectedDataLen[i] = (short)iaExpectedDataLen[i];
	    }
	    // Check for IHA Error from last IHA ISepService call.
	    int err = IHAGetLastError();
	    if (err != IHA_RET_S_OK) {
		throw new IhaException("IHAGetCapabilities failed", err);
	    }
	    return getCaps;

	} catch (RemoteException e) {
	    e.printStackTrace();
	    throw new RuntimeException("IHAGetCapabilities failed", e);
	}
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

	int status = 0;
	Log.d(TAG, "IHAGetVersion");
	try {

	    status = this.service.IHAGetVersion();
	    // Check for IHA Error from last IHA ISepService call.
	    int err = IHAGetLastError();
	    if (err != IHA_RET_S_OK) {
		throw new IhaException( "IHAGetVersion failed", err);
	    }

	} catch (RemoteException e) {
	    e.printStackTrace();
	    throw new RuntimeException("IHAGetVersion failed", e);
	}
	return status;
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
    void IHAInstall( String strAppName, String strSrcFile) {

	Log.d(TAG, "IHAInstall");
	if (strAppName.length() == 0 || strAppName.length() > MAX_APPNAME_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid App Name");
	    throw new RuntimeException("Invalid App Name argument");
	}
	if (strSrcFile.length() == 0 || strSrcFile.length() > MAX_SRC_FILE_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid Source Filename");
	    throw new RuntimeException("Invalid Source File Name argument");
	}

	try {

	    this.service.IHAInstall( strAppName, strSrcFile);
	    // Check for IHA Error from last IHA ISepService call.
	    int err = IHAGetLastError();
	    if (err != IHA_RET_S_OK) {
		throw new IhaException("IHAInstall failed", err);
	    }

	} catch (RemoteException e) {
	    e.printStackTrace();
	    throw new RuntimeException("IHAInstall failed", e);
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
    void IHAUninstall( String strAppName) {
	Log.d(TAG, "IHAUninstall");
	if (strAppName.length() == 0 || strAppName.length() > MAX_APPNAME_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid App Name");
	    throw new RuntimeException("Invalid App Name argument");
	}

	try {

	    this.service.IHAUninstall( strAppName );
	    // Check for IHA Error from last IHA ISepService call.
	    int err = IHAGetLastError();
	    if (err != IHA_RET_S_OK) {
		throw new IhaException( "IHAUninstall failed", err);
	    }

	} catch (RemoteException e) {
	    e.printStackTrace();
	    throw new RuntimeException("IHAUninstall failed", e);
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
    void IHADoFWUpdate() {
	Log.d(TAG, "IHADoFWUpdate");
	try {

	    this.service.IHADoFWUpdate( );
	    // Check for IHA Error from last IHA ISepService call.
	    int err = IHAGetLastError();
	    if (err != IHA_RET_S_OK) {
		throw new IhaException("IHADoFWUpdate failed", err);
	    }

	} catch (RemoteException e) {
	    e.printStackTrace();
	    throw new RuntimeException("IHADoFWUpdate failed", e);
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
			      byte[] baVendorData, short[] saExpectedOtpLength,
			      short[] saExpectedEncTokenLength) {


	Log.d(TAG, "Interface1 IHAGetOTP");

	if (strAppName.length() == 0 || strAppName.length() > MAX_APPNAME_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid App Name");
	    throw new RuntimeException("Invalid App Name argument");
	}
	/*
	 * AIDL does not support the 'short' data type so convert
	 * short's to int's.
	 */
	int i = 0;
	int len = saExpectedOtpLength.length;
	int iaExpectedOtpLength[] = new int[ len ];
	for ( i = 0; i < len; i++ ) {
	    iaExpectedOtpLength[i] = saExpectedOtpLength[i];
	}
	int len1 = saExpectedEncTokenLength.length;
	int iaExpectedEncTokenLen[] = new int[ len1 ];
	for ( i = 0; i < len1; i++ ) {
	    iaExpectedEncTokenLen[i] = saExpectedEncTokenLength[i];
	}

	try {
	    byte[] getOTP = this.service.IHAGetOTP( strAppName, iSessionHandle, baEncToken,
						    baVendorData, iaExpectedOtpLength,iaExpectedEncTokenLen);
	    for ( i = 0; i < len; i++ ) {
		saExpectedOtpLength[i] = (short) iaExpectedOtpLength[i];
	    }
	    for ( i = 0; i < len1; i++ ) {
        	saExpectedEncTokenLength[i] = (short) iaExpectedEncTokenLen[i];
	    }
	    // Check for IHA Error from last IHA ISepService call.
	    int err = IHAGetLastError();
	    if (err != IHA_RET_S_OK) {
		throw new IhaException("IHAGetOTP failed ", err);
	    }
	    return getOTP;

	} catch (RemoteException e) {
	    e.printStackTrace();
	    Log.d(TAG, "IHAGetOtp failed, copying expected len anyway");
	    for ( i = 0; i < len; i++ ) {
			Log.d(TAG, "OTP(int)["+i+"]=" + iaExpectedOtpLength[i]);
			saExpectedOtpLength[i] = (short) iaExpectedOtpLength[i];
			Log.d(TAG, "OTP(short)["+i+"]=" + saExpectedOtpLength[i]);
	    }
	    for ( i = 0; i < len1; i++ ) {
			Log.d(TAG, "EETL(int)["+i+"]=" + iaExpectedEncTokenLen[i]);
        	saExpectedEncTokenLength[i] = (short) iaExpectedEncTokenLen[i];
			Log.d(TAG, "EETL(short)["+i+"]=" + saExpectedEncTokenLength[i]);
	    }
	    throw new RuntimeException("Interface1 IHAGetOTP failed ", e);
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
    public int IHAGetOTPSStatus( String strAppName, int iSessionHandle, short sType) {
	int status = IHA_RET_S_OK;
	Log.d(TAG, "IHAGetOTPSStatus");

	if (strAppName.length() == 0 || strAppName.length() > MAX_APPNAME_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid App Name");
	    throw new RuntimeException("Invalid App Name argument");
	}
	if (sType < 0) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid DataType");
	    throw new RuntimeException("Invalid DataType value");
	}

	try {
	    /*
	     * AIDL does not support the 'short' data type so convert
	     * short's to int's.
	     */
	    status = this.service.IHAGetOTPSStatus( strAppName, iSessionHandle, (int)sType );
	    // Check for IHA Error from last IHA ISepService call.
	    int err = IHAGetLastError();
	    if (err != IHA_RET_S_OK) {
		throw new IhaException("IHAGetOTPSStatus failed", err);
	    }

	} catch (RemoteException e) {
	    e.printStackTrace();
	    throw new RuntimeException("IHAGetOTPSStatus failed", e);
	}
	return status;
    }

    /**
     * This function is used to obtain capabilities of embedded app, including
     * version information.
     * This is a legacy API, kept for backward compatibility with version 1.x.
     * Has been replaced by IHA_GetCapabilities() in version 2.0 onwards.
     * Please see the IHA C interface documentation for error codes thrown via
     * IhaJniException.
     *
     * @param [in]		strAppName: String identifying the embedded app in the
     *					chipset.
     * @param [in]		sType: Type of capability information requested. Can be: \n
     *					1: Embedded App Version
     *					2: Embedded App Security Version.
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
    public byte[] IHAGetOTPCapabilities( String strAppName, short sType, short[] saExpectedDataLen) {

	Log.d(TAG, "IHAGetOTPCapabilities");

	if (strAppName.length() == 0 || strAppName.length() > MAX_APPNAME_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid App Name");
	    throw new RuntimeException("Invalid App Name argument");
	}
	if (sType < 0) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid DataType");
	    throw new RuntimeException("Invalid DataType value");
	}
	if (saExpectedDataLen.length == 0 || saExpectedDataLen.length > MAX_GET_CAPS_STRING_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    Log.d(TAG, "Invalid ExpectedDataLen array length");
	    throw new RuntimeException("Invalid ExpectedDataLen array length");
	}
	/*
	 * AIDL does not support the 'short' data type so convert
	 * short's to int's.
	 */
	int len = saExpectedDataLen.length;
	byte[] getOtpCaps;
	int iaExpectedDataLen[] = new int[ len ];
	for (int i = 0; i < len; i++ ) {
	    iaExpectedDataLen[i] = saExpectedDataLen[i];
	}

	try {
	    getOtpCaps =  this.service.IHAGetOTPCapabilities( strAppName, (int)sType, iaExpectedDataLen);
	    for (int i = 0; i < len; i++ ) {
        	saExpectedDataLen[i] = (short) iaExpectedDataLen[i];
	    }
	    // Check for IHA Error from last IHA ISepService call.
	    int err = IHAGetLastError();
	    if (err != IHA_RET_S_OK) {
		throw new IhaException("IHAGetOTPCapabilities failed", err);
	    }

	} catch (RemoteException e) {
	    e.printStackTrace();
	    Log.d(TAG, "IHAGetOTPCapabilities failed");
	    throw new RuntimeException("IHAGetOTPCapabilities failed", e);
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
    void IHAInstallOTPS( String strAppName, String strSrcFile) {
	Log.d(TAG, "IHAInstallOTPS");

	if (strAppName.length() == 0 || strAppName.length() > MAX_APPNAME_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    throw new RuntimeException("Invalid App Name argument");
	}
	if (strSrcFile.length() == 0 || strSrcFile.length() > MAX_SRC_FILE_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    throw new RuntimeException("Invalid Source File Name argument");
	}

	try {

	    this.service.IHAInstallOTPS( strAppName, strSrcFile);
	    // Check for IHA Error from last IHA ISepService call.
	    int err = IHAGetLastError();
	    if (err != IHA_RET_S_OK) {
		throw new IhaException( "IHAInstallOTPS failed", err);
	    }

	} catch (RemoteException e) {
	    e.printStackTrace();
	    throw new RuntimeException("IHAInstallOTPS failed", e);
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
    void IHAUninstallOTPS( String strAppName) {
	Log.d(TAG, "IHAUninstallOTPS");

	if (strAppName.length() == 0 || strAppName.length() > MAX_APPNAME_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    throw new RuntimeException("Invalid App Name argument");
	}

	try {

	    this.service.IHAUninstallOTPS( strAppName );
	    // Check for IHA Error from last IHA ISepService call.
	    int err = IHAGetLastError();
	    if (err != IHA_RET_S_OK) {
		throw new IhaException("IHAUninstallOTPS failed", err);
	    }

	} catch (RemoteException e) {
	    e.printStackTrace();
	    throw new RuntimeException("IHAUninstallOTPS failed", e);
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
    long IHA_GetAppInstId( String strAppName ) {
	long status = 0;
	Log.d(TAG, "IHA_GetAppInstId");

	if (strAppName.length() == 0 || strAppName.length() > MAX_APPNAME_LENGTH) {
	    IHAManagerError = INVALID_INPUT;
	    throw new RuntimeException("Invalid App Name argument");
	}
	try {

	    status = this.service.IHA_GetAppInstId( strAppName );
	    // Check for IHA Error from last IHA ISepService call.
	    int err = IHAGetLastError();
	    if (err != IHA_RET_S_OK) {
		throw new IhaException("IHA_GetAppInstId failed", err);
	    }

	} catch (RemoteException e) {
	    e.printStackTrace();
	    throw new RuntimeException("IHA_GetAppInstId failed", e);
	}
	return status;
    }
    /**
     * This function is used to get the last error from IHA library on
     * the server.
     *
     * @return         Error value in Integer type
     *****************************************************************************/
    public int IHAGetLastError() {
	int status;
	try {
	    if (IHAManagerError != IHA_RET_S_OK) {
		status = IHAManagerError;
		IHAManagerError = IHA_RET_S_OK;
		return status;
	    } else {
		IHAManagerError = this.service.IHAGetLastError();
	    }
        } catch (RemoteException e) {
	    Log.d(TAG, "IHAGetLastError threw a RemoteException");
	    e.printStackTrace();
	    throw new RuntimeException("IHA_GetLastError failed", e);
        }
	return IHAManagerError;
    }
}
