// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_PROJECTRECORDINGSETTINGSDIALOG_H
#define DESKTOP_DIALOGS_PROJECTRECORDINGSETTINGSDIALOG_H
#include <QDialog>
#include <QPointer>

class QAction;
class QCheckBox;
class QLabel;
class QStackedWidget;
class KisDoubleSliderSpinBox;

namespace dialogs {

class ProjectRecordingSettingsDialog : public QDialog {
	Q_OBJECT
public:
	ProjectRecordingSettingsDialog(
		QAction *autoRecordAction, bool settingsOpen,
		QWidget *parent = nullptr);

	void updateSize(size_t lastReportedSizeInBytes, size_t sizeLimitInBytes);

	// This is also used by the Files settings.
	static void
	initSizeLimitSlider(KisDoubleSliderSpinBox *slider, double minimumInGiB);

	// This is also used by the Files settings.
	static QString getAutorecordNoteText();

Q_SIGNALS:
	void preferencesRequested();
	void setSizeLimitInBytesRequested(size_t sizeLimitInBytes);

private:
	static double bytesInGiB(size_t bytes)
	{
		return double(bytes) / 1024.0 / 1024.0 / 1024.0;
	}

	double lastReportedSizeInGiB() const
	{
		return bytesInGiB(m_lastReportedSizeInBytes);
	}

	double sizeLimitInGiB() const { return bytesInGiB(m_sizeLimitInBytes); }

	void updateSizeLimitLabelText();
	void updateFromAutoRecordAction();

	void showOrRaiseSizeLimitChangeDialog();

	QAction *m_autoRecordAction;
	QCheckBox *m_enableCheckBox;
	QStackedWidget *m_stack;
	QWidget *m_disabledPage;
	QWidget *m_enabledPage;
	QLabel *m_sizeLimitLabel;
	QPointer<QDialog> m_sizeLimitChangeDialog;
	size_t m_lastReportedSizeInBytes = 0;
	size_t m_sizeLimitInBytes = 0;
};

}

#endif
