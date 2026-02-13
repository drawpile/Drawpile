// SPDX-License-Identifier: GPL-3.0-or-later
package net.drawpile.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.SeekBar;
import android.widget.TextView;

import net.drawpile.R;

public final class ScalingDialog {

    private static final String TAG = "drawpile.ScalingDialog";
    private static int SCALING_STEP_SIZE_PERCENT = 5;

    // Keep these in sync with the enum InterfaceMode in libclient/view/enums.h!
    private static final int INTERFACE_MODE_DYNAMIC = 0;
    private static final int INTERFACE_MODE_DESKTOP = 1;
    private static final int INTERFACE_MODE_SMALL_SCREEN = 2;

    private final String m_percentTextFormat;
    private final String m_percentDefaultTextFormat;
    private final double m_maximumScale;
    private final int m_defaultProgress;
    private final View m_view;
    private final AlertDialog m_alertDialog;

    public ScalingDialog(
            Activity activity, String percentTextFormat, String percentDefaultTextFormat,
            String dynamicText, String desktopText, String mobileText, String rememberCheckBoxText,
            String okButtonText, double currentScale, double defaultScale, int interfaceMode,
            boolean showOnStartup, boolean canShowOnStartup) {

        m_percentTextFormat = percentTextFormat;
        m_percentDefaultTextFormat = percentDefaultTextFormat;
        m_maximumScale = Math.max(4.0, Math.ceil(defaultScale * 2.0));
        m_defaultProgress = scaleToProgress(defaultScale);
        m_view = activity.getLayoutInflater().inflate(R.layout.scaling_dialog_layout, null);

        m_alertDialog = new AlertDialog.Builder(activity)
                .setView(m_view)
                .setCancelable(false)
                .create();

        SeekBar seekBar = m_view.findViewById(R.id.seekBar);
        seekBar.setMax((int) Math.round((m_maximumScale - 1.0) * 100.0 / SCALING_STEP_SIZE_PERCENT));
        seekBar.setProgress(scaleToProgress(currentScale));
        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                updatePercentText(progress);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
                // Nothing.
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                int progress = seekBar.getProgress();
                double scale;
                if (progress == m_defaultProgress) {
                    scale = defaultScale;
                } else {
                    scale = progressToScale(progress);
                }
                DrawpileNative.setPrimaryScreenScale(scale);
            }
        });

        int idToCheck;
        if (interfaceMode == INTERFACE_MODE_DESKTOP) {
            idToCheck = R.id.desktopRadio;
        } else if (interfaceMode == INTERFACE_MODE_SMALL_SCREEN) {
            idToCheck = R.id.mobileRadio;
        } else {
            idToCheck = R.id.dynamicRadio;
        }
        RadioGroup interfaceModeGroup = m_view.findViewById(R.id.interfaceModeGroup);
        interfaceModeGroup.check(idToCheck);
        interfaceModeGroup.setOnCheckedChangeListener((radioGroup, checkedId) -> {
            int interfaceModeToSet;
            if (checkedId == R.id.desktopRadio) {
                interfaceModeToSet = INTERFACE_MODE_DESKTOP;
            } else if (checkedId == R.id.mobileRadio) {
                interfaceModeToSet = INTERFACE_MODE_SMALL_SCREEN;
            } else {
                interfaceModeToSet = INTERFACE_MODE_DYNAMIC;
            }
            DrawpileNative.setInterfaceMode(interfaceModeToSet);
        });

        RadioButton dynamicRadio = m_view.findViewById(R.id.dynamicRadio);
        dynamicRadio.setText(dynamicText);

        RadioButton desktopRadio = m_view.findViewById(R.id.desktopRadio);
        desktopRadio.setText(desktopText);

        RadioButton mobileRadio = m_view.findViewById(R.id.mobileRadio);
        mobileRadio.setText(mobileText);

        CheckBox rememberCheckBox = m_view.findViewById(R.id.rememberCheckBox);
        rememberCheckBox.setText(rememberCheckBoxText);
        rememberCheckBox.setChecked(showOnStartup);
        if (!canShowOnStartup) {
            rememberCheckBox.setVisibility(View.GONE);
        }

        Button okButton = m_view.findViewById(R.id.okButton);
        okButton.setText(okButtonText);

        m_view.findViewById(R.id.okButton).setOnClickListener(v -> {
            m_alertDialog.dismiss();
            DrawpileNative.savePrimaryScreenScale(rememberCheckBox.isChecked());
        });
        updatePercentText(seekBar.getProgress());

        m_alertDialog.setOnDismissListener(dialogInterface -> DrawpileNative.onScalingDialogDismissed());
    }

    public void show() {
        m_alertDialog.show();
        Window window = m_alertDialog.getWindow();
        if (window != null) {
            window.clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        }
    }

    private void updatePercentText(int progress) {
        String format;
        if (progress == m_defaultProgress) {
            format = m_percentDefaultTextFormat;
        } else {
            format = m_percentTextFormat;
        }
        TextView percentText = m_view.findViewById(R.id.percentText);
        percentText.setText(format.replace("%1", progressToPercent(progress) + "%"));
    }

    private static int scaleToProgress(double scale) {
        return (int) Math.round((scale - 1.0) * 100.0 / SCALING_STEP_SIZE_PERCENT);
    }

    private static int progressToPercent(int progress) {
        return 100 + (progress * SCALING_STEP_SIZE_PERCENT);
    }

    private static double progressToScale(int progress) {
        return ((double) progressToPercent(progress)) / 100.0;
    }
}
