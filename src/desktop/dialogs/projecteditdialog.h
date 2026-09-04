// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_PROJECTEDITDIALOG_H
#define DESKTOP_DIALOGS_PROJECTEDITDIALOG_H
#include "libclient/io/tempfile.h"
#include <QDialog>
#include <QImage>
#include <QSharedPointer>
#include <QString>
#include <QVector>

class AsyncTaskRunnable;
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
	struct Entry {
		QString path;
		QString sourceParam;
		QImage thumbnail;
		long long sessionId;
		int sourceType;
		bool project;

		QString id() const;
		QString text() const;
		QString toolTip() const;
	};

	explicit ProjectEditDialog(QWidget *parent = nullptr);

	void promptForInputFiles();
	void addInputPaths(const QStringList &paths);

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
		IdRole,
	};

	void updateButtons();

	void handleLoadFinished(AsyncTaskRunnable *runnable, int taskCount);
	void handleInputEntry(const Entry &entry, QVector<int> &outIndexesToSelect);

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
	QSharedPointer<io::TempFile> m_tempFile;
	QString m_outputPath;
};

}

#endif
