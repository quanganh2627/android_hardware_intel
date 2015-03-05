package intel.aidltest.service;

import intel.aidltest.R;
import intel.aidltest.Unipoint_ServiceActivity;
import android.app.NotificationManager;
import android.content.Context;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.os.Handler;
import android.util.Log;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.view.WindowManager.LayoutParams;
import android.widget.LinearLayout;
import android.widget.TextView;

public class NotificationDialog {

	private static final String TAG = "Unipoint-NotificationDialog";
	private Context context;
	private NotificationManager notificationManager;
	private WindowManager mWindowManager;
	private LayoutInflater inflateManager;
	private View mNotificationDialog;
	private static boolean mShowing;
	public Handler mUIthreadHandler;

	//This runnable is used to hide the dialog from screen. 
	private final class RemoveDialogRunnable implements Runnable {
		public void run() {
			hide();

		}
	}

	public RemoveDialogRunnable mRemoveWindowRunnable = new RemoveDialogRunnable();

	public NotificationDialog(Context context) {
		this.context = context;

		notificationManager = (NotificationManager) context
				.getSystemService(context.NOTIFICATION_SERVICE);
		mWindowManager = (WindowManager) context
				.getSystemService(Context.WINDOW_SERVICE);

		inflateManager = (LayoutInflater) context
				.getSystemService(Context.LAYOUT_INFLATER_SERVICE);

		mUIthreadHandler = new Handler();

	}

	// IMPORTANT: shows how to do window pop up in Service
	public void show(String message, int duration, int mcolor, int mtextsize) {
		try {

			if (null == mNotificationDialog) {
				mNotificationDialog = (View) inflateManager.inflate(
						R.layout.unipointnotificationtoast, null);
				// Set the window params and set it to TYPE_TOAST .
				WindowManager.LayoutParams lp = new WindowManager.LayoutParams(
						LayoutParams.WRAP_CONTENT,
						LayoutParams.WRAP_CONTENT,
						WindowManager.LayoutParams.TYPE_TOAST,
						WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
								| WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
						PixelFormat.OPAQUE);

				lp.gravity = Gravity.BOTTOM;
				// lp.x = 0;
				// lp.y = 0;

				try {
					mWindowManager.addView(mNotificationDialog, lp);
				} catch (Exception e) {
					if (Unipoint_ServiceActivity.bDEBUG)
						Log.v(TAG,"Add View got exception, may already added ");
				}

				mNotificationDialog.setVisibility(View.INVISIBLE);

				((LinearLayout) mNotificationDialog).setGravity(Gravity.TOP);

			}

			mNotificationDialog.setVisibility(View.INVISIBLE);

			if (!mShowing) {
				mShowing = true;
			}
			mNotificationDialog.setVisibility(View.VISIBLE);

			// It is shown already
			TextView descriptions = (TextView) mNotificationDialog
					.findViewById(R.id.notificationdescription);
			descriptions.setText(message);
			descriptions.setTextColor(mcolor);
			descriptions.setTextSize(mtextsize);

			if (Unipoint_ServiceActivity.bDEBUG)
				Log.v(TAG, "Start to Show the dialog, context :" + message
						+ " textsize : " + mtextsize + " Color :" + mcolor);

			mUIthreadHandler.removeCallbacks(mRemoveWindowRunnable);

			boolean ret = mUIthreadHandler.postDelayed(mRemoveWindowRunnable,
					duration);
			if (ret) {
				if (Unipoint_ServiceActivity.bDEBUG)
					Log.v(TAG,
							"Add delayed callbakc, duration  Successfully"
									+ duration);

			} else {
				if (Unipoint_ServiceActivity.bDEBUG)
					Log.v(TAG, "Add delayed callbakc, duration  Failed"
							+ duration);

			}

		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	public void hide() {

		if (mNotificationDialog != null) {
			mNotificationDialog.setVisibility(View.INVISIBLE);

		}
		if (mShowing) {

			mShowing = false;

		}

	}

}
