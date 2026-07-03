// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_PROJECTDIALOG_H
#define DESKTOP_DIALOGS_PROJECTDIALOG_H
#include <QDialog>

class QDialogButtonBox;
class QLabel;
class QStackedWidget;

namespace project {
class ProjectWrangler;
}

namespace dialogs {

class ProjectDialog final : public QDialog {
	Q_OBJECT
public:
	explicit ProjectDialog(QWidget *parent = nullptr);
	~ProjectDialog() override;

	void setTempPath(const QString &tempPath);

	static void showUnhandledProjectErrorMessageBoxOn(
		QWidget *parent, const QString &errorMessage);

private:
	void openProject();
	void requestCancel();

	void handleProjectError(int type, const QString &errorMessage);
	void showErrorPage(const QString &errorMessage);
	void handleSync(unsigned int syncId);
	void handleOpenSucceeded();
	void handleOverviewGenerated();

	unsigned int grabSyncId();

	static QWidget *makeLoadPage(QLabel *&label);
	static QWidget *makeErrorPage(QLabel *&label);

	QString m_tempPath;
	project::ProjectWrangler *m_projectWrangler = nullptr;
	QStackedWidget *m_stack;
	QWidget *m_loadPage;
	QLabel *m_loadLabel;
	QWidget *m_errorPage;
	QLabel *m_errorLabel;
	QWidget *m_overviewPage = nullptr;
	QDialogButtonBox *m_buttons;
	unsigned int m_lastSyncId = 0u;
	unsigned int m_cancelSyncId = 0u;
};

}

#endif
