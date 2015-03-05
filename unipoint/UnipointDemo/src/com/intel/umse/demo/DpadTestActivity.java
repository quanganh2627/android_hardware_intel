package com.intel.umse.demo;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.widget.EditText;
import android.widget.TextView;

public class DpadTestActivity extends Activity {
	static private final String TAG = "DpadTestActivity";
	static private void log(String mesg) {
		Log.d(TAG, mesg);
	}

//	String mLogText;
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main);
//		mLogText = "";
//		mLogText = (EditText) findViewById(R.id.LogText);
	}
	
	@Override
	public boolean onTrackballEvent(MotionEvent event) {
		return super.onTrackballEvent(event);
//		return true;
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		String str = null;
		if (keyCode == KeyEvent.KEYCODE_DPAD_DOWN)
			str = "DPAD_DOWN_KEY is pressed\n";
		if (keyCode == KeyEvent.KEYCODE_DPAD_UP)
			str = "DPAD_UP_KEY is pressed\n";
		if (keyCode == KeyEvent.KEYCODE_DPAD_LEFT)
			str = "DPAD_LEFT_KEY is pressed\n";
		if (keyCode == KeyEvent.KEYCODE_DPAD_RIGHT)
			str = "DPAD_RIGHT_KEY is pressed\n";
		if (str != null) {
			log(str);
			EditText edit = (EditText) findViewById(R.id.LogText);
			edit.append(str);
		}
		return super.onKeyDown(keyCode, event);
	}
}
