// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_PROJECTRECORDINGSETTINGSDIALOG_H
#define DESKTOP_DIALOGS_PROJECTRECORDINGSETTINGSDIALOG_H
#include <QDialog>

class KisDoubleSliderSpinBox;

namespace dialogs {

class ProjectRecordingSettingsDialog : public QDialog {
	Q_OBJECT
public:
	ProjectRecordingSettingsDialog(
		size_t lastReportedSizeInBytes, size_t sizeLimitInBytes,
		QWidget *parent = nullptr);

	void accept() override;

	// This is also used by the Files settings.
	static void
	initSizeLimitSlider(KisDoubleSliderSpinBox *slider, double minimumInGiB);

	// This is also used by the Files settings.
	static QString getAutorecordNoteText();

Q_SIGNALS:
	void setSizeLimitInBytesRequested(size_t sizeLimitInBytes);
	void stopRequested();

private:
	KisDoubleSliderSpinBox *m_sizeLimitSlider;
};

}

#endif
