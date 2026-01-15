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
	ProjectDialog(QWidget *parent = nullptr);

	void openProject(const QString &path);

private:
	void requestCancel();

	void handleSync(unsigned int syncId);

	unsigned int grabSyncId();

	project::ProjectWrangler *m_projectWrangler = nullptr;
	QStackedWidget *m_stack;
	QWidget *m_loadPage;
	QLabel *m_loadLabel;
	QDialogButtonBox *m_buttons;
	unsigned int m_lastSyncId = 0u;
	unsigned int m_cancelSyncId = 0u;
};

}

#endif
