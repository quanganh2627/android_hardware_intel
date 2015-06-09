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

import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;

/**
 * @author nshuster
 *
 */
public class SEPManager {
    private static final String TAG = SEPManager.class.getSimpleName();
    private static final String REMOTE_SERVICE_NAME = ISEPService.class.getName();
    private static boolean isInitialized = false;
    ISEPService service;

    public static SEPManager getInstance() {
	Log.i(TAG, "SEPManager.getInstance()");
	return new SEPManager();
    }


    private SEPManager() {
	Log.d(TAG, "Connecting to ISEPService by name [" + REMOTE_SERVICE_NAME + "]");
	this.service = ISEPService.Stub.asInterface(ServiceManager.getService(REMOTE_SERVICE_NAME));
	if (this.service == null) {
	    throw new IllegalStateException("Failed to find ISEPService by name [" + REMOTE_SERVICE_NAME + "]");
	}
    }

    public void initLibrary() {

	Log.i(TAG, "initLibrary");
	if (!isInitialized) {
	    try {
		this.service.initDrmLibrary();
		isInitialized = true;
	    } catch (RemoteException e) {
		e.printStackTrace();
		throw new RuntimeException("initLibrary failed");
	    }
	}


    }

    public long getRandomNumber() {

	Log.i(TAG, "getRandomNumber");
	try {

	    initLibrary();

	    return this.service.getRandomNumber();
	} catch (RemoteException e) {
	    e.printStackTrace();
	    throw new RuntimeException("getRandomNumber failed", e);
	}
    }

}
