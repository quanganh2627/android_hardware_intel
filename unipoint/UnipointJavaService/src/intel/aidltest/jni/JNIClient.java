package intel.aidltest.jni;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;
import intel.aidltest.UnipointMode;
import intel.aidltest.Unipoint_ServiceActivity;
import intel.aidltest.service.UnipointService;

public final class JNIClient {
	public static final String TAG = "Unipoint-JNICLient";

	public static native int getCurrentMode();

	public static native int checkConnection();

	public static native int ReConnectServer();

	public static native int SetMode(int mode);

	public static final String SERVICEACTION = "intel.aidltest.service.UnipointService";
	public static Context context;

	public JNIClient(Context context) {
		this.context = context;

	}

	public static int CallBackModeSwitchRequest(int newmode) {

		// PopUp notification bar to user ;

		// UnipointService.CallBackModeSwitchRequest(newmode);
		if(Unipoint_ServiceActivity.bDEBUG)
			Log.v(TAG, "CallBackModeSwitchRequest, new mode is " + newmode);

		Intent b = new Intent(SERVICEACTION);
		Bundle bundle = new Bundle();

		bundle.putInt(Unipoint_ServiceActivity.COMMAND,
				Unipoint_ServiceActivity.CMD_NOTIFY);

		bundle.putInt(Unipoint_ServiceActivity.PAR_NEWMODE, newmode);

		b.putExtras(bundle);

		if (null == context) {
			if (Unipoint_ServiceActivity.bDEBUG)
				Log.v(TAG, "context is null");
			return 0;
		} else {

			if (Unipoint_ServiceActivity.bDEBUG)
				Log.v(TAG, "Call Service function ");
			context.startService(b);

		}

		return 0;
	}
}
