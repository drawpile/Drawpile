// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_PROJECTEDITDIALOG_H
#define DESKTOP_DIALOGS_PROJECTEDITDIALOG_H
#include <QDialog>
#include <QSharedPointer>
#include <QVector>
#include <libclient/utils/tempfile.h>

class QLabel;
class QProgressBar;
class QListWidget;
class QPushButton;
class QStackedWidget;

namespace widgets {
class GroupedToolButton;
}

namespace dialogs {

class ProjectEditDialog final : public QDialog {
	Q_OBJECT
public:
	explicit ProjectEditDialog(QWidget *parent = nullptr);

	void addInputFiles();

	void accept() override;
	void reject() override;

Q_SIGNALS:
	void cancelRequested();
	void openOutputFileRequested(const QString &path);

protected:
	void closeEvent(QCloseEvent *event) override;

private:
	enum Roles {
		PathRole = Qt::UserRole,
	};

	void updateButtons();

	void handleInputPath(const QString &path, QVector<int> &outIndexesToSelect);

	void removeSelected();
	void moveUpSelected();
	void moveDownSelected();

	void setSelectedIndexes(const QVector<int> &indexesToSelect);

	void onConversionSucceeded();
	void onConversionCancelled();
	void onConversionFailed(const QString &message, const QString &detail);
	bool copyTemporaryToOutputFile(QString &outErrorMessage);
	void finishConversion(bool success);

	void showInputPage();
	void showProgressPage();
	void showFinishedPage();
	void openOutputFile();

	QStackedWidget *m_stack;
	QWidget *m_inputPage;
	QListWidget *m_inputList;
	widgets::GroupedToolButton *m_addButton;
	widgets::GroupedToolButton *m_removeButton;
	widgets::GroupedToolButton *m_moveUpButton;
	widgets::GroupedToolButton *m_moveDownButton;
	QPushButton *m_saveButton;
	QWidget *m_progressPage;
	QLabel *m_progressLabel;
	QProgressBar *m_progressBar;
	QPushButton *m_progressCancelButton;
	QWidget *m_finishedPage;
	QLabel *m_finishedLabel;
	QPushButton *m_finishedOpenButton;
	QSharedPointer<utils::TempFile> m_tempFile;
	QString m_outputPath;
};

}

#endif
