// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_UTILS_RECENTS_H
#define DESKTOP_UTILS_RECENTS_H

#include <QObject>
#include <QPixmap>
#include <QVector>

class QMenu;
class QPixmap;
class QSqlQuery;
class QString;

namespace utils {

class StateDatabase;

class Recents final : public QObject {
	Q_OBJECT
public:
#ifndef __EMSCRIPTEN__
	struct File {
		long long id;
		QString path;
	};
#endif
	struct Host {
		long long id;
		QString host;
		int port;
		bool joined;
		bool hosted;

		QString toString() const;
		static QString toStringFrom(const QString &host, int port);
	};

	explicit Recents(StateDatabase &state);

#ifndef __EMSCRIPTEN__
	QVector<File> getFiles() const;
	int fileCount() const;
	void addFile(const QString &path);
	bool removeFileById(long long id);
	void bindFileMenu(QMenu *menu);
#endif

	QVector<Host> getHosts() const;
	QString getMostRecentHostAddress() const;
	int hostCount() const;
	void addHost(const QString &host, int port, bool joined, bool hosted);
	bool removeHostById(long long id);

signals:
#ifndef __EMSCRIPTEN__
	void recentFilesChanged();
#endif
	void recentHostsChanged();

private:
	static constexpr int MAX_RECENT_FILES = 25;
	static constexpr int MAX_MENU_FILES = 9;
	static constexpr int MAX_RECENT_HOSTS = 25;
	static constexpr int FLAG_JOINED = 0x1;
	static constexpr int FLAG_HOSTED = 0x2;

	void createTables();
#ifndef __EMSCRIPTEN__
	void migrateFilesFromSettings();
	void migrateHostsFromSettings();
#endif
	bool removeById(const QString &sql, int id);

	static void
	collectSettingsHost(QVector<Host> &rhs, const QString &input, bool join);

#ifndef __EMSCRIPTEN__
	void updateFileMenu(QMenu *menu) const;
	QStringList getFileMenuPaths() const;
#endif

	static int packHostFlags(bool joined, bool hosted);

	StateDatabase &m_state;
};

}

#endif
