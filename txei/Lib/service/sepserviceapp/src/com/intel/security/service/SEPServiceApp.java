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

import android.app.Application;
import android.os.ServiceManager;
import android.util.Log;
import com.intel.security.service.ISEPService;

public class SEPServiceApp extends Application {
    private static final String TAG = "SEPServiceApp";
    private static final String REMOTE_SERVICE_NAME = ISEPService.class.getName();
    private ISEPServiceImpl serviceImpl;

    public void onCreate() {
	super.onCreate();
	this.serviceImpl = new ISEPServiceImpl(this);
	ServiceManager.addService(REMOTE_SERVICE_NAME, this.serviceImpl);
	Log.d(TAG, "Registered [" + serviceImpl.getClass().getName() + "] as [" + REMOTE_SERVICE_NAME + "]");
    }

    public void onTerminate() {
	super.onTerminate();
	Log.d(TAG, "Terminated");
    }
}