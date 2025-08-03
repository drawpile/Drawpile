// SPDX-License-Identifier: GPL-3.0-or-later
package net.drawpile.android;

import android.app.ForegroundServiceStartNotAllowedException;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;
import android.os.Handler;
import android.os.Looper;

import net.drawpile.cfg.DrawpileResources;

// TODO: This service doesn't actually work like this. The idea was that we prod
// Qt every few seconds and somehow make it process socket stuff, but it seems
// like that doesn't work. We need to rearchitect things it seems, moving the
// message queue and socket handling to this service so that it can keep running
// while the application is in the background (and yes, foreground services run
// in the background, it's some brilliant naming there.)
public class ConnectService extends Service {

    private static final String TAG = "ConnectService";
    private static final int NOTIFICATION_ID = 100;

    private Thread thread = null;
    private volatile boolean running = false;

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "onCreate");
    }

    private Notification getNotification() {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
            return new Notification.Builder(this, MainActivity.CONNECTION_NOTIFICATION_CHANNEL_ID)
                    .setContentTitle("Connected")
                    .setSmallIcon(DrawpileResources.getNotificationIcon())
                    .build();
        } else {
            return new Notification.Builder(this)
                    .setContentTitle("Connected")
                    .setSmallIcon(DrawpileResources.getNotificationIcon())
                    .setPriority(Notification.PRIORITY_LOW)
                    .build();
        }
    }

    private void tryStartServiceInForegroundS() {
        try {
            tryStartServiceInForegroundQ();
        } catch (ForegroundServiceStartNotAllowedException e) {
            Log.w(TAG, "Could not run the service in foreground: " + e);
        }
    }

    private void tryStartServiceInForegroundQ() {
        Log.i(TAG, "Starting foreground short service");
        startForeground(NOTIFICATION_ID, getNotification(), ServiceInfo.FOREGROUND_SERVICE_TYPE_SHORT_SERVICE);
    }

    private void tryStartServiceInForeground() {
        Log.i(TAG, "Starting foreground service");
        startForeground(NOTIFICATION_ID, getNotification());
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent.getBooleanExtra("start", false)) {
            Log.i(TAG, "onStartCommand: start");
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                tryStartServiceInForegroundS();
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                tryStartServiceInForegroundQ();
            } else {
                tryStartServiceInForeground();
            }

            running = true;
            thread = new Thread(() -> {
                while (running) {
                    try {
                        Handler mainHandler = new Handler(Looper.getMainLooper());
                        mainHandler.post(() -> {
                            try {
                                DrawpileNative.processEvents();
                            } catch(Exception|UnsatisfiedLinkError e) {
                                Log.e(TAG, "Error pumping main thread", e);
                            }
                        });
                    } catch (Exception e) {
                        Log.e(TAG, "Error plonking on main", e);
                    }

                    try {
                        Thread.sleep(5000);
                    } catch (InterruptedException e) {
                        // Nothing.
                    }
                }

                Log.i(TAG, "Stop foreground");
                try {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                        stopForeground(STOP_FOREGROUND_REMOVE);
                    } else {
                        stopForeground(true);
                    }
                } catch (Exception e) {
                    Log.e(TAG, "stopForeground error", e);
                }
            });
            thread.start();

        } else if (intent.getBooleanExtra("stop", false)) {
            Log.i(TAG, "onStartCommand: stop");
            running = false;
            while (thread != null) {
                Log.i(TAG, "Join thread");
                try {
                    thread.interrupt();
                    thread.join();
                    thread = null;
                } catch (InterruptedException e) {
                    Log.w(TAG, "Join interrupted", e);
                }
            }
            Log.i(TAG, "Stopping self");
            stopSelf();

        } else {
            Log.w(TAG, "onStartCommand: unknown");
        }
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
