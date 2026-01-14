// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_PROJECTINFODIALOG_H
#define DESKTOP_DIALOGS_PROJECTINFODIALOG_H
#include <QDialog>
#include <QVector>

class QLabel;
class QTableWidget;
class QTableWidgetItem;
struct DP_ProjectInfo;
struct DP_ProjectInfoHeader;
struct DP_ProjectInfoSession;
struct DP_ProjectInfoSnapshot;

namespace dialogs {

class ProjectInfoDialog final : public QDialog {
	Q_OBJECT
public:
	ProjectInfoDialog(QWidget *parent = nullptr);

	void loadProjectInfo(const QString &path);

private:
	bool queryProjectInfo(const QString &path, QString &outErrorMessage);
	void onProjectInfo(const DP_ProjectInfo &info);
	void onProjectInfoHeader(const DP_ProjectInfoHeader &info);
	void onProjectInfoSession(const DP_ProjectInfoSession &info);
	void onProjectInfoSnapshot(const DP_ProjectInfoSnapshot &info);
	static void onProjectInfoCallback(void *user, const DP_ProjectInfo *info);

	static void
	addRow(QTableWidget *table, const QVector<QTableWidgetItem *> items);

	static QTableWidgetItem *
	loadThumbnail(const unsigned char *data, size_t size);

	static QTableWidgetItem *loadTimestamp(double timestamp);

	QLabel *m_statusLabel;
	QTableWidget *m_headerTable;
	QTableWidget *m_sessionsTable;
	QTableWidget *m_snapshotsTable;
};

}

#endif
