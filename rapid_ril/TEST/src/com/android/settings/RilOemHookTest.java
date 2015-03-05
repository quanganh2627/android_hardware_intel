//
// Copyright 2011 Intrinsyc Software International, Inc.  All rights reserved.
//


package com.android.RILTest;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneFactory;
import com.android.internal.telephony.PhoneBase;
import com.android.internal.telephony.PhoneProxy;
import com.android.internal.telephony.CommandsInterface;
import android.os.Message;
import android.os.Handler;
import android.os.AsyncResult;
import android.util.Log;
import android.app.AlertDialog;
import java.nio.ByteBuffer;




public class RilOemHookTest extends Activity
{
    private static final String LOG_TAG = "RILOemHookTestApp";

    private RadioButton mRadioButtonAPI1 = null;

    private RadioGroup mRadioGroupAPI = null;

    private Phone mPhone = null;
    private CommandsInterface mCM;

    private static final int EVENT_RIL_OEM_HOOK_RAW_COMPLETE = 1300;
    private static final int EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE = 1310;
    private static final int EVENT_UNSOL_RIL_OEM_HOOK_RAW = 500;

    @Override
    public void onCreate(Bundle icicle)
    {
        super.onCreate(icicle);
        setContentView(R.layout.riloemhook_layout);

        mRadioButtonAPI1 = (RadioButton) findViewById(R.id.radio_api1);

        mRadioGroupAPI = (RadioGroup) findViewById(R.id.radio_group_api);

        //  Initially turn on first button.
        mRadioButtonAPI1.toggle();

        //  Get our main phone object.
        mPhone = ((PhoneProxy)PhoneFactory.getDefaultPhone()).getActivePhone();

        //  Register for OEM raw notification.
        mCM = ((PhoneBase)mPhone).mCi;
    }


    @Override
    public void onPause()
    {
        super.onPause();

        log("onPause()");

        //  Unregister for OEM raw notification.
        mCM.unSetOnUnsolOemHookRaw(mHandler);
    }

    @Override
    public void onResume()
    {
        super.onResume();

        log("onResume()");

        //  Register for OEM raw notification.
        mCM.setOnUnsolOemHookRaw(mHandler, EVENT_UNSOL_RIL_OEM_HOOK_RAW, null);
    }

    public void onRun(View view)
    {
        //  Get the checked button
        int idButtonChecked = mRadioGroupAPI.getCheckedRadioButtonId();

        byte[] oemhook = null;

        switch(idButtonChecked)
        {
            case R.id.radio_api1:
            {
                // RIL_OEM_HOOK_STRING_THERMAL_GET_SENSOR
                //  AT+XDRV=5,9,<sensor_id>

                // data: <command id>, <sensor id>
                String[] data = { "162", "2" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api2:
            {
                // RIL_OEM_HOOK_STRING_THERMAL_SET_THRESHOLD
                //  AT+XDRV=5,14,<activate>,<sensor_id>,<min_threshold>,<max_threshold>

                // data: <command id>, <activate>, <sensor_id>, <minThersholdTemp>,
                // <maxThersholdTemp>
                String[] data = { "163", "true 2 2000 5000" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api3:
            {
                // RIL_OEM_HOOK_STRING_SET_MODEM_AUTO_FAST_DORMANCY
                //  AT+XFDOR=<enable>,<delay_timer>

                // data: <command id>, <activate>, <delay_timer>
                String[] data = { "164", "true 20 20" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api4:
            {
                // RIL_OEM_HOOK_STRING_GET_ATR
                // AT+XGATR

                // data: <command id>
                String[] data = { "165" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api5:
            {
                // RIL_OEM_HOOK_STRING_GET_GPRS_CELL_ENV
                // AT+CGED=0

                // data: <command id>
                String[] data = { "166" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api6:
            {
                // RIL_OEM_HOOK_STRING_DEBUG_SCREEN_COMMAND
                // AT+XCGEDPAGE=0

                // data: <command id>
                String[] data = { "167" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api7:
            {
                // RIL_OEM_HOOK_STRING_RELEASE_ALL_CALLS
                // AT+CHLD=8

                // data: <command id>
                String[] data = { "168" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api8:
            {
                // RIL_OEM_HOOK_STRING_GET_SMS_TRANSPORT_MODE
                // AT+CGSMS?

                // data: <command id>
                String[] data = { "169" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api9:
            {
                // RIL_OEM_HOOK_STRING_SET_SMS_TRANSPORT_MODE
                // AT+CGSMS=<service>

                // data: <command id>, <service>
                String[] data = { "170", "2" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api10:
            {
                // RIL_OEM_HOOK_STRING_GET_RF_POWER_CUTBACK_TABLE
                // AT+XRFCBT?

                // data: <command id>
                String[] data = { "171" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api11:
            {
                // RIL_OEM_HOOK_STRING_SET_RF_POWER_CUTBACK_TABLE
                // AT+XRFCBT=<Offset Table Index>

                // data: <command id>, <Offset Table Index>
                String[] data = { "172", "0" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api12:
            {
                // RIL_OEM_HOOK_STRING_SET_RF_POWER_CUTBACK_TABLE
                // AT+XRFCBT=<Offset Table Index>

                // data: <command id>, <Offset Table Index>
                String[] data = { "172", "1" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            case R.id.radio_api13:
            {
                // RIL_OEM_HOOK_STRING_SET_RF_POWER_CUTBACK_TABLE
                // AT+XRFCBT=<Offset Table Index>

                // data: <command id>, <Offset Table Index>
                String[] data = { "172", "2" };

                Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE);
                mPhone.invokeOemRilRequestStrings(data, msg);
            }
            break;

            default:
                log("unknown button selected");
                break;
        }


//        Message msg = mHandler.obtainMessage(EVENT_RIL_OEM_HOOK_COMMAND_COMPLETE);
//        mPhone.invokeOemRilRequestRaw(oemhook, msg);

    }

    private void logRilOemHookRawResponse(AsyncResult ar)
    {
        log("received oem hook raw response");

        String str = new String("");


        if (ar.exception != null)
        {
            log("Exception:" + ar.exception);
            str += "Exception:" + ar.exception + "\n\n";
        }

        if (ar.result != null)
        {
            byte[] oemResponse = (byte[])ar.result;
            int size = oemResponse.length;

            log("oemResponse length=[" + Integer.toString(size) + "]");
            str += "oemResponse length=[" + Integer.toString(size) + "]" + "\n";

            if (size > 0)
            {
                for (int i=0; i<size; i++)
                {
                    byte myByte = oemResponse[i];
                    int myInt = (int)(myByte & 0x000000FF);
                    log("oemResponse[" + Integer.toString(i) + "]=[0x"
                            + Integer.toString(myInt,16) + "]");
                    str += "oemResponse[" + Integer.toString(i) + "]=[0x"
                            + Integer.toString(myInt,16) + "]" + "\n";
                }
            }
        }
        else
        {
            log("received NULL oem hook raw response");
            str += "received NULL oem hook raw response";
        }


        //  Display message box
        AlertDialog.Builder builder = new AlertDialog.Builder(this);

        builder.setMessage(str);
        builder.setPositiveButton("OK", null);

        AlertDialog alert = builder.create();
        alert.show();
    }

    private void logRilOemHookStringsResponse(AsyncResult ar)
    {
        log("received oem hook strings response");

        String str = new String("");


        if (ar.exception != null)
        {
            log("Exception:" + ar.exception);
            str += "Exception:" + ar.exception + "\n\n";
        }

        if (ar.result != null)
        {
            String[] oemResponse = (String[])ar.result;
            int size = oemResponse.length;

            log("oemResponse length=[" + Integer.toString(size) + "]");
            str += "oemResponse length=[" + Integer.toString(size) + "]" + "\n";

            if (size > 0)
            {
                for (int i=0; i<size; i++)
                {
                    log("oemResponse[" + Integer.toString(i) + "]=[" + oemResponse[i] + "]");
                    str += "oemResponse[" + Integer.toString(i) + "]=[" + oemResponse[i] + "]"
                            + "\n";
                }
            }
        }
        else
        {
            log("received NULL oem hook strings response");
            str += "received NULL oem hook strings response";
        }


        //  Display message box
        AlertDialog.Builder builder = new AlertDialog.Builder(this);

        builder.setMessage(str);
        builder.setPositiveButton("OK", null);

        AlertDialog alert = builder.create();
        alert.show();
    }

    private void log(String msg)
    {
        Log.d(LOG_TAG, "[RIL_HOOK_OEM_TESTAPP] " + msg);
    }

    private Handler mHandler = new Handler()
    {
        public void handleMessage(Message msg)
        {
            AsyncResult ar;
            switch (msg.what)
            {
                case EVENT_RIL_OEM_HOOK_RAW_COMPLETE:
                    ar = (AsyncResult) msg.obj;
                    logRilOemHookRawResponse(ar);
                    break;

                case EVENT_RIL_OEM_HOOK_STRINGS_COMPLETE:
                    ar = (AsyncResult) msg.obj;
                    logRilOemHookStringsResponse(ar);
                    break;

                case EVENT_UNSOL_RIL_OEM_HOOK_RAW:
                    break;
            }
        }
    };

}
