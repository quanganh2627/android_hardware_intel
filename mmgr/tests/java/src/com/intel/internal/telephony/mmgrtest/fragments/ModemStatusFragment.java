package com.intel.internal.telephony.mmgrtest.fragments;

import com.intel.internal.telephony.AsyncOperationResultListener;
import com.intel.internal.telephony.ModemEventListener;
import com.intel.internal.telephony.ModemNotification;
import com.intel.internal.telephony.ModemNotificationArgs;
import com.intel.internal.telephony.ModemStatus;
import com.intel.internal.telephony.ModemStatusManager;
import com.intel.internal.telephony.mmgrtest.R;
import com.intel.internal.telephony.mmgrtest.helpers.MessageBoxHelper;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

public class ModemStatusFragment extends Fragment implements
ModemEventListener, OnClickListener {
    private TextView textViewClientStatus = null;
    private TextView textViewModemStatus = null;
    private TextView textViewLastNotification = null;
    private Button buttonRequestRecovery = null;
    private Button buttonRequestRestart = null;
    private Button buttonRequestShutdown = null;
    private Button buttonRequestLock = null;
    private Button buttonRequestRelease = null;

    private ModemStatusManager modemManager = null;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        try {
            this.modemManager = ModemStatusManager.getInstance();
            this.modemManager.subscribeToEvent(this, ModemStatus.ALL,
                                               ModemNotification.ALL);
        } catch (Exception ex) {
            MessageBoxHelper.showException(this.getActivity(), ex);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        try {
            if (this.modemManager != null) {
                this.modemManager.disconnect();
                this.modemManager = null;
            }
        } catch (Exception ex) {
            MessageBoxHelper.showException(this.getActivity(), ex);
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        View ret = inflater.inflate(R.layout.modem_status, container, false);

        if (ret != null) {
            this.textViewClientStatus = (TextView)ret
                                        .findViewById(R.id.textViewClientStatusValue);
            this.textViewModemStatus = (TextView)ret
                                       .findViewById(R.id.textViewModemStatusValue);
            this.textViewLastNotification = (TextView)ret
                                            .findViewById(R.id.textViewLastNotificationValue);

            this.buttonRequestRecovery = (Button)ret
                                         .findViewById(R.id.buttonRequestRecovery);
            this.buttonRequestRestart = (Button)ret
                                        .findViewById(R.id.buttonRequestRestart);
            this.buttonRequestShutdown = (Button)ret
                                         .findViewById(R.id.buttonRequestShutdown);
            this.buttonRequestLock = (Button)ret
                                     .findViewById(R.id.buttonRequestLock);
            this.buttonRequestRelease = (Button)ret
                                        .findViewById(R.id.buttonRequestRelease);

            if (this.buttonRequestRecovery != null) {
                this.buttonRequestRecovery.setOnClickListener(this);
            }

            if (this.buttonRequestRestart != null) {
                this.buttonRequestRestart.setOnClickListener(this);
            }

            if (this.buttonRequestShutdown != null) {
                this.buttonRequestShutdown.setOnClickListener(this);
            }

            if (this.buttonRequestLock != null) {
                this.buttonRequestLock.setOnClickListener(this);
            }

            if (this.buttonRequestRelease != null) {
                this.buttonRequestRelease.setOnClickListener(this);
            }
        }

        if (this.textViewClientStatus != null) {
            this.textViewClientStatus.setText("CONNECTING...");
        }

        if (this.modemManager != null) {
            this.modemManager.connectAsync("MMGR Test", new AsyncOperationResultListener() {
                                               @Override
                                               public void onOperationError(Exception ex) {
                                                   ModemStatusFragment.this.setUIDisconnectedMode();
                                               }

                                               @Override
                                               public void onOperationComplete() {
                                                   ModemStatusFragment.this.setUIConnectedMode();
                                               }
                                           });
        } else {
            ModemStatusFragment.this.setUIDisconnectedMode();
        }
        return ret;
    }

    @Override
    public void onModemColdReset(ModemNotificationArgs arg) {
        if (this.textViewLastNotification != null) {
            this.textViewLastNotification.setText("MODEM COLD RESET");
        }
    }

    @Override
    public void onModemCoreDump(ModemNotificationArgs arg) {
        if (this.textViewLastNotification != null) {
            this.textViewLastNotification.setText("MODEM CORE DUMP");
        }
    }

    @Override
    public void onPlatformReboot(ModemNotificationArgs arg) {
        if (this.textViewLastNotification != null) {
            this.textViewLastNotification.setText("PLATFORM REBOOT");
        }
    }

    @Override
    public void onModemShutdown(ModemNotificationArgs arg) {
        if (this.textViewLastNotification != null) {
            this.textViewLastNotification.setText("MODEM SHUTDOWN");
        }
    }

    @Override
    public void onModemDead() {
        if (this.textViewModemStatus != null) {
            this.textViewModemStatus.setText("MODEM DEAD");
        }
    }

    @Override
    public void onModemDown() {
        if (this.textViewModemStatus != null) {
            this.textViewModemStatus.setText("MODEM DOWN");
        }
    }

    @Override
    public void onModemUp() {
        if (this.textViewModemStatus != null) {
            this.textViewModemStatus.setText("MODEM UP");
        }
    }

    @Override
    public void onClick(View view) {
        if (view != null) {
            try {
                switch (view.getId()) {
                case R.id.buttonRequestRecovery:
                    this.doRecoverModem();
                    break;
                case R.id.buttonRequestRestart:
                    this.doRestartModem();
                    break;
                case R.id.buttonRequestShutdown:
                    this.doShutdownModem();
                    break;
                case R.id.buttonRequestLock:
                    this.doLockModem();
                    break;
                case R.id.buttonRequestRelease:
                    this.doReleaseModem();
                    break;
                }
            } catch (Exception ex) {
                MessageBoxHelper.showException(this.getActivity(), ex);
            }
        }
    }

    private void doRecoverModem() {
        if (this.modemManager != null) {
            String[] causes = new String[2];
            causes[0] = "Requested by mmgr-test Java application";
            causes[1] = "doRecoverModem";

            this.modemManager.recoverModemAsync(new AsyncOperationResultListener() {
                                                    @Override
                                                    public void onOperationError(Exception ex) {
                                                        MessageBoxHelper.showException(
                                                            ModemStatusFragment.this.getActivity(), ex);
                                                    }

                                                    @Override
                                                    public void onOperationComplete() {
                                                        Toast.makeText(ModemStatusFragment.this.getActivity(),
                                                                       "Recover request sent",
                                                                       Toast.LENGTH_SHORT).show();
                                                    }
                                                }, causes);
        }
    }

    private void doRestartModem() {
        if (this.modemManager != null) {
            this.modemManager.restartModemAsync(new AsyncOperationResultListener() {
                                                    @Override
                                                    public void onOperationError(Exception ex) {
                                                        MessageBoxHelper.showException(
                                                            ModemStatusFragment.this.getActivity(), ex);
                                                    }

                                                    @Override
                                                    public void onOperationComplete() {
                                                        Toast.makeText(ModemStatusFragment.this.getActivity(),
                                                                       "Restart request sent",
                                                                       Toast.LENGTH_SHORT).show();
                                                    }
                                                });
        }
    }

    private void doShutdownModem() {
        if (this.modemManager != null) {
            this.modemManager.shutdownModemAsync(new AsyncOperationResultListener() {
                                                     @Override
                                                     public void onOperationError(Exception ex) {
                                                         MessageBoxHelper.showException(
                                                             ModemStatusFragment.this.getActivity(), ex);
                                                     }

                                                     @Override
                                                     public void onOperationComplete() {
                                                         Toast.makeText(ModemStatusFragment.this.getActivity(),
                                                                        "Shutdown request sent",
                                                                        Toast.LENGTH_SHORT).show();
                                                     }
                                                 });
        }
    }

    private void doLockModem() {
        if (this.modemManager != null) {
            this.modemManager.acquireModemAsync(new AsyncOperationResultListener() {
                                                    @Override
                                                    public void onOperationError(Exception ex) {
                                                        MessageBoxHelper.showException(
                                                            ModemStatusFragment.this.getActivity(), ex);
                                                    }

                                                    @Override
                                                    public void onOperationComplete() {
                                                        Toast.makeText(ModemStatusFragment.this.getActivity(),
                                                                       "Acquire request sent",
                                                                       Toast.LENGTH_SHORT).show();
                                                    }
                                                });
        }
    }

    private void doReleaseModem() {
        if (this.modemManager != null) {
            this.modemManager.releaseModemAsync(new AsyncOperationResultListener() {
                                                    @Override
                                                    public void onOperationError(Exception ex) {
                                                        MessageBoxHelper.showException(
                                                            ModemStatusFragment.this.getActivity(), ex);
                                                    }

                                                    @Override
                                                    public void onOperationComplete() {
                                                        Toast.makeText(ModemStatusFragment.this.getActivity(),
                                                                       "Release request sent",
                                                                       Toast.LENGTH_SHORT).show();
                                                    }
                                                });
        }
    }

    private void setUIConnectedMode() {
        if (this.textViewClientStatus != null) {
            this.textViewClientStatus.setText("CONNECTED");
        }
        if (this.buttonRequestLock != null) {
            this.buttonRequestLock.setEnabled(true);
        }
        if (this.buttonRequestRecovery != null) {
            this.buttonRequestRecovery.setEnabled(true);
        }
        if (this.buttonRequestRelease != null) {
            this.buttonRequestRelease.setEnabled(true);
        }
        if (this.buttonRequestRestart != null) {
            this.buttonRequestRestart.setEnabled(true);
        }
        if (this.buttonRequestShutdown != null) {
            this.buttonRequestShutdown.setEnabled(true);
        }
    }

    private void setUIDisconnectedMode() {
        if (this.textViewClientStatus != null) {
            this.textViewClientStatus.setText("DISCONNECTED");
        }
        if (this.buttonRequestLock != null) {
            this.buttonRequestLock.setEnabled(false);
        }
        if (this.buttonRequestRecovery != null) {
            this.buttonRequestRecovery.setEnabled(false);
        }
        if (this.buttonRequestRelease != null) {
            this.buttonRequestRelease.setEnabled(false);
        }
        if (this.buttonRequestRestart != null) {
            this.buttonRequestRestart.setEnabled(false);
        }
        if (this.buttonRequestShutdown != null) {
            this.buttonRequestShutdown.setEnabled(false);
        }
    }
}
