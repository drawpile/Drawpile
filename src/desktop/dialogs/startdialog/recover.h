// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_STARTDIALOG_RECOVER_H
#define DESKTOP_DIALOGS_STARTDIALOG_RECOVER_H
#include "desktop/dialogs/startdialog/page.h"
#include "libclient/project/recoverymodel.h"
#include <QFrame>

class QLabel;
class QScrollArea;
class QVBoxLayout;

namespace dialogs {
namespace startdialog {

class RecoveryEntryWidget : public QFrame {
	Q_OBJECT
public:
	explicit RecoveryEntryWidget(
		const project::RecoveryEntry &entry, QWidget *parent = nullptr);

Q_SIGNALS:
	void recoveryRequested(const QString &path);
	void removalRequested(const QString &path);

private:
	void requestRecovery();
	void requestRemoval();

	project::RecoveryEntry m_entry;
};

class Recover final : public Page {
	Q_OBJECT
public:
	Recover(QWidget *parent = nullptr);

	void activate() override;

	bool checkPotentialRecovery();

Q_SIGNALS:
	void hideLinks();

private:
	void updateRecoveryEntries();
	void clearContent();

	project::RecoveryModel *m_recoveryModel;
	QScrollArea *m_scroll;
	QWidget *m_content;
	QVBoxLayout *m_contentLayout;
};

}
}

#endif
