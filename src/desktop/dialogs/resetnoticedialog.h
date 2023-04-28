// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RESETNOTICEDIALOG_H
#define RESETNOTICEDIALOG_H

#include "libclient/drawdance/canvasstate.h"
#include <QDialog>

class QAbstractButton;
class QLabel;
class QProgressBar;
class QPushButton;

namespace dialogs {

class ResetNoticeDialog final : public QDialog {
	Q_OBJECT
public:
	ResetNoticeDialog(
		const drawdance::CanvasState &canvasState, bool compatibilityMode,
		QWidget *parent = nullptr);

	void setSaveInProgress(bool saveInProgress);

signals:
    void saveRequested(const drawdance::CanvasState &canvasState);

public slots:
	void catchupProgress(int percent);

private slots:
    void buttonClicked(QAbstractButton *button);

private:
	drawdance::CanvasState m_canvasState;
	QLabel *m_messageLabel;
	QProgressBar *m_progressBar;
    QPushButton *m_saveButton;
    QPushButton *m_closeButton;
};

}

#endif
