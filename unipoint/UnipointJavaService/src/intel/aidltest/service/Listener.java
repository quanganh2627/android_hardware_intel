package intel.aidltest.service;

import intel.aidltest.Unipoint_ServiceActivity;

import java.util.Date;

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;
import android.preference.PreferenceManager;
import android.provider.ContactsContract;
import android.provider.ContactsContract.Contacts;
import android.provider.ContactsContract.PhoneLookup;
import android.telephony.PhoneStateListener;
import android.telephony.SmsMessage;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.view.WindowManager.LayoutParams;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

public class Listener extends BroadcastReceiver {

	private Context context;

	private static final String TAG = "Unipoint.onBootListener";
	private static final String BOOT_COMPLETED_ACTION = "android.intent.action.BOOT_COMPLETED";

	@Override
	public void onReceive(Context context, Intent intent) {

		try {
			this.context = context;

			if (Unipoint_ServiceActivity.bDEBUG)
				Log.v(TAG, "======onReceive=========");
			// Call Received
			if (intent.getAction().equals(BOOT_COMPLETED_ACTION)) {
				StartUnipointNotificationService(intent);

			}
		} catch (Exception e) {
			e.printStackTrace();
		}

	}

	// Start Unipoint Notification Service
	private void StartUnipointNotificationService(Intent intent) {

		Intent b = new Intent(Unipoint_ServiceActivity.SERVICEACTION);

		if (Unipoint_ServiceActivity.bDEBUG)
			Log.v(TAG, "Call Service function ");
		context.startService(b);

	}

}
