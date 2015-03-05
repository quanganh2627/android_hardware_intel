package com.intel.unipoint_api_example;




import intel.aidltest.aidlsrc.IUnipointService;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.TextView;


public class Unipoint_API_Example_APP extends Activity {
	
	public static final int MODE_NONE = 0;
	public static final int MODE_NORMAL = 1;
	public static final int MODE_VOLUME = 2;
	public static final int RET_FAILURE = -1;
	
	
	
    private TextView mInformationText;
	private Button mShowmodeButton;

	private IUnipointService iunipointservice = null;
	public static final String SERVICEACTION = "intel.aidltest.service.UnipointService";
	protected static final String TAG = "AIDLTest_Client";
	private ServiceConnection serviceConnection = new ServiceConnection()
	{


		public void onServiceConnected(ComponentName name, IBinder service) {
			
			Log.v(TAG,"Service Connected");
			iunipointservice = IUnipointService.Stub.asInterface(service);
			
			
		}


		public void onServiceDisconnected(ComponentName name) {
			Log.v(TAG,"Service DISConnected");
			
		}
		
	};
	private Button mchangemodebutton;
	
	/** Called when the activity is first created. */

    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        
        Intent mintent = new Intent(SERVICEACTION);
        this.bindService(mintent,serviceConnection , BIND_AUTO_CREATE);
        
        
        init_screen();
    }

	private void init_screen() {
		mInformationText = (TextView)this.findViewById(R.id.informationtext);
		mShowmodeButton = (Button)this.findViewById(R.id.getmodebutton);
		mchangemodebutton = (Button)this.findViewById(R.id.changemodebutton);
		mShowmodeButton.setOnClickListener(new OnClickListener(){


			public void onClick(View v) {
				
				try {
					int data = iunipointservice.getCurrentMode();
					String message = "";
					
					
					if(data == RET_FAILURE)
					{
						message = "Get Mode failed, return value "+data;
					}else{
					
						switch(data)
						{
						case MODE_NONE:
							message = "Unipoint Mode Detected :DISABLED";
							break;
						case MODE_NORMAL:
							message = "Unipoint Mode Detected: MODE_NORMAL";
							break;
						case MODE_VOLUME:
							message = "Unipoint Mode Detected: MODE_VOLUME";
							break;
						default:
							message = "Unrecognized Mode:  "+data;
							
							break;
						}
						
					}
				
					mInformationText.setText(""+message);
					
					
				} catch (RemoteException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
				
				
			}});
		
		
		
		
		//Change current mode to next mode 
		mchangemodebutton.setOnClickListener(new OnClickListener(){


			public void onClick(View v) {
				try {
					int mode = iunipointservice.getCurrentMode();
					if(mode == MODE_NORMAL)
					{
						mode=MODE_VOLUME;
						iunipointservice.setMode(mode);
						
					}else if(mode== MODE_VOLUME)
					{
						mode =MODE_NORMAL;
						iunipointservice.setMode(mode);
					}
						
					
					
				} catch (RemoteException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
				
				
			}});
	}
	
	
	
}