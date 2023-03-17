/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017-2018 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DP_SERVER_FILEDHISTORY_H
#define DP_SERVER_FILEDHISTORY_H

#include "libserver/sessionhistory.h"
#include "libshared/net/protover.h"

#include <QDir>
#include <QVector>
#include <QSet>

namespace server {

class FiledHistory final : public SessionHistory
{
	Q_OBJECT
public:
	~FiledHistory() override;

	/**
	 * @brief Start a new file backed history
	 * @param dir where to put the session files
	 * @param id session ID
	 * @param alias ID alias
	 * @param version full protocol version
	 * @param founder name of the session founder
	 * @param parent
	 * @return FiledHistory object or nullptr on error
	 */
	static FiledHistory *startNew(const QDir &dir, const QString &id, const QString &alias, const protocol::ProtocolVersion &version, const QString &founder, QObject *parent=nullptr);

	/**
	 * @brief Load a session from file
	 * @param path
	 * @param parent
	 * @return
	 */
	static FiledHistory *load(const QString &path, QObject *parent=nullptr);

	/**
	 * @brief Close the currently open block (if any) and start a new one
	 */
	void closeBlock();

	/**
	 * @brief Enable archival mode
	 *
	 * In archive mode, files are not deleted when session ends or is reset.
	 * Instead, ".archived" is appended to the end of the journal file on termination.
	 * @param archive
	 */
	void setArchive(bool archive) { m_archive = archive; }

	//! Get the metadata journal file name for the given session ID
	static QString journalFilename(const QString &id);

	QString idAlias() const override { return m_alias; }
	QString founderName() const override { return m_founder; }
	protocol::ProtocolVersion protocolVersion() const override { return m_version; }
	QByteArray passwordHash() const override { return m_password; }
	QByteArray opwordHash() const override { return m_opword; }
	int maxUsers() const override { return m_maxUsers; }
	QString title() const override { return m_title; }
	uint autoResetThreshold() const override { return m_autoResetThreshold; }
	Flags flags() const override { return m_flags; }

	void setPasswordHash(const QByteArray &password) override;
	void setOpwordHash(const QByteArray &opword) override;
	void setMaxUsers(int max) override;
	void setTitle(const QString &title) override;
	void setFlags(Flags f) override;
	void setAutoResetThreshold(uint limit) override;
	void joinUser(uint8_t id, const QString &name) override;

	void terminate() override;
	void cleanupBatches(int before) override;
	std::tuple<protocol::MessageList, int> getBatch(int after) const override;

	void addAnnouncement(const QString &) override;
	void removeAnnouncement(const QString &url) override;
	QStringList announcements() const override { return m_announcements; }

	void setAuthenticatedOperator(const QString &authId, bool op) override;
	void setAuthenticatedTrust(const QString &authId, bool trusted) override;
	bool isOperator(const QString &authId) const override { return m_ops.contains(authId); }
	bool isTrusted(const QString &authId) const override { return m_trusted.contains(authId); }
	bool isAuthenticatedOperators() const override { return !m_ops.isEmpty(); }

protected:
	void historyAdd(const protocol::MessagePtr &msg) override;
	void historyReset(const protocol::MessageList &newHistory) override;
	void historyAddBan(int id, const QString &username, const QHostAddress &ip, const QString &extAuthId, const QString &bannedBy) override;
	void historyRemoveBan(int id) override;

	void timerEvent(QTimerEvent *event) override;

private:
	FiledHistory(const QDir &dir, QFile *journal, const QString &id, const QString &alias, const protocol::ProtocolVersion &version, const QString &founder, QObject *parent);
	FiledHistory(const QDir &dir, QFile *journal, const QString &id, QObject *parent);

	struct Block {
		qint64 startOffset;
		int startIndex;
		int count;
		qint64 endOffset;
		protocol::MessageList messages;
	};

	bool create();
	bool load();
	bool scanBlocks();
	bool initRecording();

	QDir m_dir;
	QFile *m_journal;
	QFile *m_recording;

	// Current state:
	QString m_alias;
	QString m_founder;
	QString m_title;
	protocol::ProtocolVersion m_version;
	QByteArray m_password;
	QByteArray m_opword;
	int m_maxUsers;
	uint m_autoResetThreshold;
	Flags m_flags;
	QStringList m_announcements;
	QSet<QString> m_ops;
	QSet<QString> m_trusted;

	QVector<Block> m_blocks;
	int m_fileCount;
	bool m_archive;
};

}

#endif
