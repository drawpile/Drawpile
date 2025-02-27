// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_SERVER_FILEDHISTORY_H
#define DP_SERVER_FILEDHISTORY_H
#include "libserver/sessionhistory.h"
#include "libshared/net/protover.h"
#include "libshared/util/qtcompat.h"
#include <QDir>
#include <QVector>

struct DP_BinaryReader;
struct DP_BinaryWriter;

namespace server {

class FiledHistory final : public SessionHistory {
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
	static FiledHistory *startNew(
		const QDir &dir, const QString &id, const QString &alias,
		const protocol::ProtocolVersion &version, const QString &founder,
		QObject *parent = nullptr);

	/**
	 * @brief Load a session from file
	 * @param path
	 * @param parent
	 * @return
	 */
	static FiledHistory *load(const QString &path, QObject *parent = nullptr);

	/**
	 * @brief Close the currently open block (if any) and start a new one
	 */
	void closeBlock();

	/**
	 * @brief Enable archival mode
	 *
	 * In archive mode, files are not deleted when session ends or is reset.
	 * Instead, ".archived" is appended to the end of the journal file on
	 * termination.
	 * @param archive
	 */
	void setArchive(bool archive) { m_archive = archive; }

	//! Get the metadata journal file name for the given session ID
	static QString journalFilename(const QString &id);

	QString idAlias() const override { return m_alias; }
	QString founderName() const override { return m_founder; }
	protocol::ProtocolVersion protocolVersion() const override
	{
		return m_version;
	}
	QByteArray passwordHash() const override { return m_password; }
	QByteArray opwordHash() const override { return m_opword; }
	int maxUsers() const override { return m_maxUsers; }
	QString title() const override { return m_title; }
	size_t autoResetThreshold() const override { return m_autoResetThreshold; }
	Flags flags() const override { return m_flags; }

	void setFounderName(const QString &founder) override;
	void setPasswordHash(const QByteArray &password) override;
	void setOpwordHash(const QByteArray &opword) override;
	void setMaxUsers(int max) override;
	void setTitle(const QString &title) override;
	void setFlags(Flags f) override;
	void setAutoResetThreshold(size_t limit) override;
	int nextCatchupKey() override;
	void joinUser(uint8_t id, const QString &name) override;

	bool isStreamResetIoAvailable() const override;
	qint64 resetStreamForkPos() const override;
	qint64 resetStreamHeaderPos() const override;

	void terminate() override;
	void cleanupBatches(long long before) override;
	std::tuple<net::MessageList, long long>
	getBatch(long long after) const override;

	void addAnnouncement(const QString &) override;
	void removeAnnouncement(const QString &url) override;
	QStringList announcements() const override { return m_announcements; }

	void setAuthenticatedOperator(const QString &authId, bool op) override;

	void setAuthenticatedTrust(const QString &authId, bool trusted) override;

	void setAuthenticatedUsername(
		const QString &authId, const QString &username) override;

	Invite *createInvite(
		const QString &createdBy, int maxUses, bool trust, bool op) override;
	bool removeInvite(const QString &secret) override;
	CheckInviteResult checkInvite(
		Client *client, const QString &secret, bool use,
		QString *outClientKey = nullptr, Invite **outInvite = nullptr,
		InviteUse **outInviteUse = nullptr) override;

protected:
	void historyAdd(const net::Message &msg) override;
	void historyReset(const net::MessageList &newHistory) override;
	void historyAddBan(
		int id, const QString &username, const QHostAddress &ip,
		const QString &extAuthId, const QString &sid,
		const QString &bannedBy) override;
	void historyRemoveBan(int id) override;

	StreamResetStartResult
	openResetStream(const net::MessageList &serverSideStateMessages) override;
	StreamResetAddResult
	addResetStreamMessage(const net::Message &msg) override;
	StreamResetPrepareResult prepareResetStream() override;
	bool resolveResetStream(
		long long newFirstIndex, long long &outMessageCount,
		size_t &outSizeInBytes, QString &outError) override;
	void discardResetStream() override;

	void timerEvent(QTimerEvent *event) override;

private:
	struct Block {
		qint64 startOffset;
		long long startIndex;
		long long count;
		qint64 endOffset;
		net::MessageList messages;

		Block(qint64 offset, long long index)
			: startOffset(offset)
			, startIndex(index)
			, count(0LL)
			, endOffset(offset)
		{
		}
	};

	class BlockCache {
	public:
		const Block &lastBlock() const { return m_blocks.last(); }
		Block &findBlock(long long after);

		void addBlock(qint64 offset, long long index);
		void addToLastBlock(const net::Message &msg, size_t len);
		void incrementLastBlock(size_t len);
		void closeLastBlock();

		void cleanup(long long before);

		long long totalMessageCount() const;

		compat::sizetype size() const { return m_blocks.size(); }
		void clear() { m_blocks.clear(); }
		bool isEmpty() const { return m_blocks.isEmpty(); }

		void replaceWithResetStream(
			BlockCache &streamCache, compat::sizetype blockIndex,
			long long newFirstIndex);

	private:
		void incrementBlock(Block &b, size_t len);

		QVector<Block> m_blocks;
	};

	FiledHistory(
		const QDir &dir, QFile *journal, const QString &id,
		const QString &alias, const protocol::ProtocolVersion &version,
		const QString &founder, QObject *parent);
	FiledHistory(
		const QDir &dir, QFile *journal, const QString &id, QObject *parent);

	bool create();
	bool load();
	bool scanBlocks();
	bool initRecording();
	bool openRecording(
		const QString &fileName, bool stream, QFile **outRecording,
		DP_BinaryReader **outReader, DP_BinaryWriter **outWriter);

	void writeFileEntryToJournal(const QString &fileName);
	void writeStringToJournal(const QString &s);
	void writeBytesToJournal(const QByteArray &bytes);
	void flushRecording();
	void flushJournal();
	void removeOrArchive(QFile *recording) const;

	bool copyForkMessagesToResetStream(QString &outError);

	QDir m_dir;
	QFile *m_journal;
	QFile *m_recording;
	DP_BinaryReader *m_reader;
	DP_BinaryWriter *m_writer;

	// Current state:
	QString m_alias;
	QString m_founder;
	QString m_title;
	protocol::ProtocolVersion m_version;
	QByteArray m_password;
	QByteArray m_opword;
	int m_maxUsers;
	size_t m_autoResetThreshold;
	Flags m_flags;
	int m_nextCatchupKey;
	QStringList m_announcements;

	mutable BlockCache m_blockCache;
	int m_fileCount;
	bool m_archive;

	QString m_resetStreamFileName;
	QFile *m_resetStreamRecording = nullptr;
	DP_BinaryReader *m_resetStreamReader = nullptr;
	DP_BinaryWriter *m_resetStreamWriter = nullptr;
	int m_resetStreamFileCount = -1;
	qint64 m_resetStreamForkPos;
	qint64 m_resetStreamHeaderPos;
	compat::sizetype m_resetStreamBlockIndex;
	BlockCache m_resetStreamBlockCache;
};

}

#endif
