package intel.aidltest;

import intel.aidltest.aidlsrc.IUnipointService;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

public class Unipoint_ServiceActivity extends Activity {

	public static final String SERVICEACTION = "intel.aidltest.service.UnipointService";

	private static final String TAG = "Unipoint_ServiceActivity";

	public static final String COMMAND = "COMMAND";

	public static final int CMD_NOTIFY = 1;
	public static final int CMD_RECONNECT = 2;
	
	
	public static final String PAR_NEWMODE = "PAR_NEWMODE";

	public static final boolean bDEBUG = true;


	public static final int MODE_NONE = 0;
	public static final int MODE_NORMAL = 1;
	public static final int MODE_VOLUME = 2;
	public static final int RET_FAILURE = -1;
	
	
	
	

	private TextView mInformationText;
	private Button mShowmodeButton;
	private Button mchangeToNormalmode;
	private Button changemodevolume;

	private Unipoint_ServiceActivity self;

	private int mCurrentmode = -1;
	private Button mreconnecttodaemon;

	private Button munipointonoffswitch;

	private boolean bUnipointEnabled = false;

	private Handler myHandler;
	
	private IUnipointService iunipointservice = null;

	private ServiceConnection serviceConnection = new ServiceConnection()
	{


		public void onServiceConnected(ComponentName name, IBinder service) {
			
			Log.v(TAG,"Service Connected");
			iunipointservice = IUnipointService.Stub.asInterface(service);
			
			
			checkCurrentModeAndServiceStatus();
		}


		public void onServiceDisconnected(ComponentName name) {
			Log.v(TAG,"Service DISConnected");
			
			checkCurrentModeAndServiceStatus();
		}
		
	};
	
	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		
		
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main);

		self = this;
		
        Intent mintent = new Intent(SERVICEACTION);
        this.bindService(mintent,serviceConnection , BIND_AUTO_CREATE);
        
        
		init_screen();

		//
	}

	
	
	/*
	 * Prepare screen . 
	 */
	private void init_screen() {
		

		mInformationText = (TextView) this.findViewById(R.id.informationtext);
		mShowmodeButton = (Button) this.findViewById(R.id.getmodebutton);
		changemodevolume = (Button) this.findViewById(R.id.changemodebutton1);
		munipointonoffswitch = (Button)this.findViewById(R.id.unipointonoffswitch);
		
		mchangeToNormalmode = (Button) this.findViewById(R.id.changemodebutton);
		mreconnecttodaemon = (Button) this.findViewById(R.id.reconnecttodaemon);
	
		
		/*
		 * Initialize button for unipoint on/off switch
		 */
		munipointonoffswitch.setOnClickListener(new OnClickListener() {

			public void onClick(View v) {

				int ret = RET_FAILURE;

	
					if(!bUnipointEnabled){
						
						if (Unipoint_ServiceActivity.bDEBUG)
							Log.v(TAG, "TRYING to switch to MODE_NORMAL, enable unipoint ");
						
						
						try {
							iunipointservice.setMode(MODE_NORMAL);
						} catch (RemoteException e) {
							// TODO Auto-generated catch block
							e.printStackTrace();
						}
						
						
						try {
							
							
							ret = iunipointservice.getCurrentMode();
							
							
							
						} catch (RemoteException e) {
							// TODO Auto-generated catch block
							e.printStackTrace();
						}
						
					}else
					{
						
						if (Unipoint_ServiceActivity.bDEBUG)
							Log.v(TAG, "TRYING to switch to MODE_NONE, disable unipoint ");
						
						try {
							iunipointservice.setMode(MODE_NONE);
						} catch (RemoteException e) {
							// TODO Auto-generated catch block
							e.printStackTrace();
						}
						
					}
					//TODO , check returning code 
					
					
					checkCurrentModeAndServiceStatus();
					

			}
		});

		
		
		
		/*
		 * Initialize button for Restart Unipoint-Java Service Button. (Not Restart Daemon)
		 */
		mreconnecttodaemon.setOnClickListener(new OnClickListener() {

			public void onClick(View arg0) {

				Intent b = new Intent(SERVICEACTION);
			//	self.stopService(b);

				try {
					Thread.sleep(1000);
					
					
					Bundle bundle = new Bundle();

					bundle.putInt(Unipoint_ServiceActivity.COMMAND,
							Unipoint_ServiceActivity.CMD_RECONNECT);

					b.putExtras(bundle);
					
					self.startService(b);

					checkCurrentModeAndServiceStatus();
				    //self.bindService(b,serviceConnection , BIND_AUTO_CREATE);
					   
				} catch (InterruptedException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}

			}
		});

		mreconnecttodaemon.setVisibility(View.GONE);
		
		
		/*
		 * Initialize button for show current unipoint mode 
		 */
		mShowmodeButton.setOnClickListener(new OnClickListener() {

			public void onClick(View v) {
				
				
				//Get the current mode 
				int data = checkCurrentModeAndServiceStatus();
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

				if (Unipoint_ServiceActivity.bDEBUG)
					Log.v(TAG, message);

				Toast.makeText(self, message, Toast.LENGTH_SHORT).show();
				checkCurrentModeAndServiceStatus();
			}
		});
		
		
		
		

		mchangeToNormalmode.setOnClickListener(new OnClickListener() {

			public void onClick(View v) {

				if (Unipoint_ServiceActivity.bDEBUG)
					Log.v(TAG, "TRYING to switch to MODE_NORMAL");
				int data = RET_FAILURE;
				try {
					iunipointservice.setMode(MODE_NORMAL);
				} catch (RemoteException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}

				
				try {
					data = iunipointservice.getCurrentMode();
				} catch (RemoteException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}

				if (Unipoint_ServiceActivity.bDEBUG)
					Log.v(TAG, "Got unipoit Mode :" + data);

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

				if (Unipoint_ServiceActivity.bDEBUG)
					Log.v(TAG, message);

				Toast.makeText(self, message, Toast.LENGTH_SHORT).show();
				checkCurrentModeAndServiceStatus();
			}
		});

		
		
		
		/*
		 * Change mode to Volume mode 
		 */
		changemodevolume.setOnClickListener(new OnClickListener() {

			public void onClick(View v) {

				if (Unipoint_ServiceActivity.bDEBUG)
					Log.v(TAG, "TRYING to switch to MODE_VOLUME");
				
				int data = RET_FAILURE;
				try {
					iunipointservice.setMode(MODE_VOLUME);
				} catch (RemoteException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
				
				 try {
					data = iunipointservice.getCurrentMode();
				} catch (RemoteException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}

				if (Unipoint_ServiceActivity.bDEBUG)
					Log.v(TAG, "Got unipoit Mode :" + data);
				
				
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

				if (Unipoint_ServiceActivity.bDEBUG)
					Log.v(TAG, message);

				Toast.makeText(self, message, Toast.LENGTH_SHORT).show();
				checkCurrentModeAndServiceStatus();
			}
		});
	}

	



	//This function is used to get current mode , update the button contents based on returning result.
	private int checkCurrentModeAndServiceStatus() {
	
		try{

				
		int	data = iunipointservice.getCurrentMode();

			
		mCurrentmode = data;
		
		if(mCurrentmode == -1)
		{
			mInformationText.setText("Status: Service Not Connected");
			munipointonoffswitch.setEnabled(false);
			
			//Service not connected 
			
		}else if(mCurrentmode == 0) // it is None Mode 
		{
			munipointonoffswitch.setEnabled(true);
			munipointonoffswitch.setText(this.getString(R.string.enableunipoint));
			mInformationText.setText("Status: Service Connected, Unipoint Disabled ");
			bUnipointEnabled = false;
			
			changemodevolume.setEnabled(false);
			mchangeToNormalmode.setEnabled(false);
		}else if(mCurrentmode >=0)
		{
			
			changemodevolume.setEnabled(true);
			mchangeToNormalmode.setEnabled(true);
			
			
			munipointonoffswitch.setEnabled(true);
			munipointonoffswitch.setText(this.getString(R.string.disableunipoint));
			mInformationText.setText("Status: Service Connected, Unipoint Enabled");
			bUnipointEnabled = true;
		}
		
		
		return data;
	
	}catch(Exception e)
	{
		e.printStackTrace();
	}
		return -1;
	}

}