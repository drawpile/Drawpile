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
	void removalRequested(const QString &path);
	void openRequested(const QString &savePath);

private:
	void promptRemoval();
	void requestRemoval();

#ifdef __EMSCRIPTEN__
	void download();
#else
	void save();
	bool saveTo(const QString &savePath, QString &outError);
	bool compareSaved(const QString &savePath, QString &outError);
#endif

	QString getSuggestedExportBaseName() const;

	QString m_path;
	bool m_locked;
};

class Recover final : public Page {
	Q_OBJECT
public:
	Recover(QWidget *parent = nullptr);

	void activate() override;

	bool checkPotentialRecovery();

Q_SIGNALS:
	void hideLinks();
	void openPath(const QString &path);

private:
	void updateRecoveryEntries();
	void clearContent();

	void removePath(const QString &path);
	bool tryRemovePath(const QFileInfo &fileInfo, QString &outError);

	project::RecoveryModel *m_recoveryModel;
	QScrollArea *m_scroll;
	QWidget *m_content;
	QVBoxLayout *m_contentLayout;
};

}
}

#endif
