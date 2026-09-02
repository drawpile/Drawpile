// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_PROJECTREPAIRDIALOG_H
#define DESKTOP_DIALOGS_PROJECTREPAIRDIALOG_H
#include "libclient/utils/tempfile.h"
#include <QDialog>

class QLabel;
class QProgressBar;
class QPushButton;
class QStackedWidget;

namespace project {
class ProjectRepair;
}

namespace dialogs {

class ProjectRepairDialog final : public QDialog {
	Q_OBJECT
public:
	explicit ProjectRepairDialog(
		const QString &path, QWidget *parent = nullptr);

Q_SIGNALS:
	void openRequested(const QString &path);

private:
	enum class ProgressStage { Initial, Progressing, Final };

	void handleProgress(qint64 inputSize, qint64 repairedSize);
	void handleFinished(project::ProjectRepair *pr);

	void showSave(bool verified);
	void showError(const QString &message);

	void requestSave();
	bool saveTo(const QString &path, QString &outError);

	QStackedWidget *m_stack;
	QWidget *m_progressPage;
	QLabel *m_progressLabel;
	QProgressBar *m_progressBar;
	QWidget *m_finishedPage;
	QLabel *m_finishedLabel;
	QPushButton *m_finishedSaveButton;
	QPushButton *m_finishedCloseButton;
	QString m_path;
	QString m_ext;
	utils::TempFileHolder m_repairedFile = nullptr;
	ProgressStage m_progressStage = ProgressStage::Initial;
};

}

#endif
