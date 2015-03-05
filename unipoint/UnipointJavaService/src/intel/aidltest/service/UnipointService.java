package intel.aidltest.service;

import intel.aidltest.UnipointMode;
import intel.aidltest.Unipoint_ServiceActivity;
import intel.aidltest.aidlsrc.IUnipointService;
import intel.aidltest.jni.JNIClient;
import android.app.Activity;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import android.view.Gravity;
import android.widget.Toast;

public class UnipointService extends Service {

    private static final String TAG = "UnipointService";
    private static int mcurrentmode;
    private static Context self;
    private NotificationDialog mNotificationDialog;
    private boolean threadDisable;
    private JNIClient client;

    static {
        System.loadLibrary("Unipoint");
    }

    @Override
    public IBinder onBind(Intent arg0) {
        // TODO Auto-generated method stub
        return new UnipointSerivceimpl();
    }

    @Override
    public void onDestroy() {

        if (Unipoint_ServiceActivity.bDEBUG)
            Log.v(TAG, "Unipoint Service Stoped");
        super.onDestroy();

        this.threadDisable = true;

    }

    @Override
    public void onCreate() {
        // TODO Auto-generated method stub
        super.onCreate();

        self = this;

        mNotificationDialog = new NotificationDialog(self);
        mcurrentmode = UnipointMode.MODE_NORMAL;

        if (Unipoint_ServiceActivity.bDEBUG)
            Log.v(TAG, "Unipoint Service Started");

        client = new JNIClient(this);

        new Thread(new Runnable() {

            public void run() {
                while (!threadDisable) {
                    try {
                        Thread.sleep(60000);

                        checkConnection();

                    } catch (InterruptedException e) {
                    }
                }

            }

        }).start();

    }

    protected void checkConnection() {

        try {
            int mode = JNIClient.checkConnection();
            if (mode < 0) {

                if (Unipoint_ServiceActivity.bDEBUG)
                    Log.v("DEBUG",
                          "Daemon not connected, Starts to tetry connection ");

                JNIClient.ReConnectServer();
            }

        } catch (Exception e) {
            e.printStackTrace();
        }

    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // TODO Auto-generated method stub

        if (Unipoint_ServiceActivity.bDEBUG)
            Log.v(TAG, "onStartCommand");

        if (null != intent) {

            Bundle b = intent.getExtras();

            if (null != b) {

                if (Unipoint_ServiceActivity.bDEBUG)
                    Log.v("DEBUG", "FOUND BUNDLE inside intent");

                Integer cmd = b.getInt(Unipoint_ServiceActivity.COMMAND);
                if (null != cmd) {
                    switch (cmd) {
                    case Unipoint_ServiceActivity.CMD_NOTIFY: {


                        int newmode = b
                                      .getInt(Unipoint_ServiceActivity.PAR_NEWMODE);
                        String message = "";
                        if (newmode == 0) {
                            message = "Unipoint DISABLED ? We should not get this mode notification. Report ERROR";
                        } else if (newmode == 1) {
                            message = "Unipoint Mode switch detected, new mode : MODE_NORMAL";

                        } else if (newmode == 2) {
                            message = "Unipoint Mode switch detected, new mode : MODE_VOLUME";

                        }
                        if (Unipoint_ServiceActivity.bDEBUG)
                            Log.v(TAG, message);

                        mcurrentmode = newmode;
                        mNotificationDialog
                        .show(message, 2000, Color.WHITE, 18);

                        break;
                    }
                    case Unipoint_ServiceActivity.CMD_RECONNECT: {

                        try {
                            if (Unipoint_ServiceActivity.bDEBUG)
                                Log.v(TAG, "Got CMD_RECONNECT, Start to call JNIClient.ReConnectServer function ");
                            int ret = JNIClient.ReConnectServer();
                            if(ret!=-1)
                            {
                                if (Unipoint_ServiceActivity.bDEBUG)
                                    Log.v(TAG, "JNIClient.ReConnectServer function SUCESS");

                            } else
                            {
                                if (Unipoint_ServiceActivity.bDEBUG)
                                    Log.v(TAG, "JNIClient.ReConnectServer function FAILED with ERROR CODE "+ret);

                            }
                        } catch(Exception e)
                        {
                            e.printStackTrace();
                        }
                        break;
                    }
                    default:
                        break;

                    }

                } else {
                    if (Unipoint_ServiceActivity.bDEBUG)
                        Log.v(TAG, "No Bundle within intent");
                }
            }

        } else {
            if (Unipoint_ServiceActivity.bDEBUG)
                Log.v(TAG, "No intent");
        }

        return super.onStartCommand(intent, flags, startId);
    }

    // This function is called by 3rd party Applications
    public class UnipointSerivceimpl extends IUnipointService.Stub {

        public int getCurrentMode() throws RemoteException {
            // TODO Auto-generated method stub
            if (Unipoint_ServiceActivity.bDEBUG)
				Log.v(TAG, "UnipointSerivceimpl:  Got getCurrentMode request from API layer ");
            int data = JNIClient.getCurrentMode();


            String message = "";
			if(data == Unipoint_ServiceActivity.RET_FAILURE)
			{
				message = "Get Mode failed, return value "+data;
			}else{
			
				switch(data)
				{
				case Unipoint_ServiceActivity.MODE_NONE:
					message = "Unipoint Mode Detected :DISABLED";
					break;
				case Unipoint_ServiceActivity.MODE_NORMAL:
					message = "Unipoint Mode Detected: MODE_NORMAL";
					break;
				case Unipoint_ServiceActivity.MODE_VOLUME:
					message = "Unipoint Mode Detected: MODE_VOLUME";
					break;
				default:
					message = "Unrecognized Mode:  "+data;
					
					break;
				}
				
			}
			
			

            if (Unipoint_ServiceActivity.bDEBUG)
                Log.v(TAG, message);

            // Toast.makeText(self, message, Toast.LENGTH_SHORT).show();
            mcurrentmode = data;

            return mcurrentmode;
        }

        // This function is called by 3rd party Applications

        public void setMode(int mode) throws RemoteException {

            if (Unipoint_ServiceActivity.bDEBUG)
                Log.v(TAG, "Got setMode request");

            JNIClient.SetMode(mode);

        }

    }

}
