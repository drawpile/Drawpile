// SPDX-License-Identifier: GPL-3.0-or-later
package net.drawpile.android;

public class DrawpileNative {
    public static native void processEvents();
    public static native void setPrimaryScreenScale(double scale);
    public static native void savePrimaryScreenScale(boolean askOnStartup);
    public static native void setInterfaceMode(int interfaceMode);
    public static native void onScalingDialogDismissed();
}
