/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.intel.umse.demo;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.view.Window;
import android.opengl.GLSurfaceView;
import java.util.Random;

public class Kube extends Activity implements KubeRenderer.AnimationCallback {
	static private final String TAG = "Kube";
	static private void log(String mesg) {
		Log.d(TAG, mesg);
	}
	
	private GLWorld makeGLWorld() {
		GLWorld world = new GLWorld();

		int one = 0x10000;
		int half = 0x08000;
		GLColor red = new GLColor(one, 0, 0);
		GLColor green = new GLColor(0, one, 0);
		GLColor blue = new GLColor(0, 0, one);
		GLColor yellow = new GLColor(one, one, 0);
		GLColor orange = new GLColor(one, half, 0);
		GLColor white = new GLColor(one, one, one);
		GLColor black = new GLColor(0, 0, 0);

		// coordinates for our cubes
		float c0 = -1.0f;
		float c1 = -0.38f;
		float c2 = -0.32f;
		float c3 = 0.32f;
		float c4 = 0.38f;
		float c5 = 1.0f;

		// top back, left to right
		mCubes[0] = new Cube(world, c0, c4, c0, c1, c5, c1);
		mCubes[1] = new Cube(world, c2, c4, c0, c3, c5, c1);
		mCubes[2] = new Cube(world, c4, c4, c0, c5, c5, c1);
		// top middle, left to right
		mCubes[3] = new Cube(world, c0, c4, c2, c1, c5, c3);
		mCubes[4] = new Cube(world, c2, c4, c2, c3, c5, c3);
		mCubes[5] = new Cube(world, c4, c4, c2, c5, c5, c3);
		// top front, left to right
		mCubes[6] = new Cube(world, c0, c4, c4, c1, c5, c5);
		mCubes[7] = new Cube(world, c2, c4, c4, c3, c5, c5);
		mCubes[8] = new Cube(world, c4, c4, c4, c5, c5, c5);
		// middle back, left to right
		mCubes[9] = new Cube(world, c0, c2, c0, c1, c3, c1);
		mCubes[10] = new Cube(world, c2, c2, c0, c3, c3, c1);
		mCubes[11] = new Cube(world, c4, c2, c0, c5, c3, c1);
		// middle middle, left to right
		mCubes[12] = new Cube(world, c0, c2, c2, c1, c3, c3);
		mCubes[13] = null;
		mCubes[14] = new Cube(world, c4, c2, c2, c5, c3, c3);
		// middle front, left to right
		mCubes[15] = new Cube(world, c0, c2, c4, c1, c3, c5);
		mCubes[16] = new Cube(world, c2, c2, c4, c3, c3, c5);
		mCubes[17] = new Cube(world, c4, c2, c4, c5, c3, c5);
		// bottom back, left to right
		mCubes[18] = new Cube(world, c0, c0, c0, c1, c1, c1);
		mCubes[19] = new Cube(world, c2, c0, c0, c3, c1, c1);
		mCubes[20] = new Cube(world, c4, c0, c0, c5, c1, c1);
		// bottom middle, left to right
		mCubes[21] = new Cube(world, c0, c0, c2, c1, c1, c3);
		mCubes[22] = new Cube(world, c2, c0, c2, c3, c1, c3);
		mCubes[23] = new Cube(world, c4, c0, c2, c5, c1, c3);
		// bottom front, left to right
		mCubes[24] = new Cube(world, c0, c0, c4, c1, c1, c5);
		mCubes[25] = new Cube(world, c2, c0, c4, c3, c1, c5);
		mCubes[26] = new Cube(world, c4, c0, c4, c5, c1, c5);

		// paint the sides
		int i, j;
		// set all faces black by default
		for (i = 0; i < 27; i++) {
			Cube cube = mCubes[i];
			if (cube != null) {
				for (j = 0; j < 6; j++)
					cube.setFaceColor(j, black);
			}
		}

		// paint top
		for (i = 0; i < 9; i++)
			mCubes[i].setFaceColor(Cube.kTop, orange);
		// paint bottom
		for (i = 18; i < 27; i++)
			mCubes[i].setFaceColor(Cube.kBottom, red);
		// paint left
		for (i = 0; i < 27; i += 3)
			mCubes[i].setFaceColor(Cube.kLeft, yellow);
		// paint right
		for (i = 2; i < 27; i += 3)
			mCubes[i].setFaceColor(Cube.kRight, white);
		// paint back
		for (i = 0; i < 27; i += 9)
			for (j = 0; j < 3; j++)
				mCubes[i + j].setFaceColor(Cube.kBack, blue);
		// paint front
		for (i = 6; i < 27; i += 9)
			for (j = 0; j < 3; j++)
				mCubes[i + j].setFaceColor(Cube.kFront, green);

		for (i = 0; i < 27; i++)
			if (mCubes[i] != null)
				world.addShape(mCubes[i]);

		// initialize our permutation to solved position
		mPermutation = new int[27];
		for (i = 0; i < mPermutation.length; i++)
			mPermutation[i] = i;

		createLayers();
		updateLayers();

		world.generate();

		return world;
	}

	private void createLayers() {
		mLayers[kUp] = new Layer(Layer.kAxisY);
		mLayers[kDown] = new Layer(Layer.kAxisY);
		mLayers[kLeft] = new Layer(Layer.kAxisX);
		mLayers[kRight] = new Layer(Layer.kAxisX);
		mLayers[kFront] = new Layer(Layer.kAxisZ);
		mLayers[kBack] = new Layer(Layer.kAxisZ);
		mLayers[kMiddle] = new Layer(Layer.kAxisX);
		mLayers[kEquator] = new Layer(Layer.kAxisY);
		mLayers[kSide] = new Layer(Layer.kAxisZ);
	}

	private void updateLayers() {
		Layer layer;
		GLShape[] shapes;
		int i, j, k;

		// up layer
		layer = mLayers[kUp];
		shapes = layer.mShapes;
		for (i = 0; i < 9; i++)
			shapes[i] = mCubes[mPermutation[i]];

		// down layer
		layer = mLayers[kDown];
		shapes = layer.mShapes;
		for (i = 18, k = 0; i < 27; i++)
			shapes[k++] = mCubes[mPermutation[i]];

		// left layer
		layer = mLayers[kLeft];
		shapes = layer.mShapes;
		for (i = 0, k = 0; i < 27; i += 9)
			for (j = 0; j < 9; j += 3)
				shapes[k++] = mCubes[mPermutation[i + j]];

		// right layer
		layer = mLayers[kRight];
		shapes = layer.mShapes;
		for (i = 2, k = 0; i < 27; i += 9)
			for (j = 0; j < 9; j += 3)
				shapes[k++] = mCubes[mPermutation[i + j]];

		// front layer
		layer = mLayers[kFront];
		shapes = layer.mShapes;
		for (i = 6, k = 0; i < 27; i += 9)
			for (j = 0; j < 3; j++)
				shapes[k++] = mCubes[mPermutation[i + j]];

		// back layer
		layer = mLayers[kBack];
		shapes = layer.mShapes;
		for (i = 0, k = 0; i < 27; i += 9)
			for (j = 0; j < 3; j++)
				shapes[k++] = mCubes[mPermutation[i + j]];

		// middle layer
		layer = mLayers[kMiddle];
		shapes = layer.mShapes;
		for (i = 1, k = 0; i < 27; i += 9)
			for (j = 0; j < 9; j += 3)
				shapes[k++] = mCubes[mPermutation[i + j]];

		// equator layer
		layer = mLayers[kEquator];
		shapes = layer.mShapes;
		for (i = 9, k = 0; i < 18; i++)
			shapes[k++] = mCubes[mPermutation[i]];

		// side layer
		layer = mLayers[kSide];
		shapes = layer.mShapes;
		for (i = 3, k = 0; i < 27; i += 9)
			for (j = 0; j < 3; j++)
				shapes[k++] = mCubes[mPermutation[i + j]];
	}

//	private GestureDetector mGestureDetector;


//	private void initGestureDetector(Context context) {
//		mGestureDetector = new GestureDetector(context,
//				new GestureDetector.SimpleOnGestureListener() {
//					private final double normalize(double v, double f) {
//						if (v > 50.0f)
//							v = 50.0f;
//						if (v < -50.0f)
//							v = -50.0f;
//						return v / f;
//					}
//
//					@Override
//					public boolean onFling(MotionEvent e1, MotionEvent e2,
//							float velocityX, float velocityY) {
//						mXRotationSpeed = normalize(velocityX, 10.0f);
//						// mYRotationSpeed = normalize(velocityY, 20.0f);
//						return super.onFling(e1, e2, velocityX, velocityY);
//					}
//
//					@Override
//					public boolean onDown(MotionEvent e) {
//						mXRotationSpeed = mYRotationSpeed = 0.0f;
//						return super.onDown(e);
//					}
//				});
//	}
	
	private double mXRotationSpeed;
	private double mYRotationSpeed;
	@Override
	public boolean onTrackballEvent(MotionEvent event) {
		float rx = event.getX() * 3.5f;
		float ry = event.getY() * 2.5f;
		log("event(x, y) (" + rx + ", " + ry + ")\n");
		mXRotationSpeed += rx;
		mYRotationSpeed -= ry;
//		return super.onTrackballEvent(event);
		return true;
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		float rx = 0.0f;
		float ry = 0.0f;
		if (keyCode == KeyEvent.KEYCODE_DPAD_DOWN) {
			ry = -8.0f;
			log("DPAD_DOWN_KEY is pressed\n");
		} else if (keyCode == KeyEvent.KEYCODE_DPAD_UP) {
			ry = 8.0f;
			log("DPAD_UP_KEY is pressed\n");
		} else if (keyCode == KeyEvent.KEYCODE_DPAD_LEFT) {
			rx  = -8.0f;
			log("DPAD_LEFT_KEY is pressed\n");
		} else if (keyCode == KeyEvent.KEYCODE_DPAD_RIGHT) {
			rx = 8.0f;
			log("DPAD_RIGHT_KEY is pressed\n");
		}

		mXRotationSpeed += rx;
		mYRotationSpeed += ry;

		return super.onKeyDown(keyCode, event);
	}

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		// We don't need a title either.
		requestWindowFeature(Window.FEATURE_NO_TITLE);

		mView = new GLSurfaceView(getApplication());
		mRenderer = new KubeRenderer(makeGLWorld(), this);
		mView.setRenderer(mRenderer);
		setContentView(mView);
		mXRotationSpeed = mYRotationSpeed = 0.0f;
//		initGestureDetector(getApplicationContext());
//
//		startService(new Intent(this, UnipointService.class));
//		
//		IntentFilter gesFilter = new IntentFilter();
//		gesFilter.addAction(UnipointService.GESTURE_ACTION);
//		registerReceiver(new BroadcastReceiver() {
//			@Override
//			public void onReceive(Context context, Intent intent) {
//				String action = intent.getAction();
//				Log.e("Kube", action);
//				if (action.equals(UnipointService.GESTURE_ACTION)) {
//					int key = intent.getIntExtra(UnipointService.GESTURE_KEY, -1);
//					int strength = intent.getIntExtra(UnipointService.GESTURE_STRENGTH, -1);
//					Log.e("key", "@" + key);
//					Log.e("strength", "@" + strength);
//					if (key != -1 && strength != -1) {
//						if (key == UnipointService.KEY_GESTURE_DOWN) {
//							mYRotationSpeed = 3.0d;
//							mXRotationSpeed = 0.0d;
//						} else if (key == UnipointService.KEY_GESTURE_UP) {
//							mYRotationSpeed = -3.0d;
//							mXRotationSpeed = 0.0d;
//						} else if (key == UnipointService.KEY_GESTURE_LEFT) {
//							mXRotationSpeed = -3.0d;
//							mYRotationSpeed = 0.0d;
//						} else if (key == UnipointService.KEY_GESTURE_RIGHT) {
//							mXRotationSpeed = 3.0d;
//							mYRotationSpeed = 0.0d;
//						}
//					}
//				}				
//			}			
//		}, gesFilter);
	}

	@Override
	protected void onDestroy() {
//		stopService(new Intent(this, UnipointService.class));
		super.onDestroy();
	}

	@Override
	protected void onResume() {
		super.onResume();
		mView.onResume();
	}

	@Override
	protected void onPause() {
		super.onPause();
		mView.onPause();
	}

	public void animate() {
		// change our angle of view
		mRenderer.setXAngle(mRenderer.getXAngle() + mXRotationSpeed);
		if (mXRotationSpeed > 0)
			mXRotationSpeed = Math.max(0, mXRotationSpeed - 0.05);
		else
			mXRotationSpeed = Math.min(0, mXRotationSpeed + 0.05);			

//		mXRotationSpeed = (mXRotationSpeed > 0 ? 1.0f : -1.0f)
//				* ((Math.abs(mXRotationSpeed) > 0.03) ? (Math
//						.abs(mXRotationSpeed) - 0.03) : 0.0f);
		mRenderer.setYAngle(mRenderer.getYAngle() + mYRotationSpeed);
		if (mYRotationSpeed > 0)
			mYRotationSpeed = Math.max(0, mYRotationSpeed - 0.05);
		else
			mYRotationSpeed = Math.min(0, mYRotationSpeed + 0.05);			

//		mYRotationSpeed = (mYRotationSpeed > 0 ? 1.0f : -1.0f)
//		* ((Math.abs(mYRotationSpeed) > 0.03) ? (Math
//				.abs(mYRotationSpeed) - 0.03) : 0.0f);

/*
		if (mCurrentLayer == null) {
			int layerID = mRandom.nextInt(9);
			mCurrentLayer = mLayers[layerID];
			mCurrentLayerPermutation = mLayerPermutations[layerID];
			mCurrentLayer.startAnimation();
			boolean direction = mRandom.nextBoolean();
			int count = mRandom.nextInt(3) + 1;

			count = 1;
			direction = false;
			mCurrentAngle = 0;
			if (direction) {
				mAngleIncrement = (float) Math.PI / 50;
				mEndAngle = mCurrentAngle + ((float) Math.PI * count) / 2f;
			} else {
				mAngleIncrement = -(float) Math.PI / 50;
				mEndAngle = mCurrentAngle - ((float) Math.PI * count) / 2f;
			}
		}

		mCurrentAngle += mAngleIncrement;

		if ((mAngleIncrement > 0f && mCurrentAngle >= mEndAngle)
				|| (mAngleIncrement < 0f && mCurrentAngle <= mEndAngle)) {
			mCurrentLayer.setAngle(mEndAngle);
			mCurrentLayer.endAnimation();
			mCurrentLayer = null;

			// adjust mPermutation based on the completed layer rotation
			int[] newPermutation = new int[27];
			for (int i = 0; i < 27; i++) {
				newPermutation[i] = mPermutation[mCurrentLayerPermutation[i]];
				// newPermutation[i] =
				// mCurrentLayerPermutation[mPermutation[i]];
			}
			mPermutation = newPermutation;
			updateLayers();

		} else {
			mCurrentLayer.setAngle(mCurrentAngle);
		}
*/
	}

	GLSurfaceView mView;
	KubeRenderer mRenderer;
	Cube[] mCubes = new Cube[27];
	// a Layer for each possible move
	Layer[] mLayers = new Layer[9];
	// permutations corresponding to a pi/2 rotation of each layer about its
	// axis
	static int[][] mLayerPermutations = {
			// permutation for UP layer
			{ 2, 5, 8, 1, 4, 7, 0, 3, 6, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
					19, 20, 21, 22, 23, 24, 25, 26 },
			// permutation for DOWN layer
			{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 20,
					23, 26, 19, 22, 25, 18, 21, 24 },
			// permutation for LEFT layer
			{ 6, 1, 2, 15, 4, 5, 24, 7, 8, 3, 10, 11, 12, 13, 14, 21, 16, 17,
					0, 19, 20, 9, 22, 23, 18, 25, 26 },
			// permutation for RIGHT layer
			{ 0, 1, 8, 3, 4, 17, 6, 7, 26, 9, 10, 5, 12, 13, 14, 15, 16, 23,
					18, 19, 2, 21, 22, 11, 24, 25, 20 },
			// permutation for FRONT layer
			{ 0, 1, 2, 3, 4, 5, 24, 15, 6, 9, 10, 11, 12, 13, 14, 25, 16, 7,
					18, 19, 20, 21, 22, 23, 26, 17, 8 },
			// permutation for BACK layer
			{ 18, 9, 0, 3, 4, 5, 6, 7, 8, 19, 10, 1, 12, 13, 14, 15, 16, 17,
					20, 11, 2, 21, 22, 23, 24, 25, 26 },
			// permutation for MIDDLE layer
			{ 0, 7, 2, 3, 16, 5, 6, 25, 8, 9, 4, 11, 12, 13, 14, 15, 22, 17,
					18, 1, 20, 21, 10, 23, 24, 19, 26 },
			// permutation for EQUATOR layer
			{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 11, 14, 17, 10, 13, 16, 9, 12, 15, 18,
					19, 20, 21, 22, 23, 24, 25, 26 },
			// permutation for SIDE layer
			{ 0, 1, 2, 21, 12, 3, 6, 7, 8, 9, 10, 11, 22, 13, 4, 15, 16, 17,
					18, 19, 20, 23, 14, 5, 24, 25, 26 } };

//	@Override
//	public boolean onTouchEvent(MotionEvent event) {
//		boolean retValue = mGestureDetector.onTouchEvent(event);
//		return retValue;
//	}

	// current permutation of starting position
	int[] mPermutation;

	// for random cube movements
	Random mRandom = new Random(System.currentTimeMillis());
	// currently turning layer
	Layer mCurrentLayer = null;
	// current and final angle for current Layer animation
	float mCurrentAngle, mEndAngle;
	// amount to increment angle
	float mAngleIncrement;
	int[] mCurrentLayerPermutation;

	// names for our 9 layers (based on notation from
	// http://www.cubefreak.net/notation.html)
	static final int kUp = 0;
	static final int kDown = 1;
	static final int kLeft = 2;
	static final int kRight = 3;
	static final int kFront = 4;
	static final int kBack = 5;
	static final int kMiddle = 6;
	static final int kEquator = 7;
	static final int kSide = 8;

}
