// SPDX-License-Identifier: GPL-3.0-or-later
package net.drawpile.android;

import android.content.Intent;
import android.os.Bundle;
import android.os.Build;
import android.util.Log;
import android.view.ViewConfiguration;

import android.app.NotificationChannel;
import android.app.NotificationManager;

import org.qtproject.qt5.android.QtNative;
import org.qtproject.qt5.android.bindings.QtActivity;
import org.qtproject.qt5.android.bindings.QtApplication;

public class MainActivity extends QtActivity {

    private static final String TAG = "drawpile.MainActivity";

    public static final String CONNECTION_NOTIFICATION_CHANNEL_ID = "net.drawpile.connection";
    public static NotificationChannel connectionNotificationChannel = null;
    private static boolean started = false;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.QT_ANDROID_DEFAULT_THEME = "DefaultTheme";
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onStart() {
        super.onStart();
        // This is called multiple times in the application's lifecycle, but we
        // only want to full-screen it once at the beginning. Doing it here
        // avoids some flickering with the main window that happens otherwise.
        if (!started) {
            started = true;
            try {
                setFullScreen(true);
            } catch (Exception | UnsatisfiedLinkError e) {
                Log.e(TAG, "Failed to enter fullscreen", e);
            }
        }
    }

    public boolean createConnectionNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && connectionNotificationChannel == null) {
            Log.i(TAG, "createConnectionNotificationChannel");
            connectionNotificationChannel = createNotificationChannel(CONNECTION_NOTIFICATION_CHANNEL_ID, "Connection", "Shown while you're connected to a session.", NotificationManager.IMPORTANCE_LOW);
        }
        return connectionNotificationChannel != null;
    }

    private NotificationChannel createNotificationChannel(String id, CharSequence name, String description, int importance) {
        try {
            NotificationChannel channel = new NotificationChannel(id, name, importance);
            channel.setDescription(description);

            NotificationManager notificationManager = getSystemService(NotificationManager.class);
            notificationManager.createNotificationChannel(channel);

            return channel;
        } catch (Exception e) {
            Log.e(TAG, "Failed to create notification channel " + id, e);
            return null;
        }
    }

    public void startConnectService() {
        Log.i(TAG, "Start connect service");
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            if (!createConnectionNotificationChannel()) {
                Log.w(TAG, "Creating connect notification channel failed");
                return;
            }
        }
        startConnectServiceCommand("start");
    }

    public void stopConnectService() {
        Log.i(TAG, "Stop connect service");
        startConnectServiceCommand("stop");
    }

    private void startConnectServiceCommand(String command) {
        Intent intent = new Intent(this, ConnectService.class);
        intent.putExtra(command, true);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            tryStartForegroundService(intent);
        } else {
            tryStartService(intent);
        }
    }

    private void tryStartForegroundService(Intent intent) {
        try {
            startForegroundService(intent);
        } catch (Exception e) {
            Log.w(TAG, "Failed to start foreground service", e);
            tryStartService(intent);
        }
    }

    private void tryStartService(Intent intent) {
        try {
            startService(intent);
        } catch (Exception e) {
            Log.w(TAG, "Failed to start service", e);
        }
    }

    public static int getLongPressTimeout() {
        try {
            return ViewConfiguration.get(QtNative.activity()).getLongPressTimeout();
        } catch (Exception|UnsatisfiedLinkError e) {
            Log.e(TAG, "Exception getting long press timeout", e);
            return 500;
        }
    }

    public static boolean looksLikeXiaomiDevice() {
        return containsXiaomi(Build.BRAND) || containsXiaomi(Build.MANUFACTURER);
    }

    private static boolean containsXiaomi(String s) {
        return s != null && s.toLowerCase().contains("xiaomi");
    }
}
