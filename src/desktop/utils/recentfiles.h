// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_UTILS_RECENTFILES_H
#define DESKTOP_UTILS_RECENTFILES_H

#include <QObject>
#include <QPixmap>
#include <QVector>

class QMenu;
class QPixmap;
class QSqlQuery;
class QString;

namespace utils {

class StateDatabase;

class RecentFiles final : public QObject {
	Q_OBJECT
public:
	struct File {
		long long id;
		QString path;
	};

	explicit RecentFiles(StateDatabase &state);

	QVector<File> getFiles() const;
	int fileCount() const;

	void addFile(const QString &path);
	bool removeFileById(long long id);

	void bindMenu(QMenu *menu);

signals:
	void recentFilesChanged();

private:
	static constexpr int MAX_RECENT_FILES = 25;
	static constexpr int MAX_MENU_FILES = 9;

	void createTables();
	void migrateFromSettings();

	void updateMenu(QMenu *menu) const;
	QStringList getMenuPaths() const;

	StateDatabase &m_state;
};

}

#endif
