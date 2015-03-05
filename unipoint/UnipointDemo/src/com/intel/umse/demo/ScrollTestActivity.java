package com.intel.umse.demo;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Bundle;
import android.util.Log;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.view.View;

public class ScrollTestActivity extends Activity {
	static private final String TAG = "ScrollTestActivity";
	static private void log(String mesg) {
		Log.d(TAG, mesg);
	}

	ScrollView mView;
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		mView = new ScrollView(this);
		setContentView(mView);
	}

	@Override
	public boolean onTrackballEvent(MotionEvent event) {
		mView.setX((int) (mView.getX() + event.getX()*60));
		mView.setY((int) (mView.getY() + event.getY()*50));
		mView.invalidate();
		return true;
	}
	
	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		int offsetX = 0;
		int offsetY = 0;
		if (keyCode == KeyEvent.KEYCODE_DPAD_DOWN) {
			offsetY = -200;
			log("DPAD_DOWN_KEY is pressed\n");
		} else if (keyCode == KeyEvent.KEYCODE_DPAD_UP) {
			offsetY = 200;
			log("DPAD_UP_KEY is pressed\n");
		} else if (keyCode == KeyEvent.KEYCODE_DPAD_LEFT) {
			offsetX = -200;
			log("DPAD_LEFT_KEY is pressed\n");
		} else if (keyCode == KeyEvent.KEYCODE_DPAD_RIGHT) {
			offsetX = 200;
			log("DPAD_RIGHT_KEY is pressed\n");
		}

		mView.setX((int) (mView.getX() + offsetX));
		mView.setY((int) (mView.getY() + offsetY));
		mView.invalidate();

		return super.onKeyDown(keyCode, event);
	}

	private static class ScrollView extends View {
		private int mX;
		private int mY;
		private Bitmap mBackground;
		private int mBgWidth;
		private int mBgHeight;
		private int mViewWidth;
		private int mViewHeight;
		private Paint mPaint;
		public ScrollView(Context context) {
			super(context);
			mBackground = BitmapFactory.decodeResource(context.getResources(), R.drawable.earth, null);
			mBgWidth = mBackground.getWidth();
			mBgHeight = mBackground.getHeight();
			mX = mBgWidth / 2;
			mY = mBgHeight / 2;
			mPaint = new Paint();
		}
		
		public int getX() {
			return mX;
		}
		
		public int getY() {
			return mY;
		}
		
		public void setX(int x) {			
			mX = Math.max(0, Math.min(x, mBgWidth - mViewWidth));
		}
		
		public void setY(int y) {
			mY = Math.max(0, Math.min(y, mBgHeight - mViewHeight));
		}

		@Override
		protected void onDraw(Canvas canvas) {
			log("x=" + mX + ",y=" + mY);
			canvas.drawBitmap(mBackground, new Rect(mX, mY, mX + mViewWidth, mY + mViewHeight), new RectF(0, 0, mViewWidth, mViewHeight), mPaint);
			super.onDraw(canvas);
		}

		@Override
		protected void onSizeChanged(int w, int h, int oldw, int oldh) {
			mViewWidth = w;
			mViewHeight = h;
			super.onSizeChanged(w, h, oldw, oldh);
		}		
	}
}
