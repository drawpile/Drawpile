// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/input.h>
#include <dpcommon/input_qt.h>
#include <dpcommon/output.h>
#include <dpcommon/output_qt.h>
#include <dpmsg/binary_reader.h>
#include <dpmsg/binary_writer.h>
}
#include "libserver/filedhistory.h"
#include "libshared/util/filename.h"
#include "libshared/util/passwordhash.h"
#include <QDebug>
#include <QFile>
#include <QScopedPointer>
#include <QSet>
#include <QTimerEvent>
#include <QVarLengthArray>
#include <dpcommon/platform_qt.h>

namespace server {

// A block is closed when its size goes above this limit
static const qint64 MAX_BLOCK_SIZE = 0xffff * 10;

FiledHistory::FiledHistory(
	const QDir &dir, QFile *journal, const QString &id, const QString &alias,
	const protocol::ProtocolVersion &version, const QString &founder,
	QObject *parent)
	: SessionHistory(id, parent)
	, m_dir(dir)
	, m_journal(journal)
	, m_recording(nullptr)
	, m_reader(nullptr)
	, m_writer(nullptr)
	, m_alias(alias)
	, m_founder(founder)
	, m_version(version)
	, m_maxUsers(254)
	, m_flags()
	, m_nextCatchupKey(INITIAL_CATCHUP_KEY)
	, m_fileCount(0)
	, m_archive(false)
{
	Q_ASSERT(journal);

	// Flush the recording file periodically
	startTimer(1000 * 30, Qt::VeryCoarseTimer);
}

FiledHistory::FiledHistory(
	const QDir &dir, QFile *journal, const QString &id, QObject *parent)
	: FiledHistory(
		  dir, journal, id, QString(), protocol::ProtocolVersion(), QString(),
		  parent)
{
}

FiledHistory::~FiledHistory()
{
	DP_binary_writer_free(m_resetStreamWriter);
	DP_binary_reader_free(m_resetStreamReader);
	DP_binary_writer_free(m_writer);
	DP_binary_reader_free(m_reader);
}

QString FiledHistory::journalFilename(const QString &id)
{
	return id + ".session";
}

static QString
uniqueRecordingFilename(const QDir &dir, const QString &id, int idx)
{
	QString idstr = id;
	if(idx > 1)
		idstr = QString("%1_r%2").arg(id).arg(idx);

	// The filename should be unique already, but better safe than sorry
	return utils::uniqueFilename(dir, idstr, "dprec", false);
}

FiledHistory *FiledHistory::startNew(
	const QDir &dir, const QString &id, const QString &alias,
	const protocol::ProtocolVersion &version, const QString &founder,
	QObject *parent)
{
	QFile *journal =
		new QFile(QFileInfo(dir, journalFilename(id)).absoluteFilePath());

	FiledHistory *fh =
		new FiledHistory(dir, journal, id, alias, version, founder, parent);
	journal->setParent(fh);

	if(!fh->create()) {
		delete fh;
		return nullptr;
	}

	return fh;
}

FiledHistory *FiledHistory::load(const QString &path, QObject *parent)
{
	const QString filename = QFileInfo(path).baseName();
	const QDir dir = QFileInfo(path).dir();
	// TODO validate session ID here

	QFile *journal = new QFile(path);
	if(!journal->open(QFile::ReadWrite)) {
		qWarning() << path << journal->errorString();
		delete journal;
		return nullptr;
	}

	FiledHistory *fh = new FiledHistory(dir, journal, filename, parent);
	journal->setParent(fh);
	if(!fh->load()) {
		delete fh;
		return nullptr;
	}

	return fh;
}

bool FiledHistory::create()
{
	if(m_journal->exists()) {
		qWarning() << m_journal->fileName() << "already exists!";
		return false;
	}

	if(!m_journal->open(DP_QT_WRITE_FLAGS)) {
		qWarning() << m_journal->fileName() << m_journal->errorString();
		return false;
	}

	if(!initRecording())
		return false;

	if(m_alias.isEmpty()) {
		writeStringToJournal(QStringLiteral("FOUNDER %1\n").arg(m_founder));
	} else {
		writeStringToJournal(
			QStringLiteral("ALIAS %1\nFOUNDER %2\n").arg(m_alias, m_founder));
	}

	return true;
}

bool FiledHistory::initRecording()
{
	Q_ASSERT(m_blockCache.isEmpty());
	Q_ASSERT(!m_reader);
	Q_ASSERT(!m_writer);
	QString fileName = uniqueRecordingFilename(m_dir, id(), ++m_fileCount);
	if(!openRecording(fileName, false, &m_recording, &m_reader, &m_writer)) {
		return false;
	}

	writeFileEntryToJournal(fileName);
	m_blockCache.addBlock(m_recording->pos(), firstIndex());
	return true;
}

bool FiledHistory::openRecording(
	const QString &fileName, bool stream, QFile **outRecording,
	DP_BinaryReader **outReader, DP_BinaryWriter **outWriter)
{
	QFile *recording = new QFile(m_dir.absoluteFilePath(fileName), this);
	if(!recording->open(QFile::ReadWrite)) {
		qWarning(
			"Error opening '%s': %s", qUtf8Printable(fileName),
			qUtf8Printable(recording->errorString()));
		recording->remove();
		delete recording;
		return false;
	}

	DP_BinaryReader *reader = DP_binary_reader_new(
		DP_qfile_input_new(recording, false, DP_input_new),
		DP_BINARY_READER_FLAG_NO_LENGTH | DP_BINARY_READER_FLAG_NO_HEADER);
	DP_BinaryWriter *writer = DP_binary_writer_new(
		DP_qfile_output_new(recording, false, DP_output_new));

	JSON_Value *headerValue = json_value_init_object();
	JSON_Object *headerObject = json_value_get_object(headerValue);
	json_object_set_string( // the hosting client's protocol version
		headerObject, "version", qUtf8Printable(m_version.asString()));
	if(stream) {
		json_object_set_number(
			headerObject, "streamfork", double(m_resetStreamForkPos));
	}
	bool ok = DP_binary_writer_write_header(writer, headerObject);
	if(!ok) {
		qWarning(
			"Error writing header to '%s': %s", qUtf8Printable(fileName),
			DP_error());
	}
	json_value_free(headerValue);

	if(ok) {
		if(!recording->flush()) {
			qWarning(
				"Error flushing recording to '%s': %s",
				qUtf8Printable(fileName),
				qUtf8Printable(recording->errorString()));
			ok = false;
		}
	}

	if(ok) {
		*outRecording = recording;
		*outReader = reader;
		*outWriter = writer;
		return true;
	} else {
		DP_binary_writer_free(writer);
		DP_binary_reader_free(reader);
		recording->remove();
		delete recording;
		return false;
	}
}

void FiledHistory::writeFileEntryToJournal(const QString &fileName)
{
	writeStringToJournal(QStringLiteral("FILE %1\n").arg(fileName));
}

void FiledHistory::writeStringToJournal(const QString &s)
{
	writeBytesToJournal(s.toUtf8());
}

void FiledHistory::writeBytesToJournal(const QByteArray &bytes)
{
	if(m_journal) {
		qint64 written = m_journal->write(bytes);
		if(written == bytes.size()) {
			flushJournal();
		} else if(written < 0) {
			qWarning(
				"Error writing to journal: %s",
				qUtf8Printable(m_journal->errorString()));
		} else {
			qWarning(
				"Tried to write %zu byte(s) to journal, but wrote %zu",
				size_t(bytes.size()), size_t(written));
		}
	}
}

void FiledHistory::flushRecording()
{
	if(m_recording) {
		if(!m_recording->flush()) {
			qWarning(
				"Error flushing recording: %s",
				qUtf8Printable(m_recording->errorString()));
		}
	}
}

void FiledHistory::flushJournal()
{
	if(m_journal) {
		if(!m_journal->flush()) {
			qWarning(
				"Error flushing journal: %s",
				qUtf8Printable(m_journal->errorString()));
		}
	}
}

void FiledHistory::removeOrArchive(QFile *f) const
{
	if(f) {
		if(m_archive) {
			QString fileName = f->fileName();
			if(!f->rename(fileName + QStringLiteral(".archived"))) {
				qWarning(
					"Error archiving '%s': %s", qUtf8Printable(fileName),
					qUtf8Printable(f->errorString()));
			}
		} else if(!f->remove()) {
			qWarning(
				"Error removing '%s': %s", qUtf8Printable(f->fileName()),
				qUtf8Printable(f->errorString()));
		}
	}
}

bool FiledHistory::load()
{
	QByteArray line;
	QByteArray cmd;
	QString recordingFile;

	// Parse the journal
	do {
		line = m_journal->readLine().trimmed();
		if(line.isEmpty() || line.at(0) == '#')
			continue;

		// qDebug() << QString::fromUtf8(line.trimmed());

		QByteArray params;
		if(int sep = line.indexOf(' '); sep != -1) {
			cmd = line.left(sep).trimmed();
			params = line.mid(sep + 1).trimmed();
		} else {
			cmd = line;
		}

		if(cmd == QByteArrayLiteral("FILE")) {
			recordingFile = QString::fromUtf8(params);
			++m_fileCount;
			m_blockCache.clear();

		} else if(cmd == QByteArrayLiteral("ALIAS")) {
			if(m_alias.isEmpty())
				m_alias = QString::fromUtf8(params);
			else
				qWarning() << id() << "alias set twice.";

		} else if(cmd == QByteArrayLiteral("FOUNDER")) {
			m_founder = QString::fromUtf8(params);

		} else if(cmd == QByteArrayLiteral("PASSWORD")) {
			if(params.isEmpty() || passwordhash::isValidHash(params))
				m_password = params;

		} else if(cmd == QByteArrayLiteral("OPWORD")) {
			if(params.isEmpty() || passwordhash::isValidHash(params))
				m_opword = params;

		} else if(cmd == QByteArrayLiteral("MAXUSERS")) {
			m_maxUsers = qBound(1, params.toInt(), 254);

		} else if(cmd == QByteArrayLiteral("AUTORESET")) {
			m_autoResetThreshold = params.toULong();

		} else if(cmd == QByteArrayLiteral("TITLE")) {
			m_title = params;

		} else if(cmd == QByteArrayLiteral("FLAGS")) {
			Flags flags;
			for(const QByteArray &f : params.split(' ')) {
				if(f == QStringLiteral("persistent")) {
					flags |= Persistent;
				} else if(f == QStringLiteral("preserveChat")) {
					flags |= PreserveChat;
				} else if(f == QStringLiteral("nsfm")) {
					flags |= Nsfm;
				} else if(f == QStringLiteral("deputies")) {
					flags |= Deputies;
				} else if(f == QStringLiteral("authonly")) {
					flags |= AuthOnly;
				} else if(f == QStringLiteral("idleoverride")) {
					flags |= IdleOverride;
				} else if(f == QStringLiteral("allowweb")) {
					flags |= AllowWeb;
				} else if(f == QStringLiteral("autotitle")) {
					flags |= AutoTitle;
				} else if(f == QStringLiteral("invites")) {
					flags |= Invites;
				} else {
					qWarning()
						<< id() << "unknown flag:" << QString::fromUtf8(f);
				}
			}
			m_flags = flags;

		} else if(cmd == QByteArrayLiteral("BAN")) {
			const QList<QByteArray> args = params.split(' ');
			int length = args.length();
			if(length < 5 || length > 6) {
				qWarning() << id()
						   << "invalid ban entry:" << QString::fromUtf8(params);
			} else {
				int id = args.at(0).toInt();
				QString name{QString::fromUtf8(
					QByteArray::fromPercentEncoding(args.at(1)))};
				QHostAddress ip{QString::fromUtf8(args.at(2))};
				QString extAuthId{QString::fromUtf8(
					QByteArray::fromPercentEncoding(args.at(3)))};
				QString bannedBy{QString::fromUtf8(
					QByteArray::fromPercentEncoding(args.at(4)))};
				QString sid;
				if(length >= 6) {
					sid = QString::fromUtf8(
						QByteArray::fromPercentEncoding(args.at(5)));
				}
				m_banlist.addBan(name, ip, extAuthId, sid, bannedBy, id);
			}

		} else if(cmd == QByteArrayLiteral("UNBAN")) {
			m_banlist.removeBan(params.toInt());

		} else if(cmd == QByteArrayLiteral("ANNOUNCE")) {
			if(!m_announcements.contains(params))
				m_announcements << params;

		} else if(cmd == QByteArrayLiteral("UNANNOUNCE")) {
			m_announcements.removeAll(params);

		} else if(cmd == QByteArrayLiteral("USER")) {
			const QList<QByteArray> args = params.split(' ');
			if(args.length() != 2) {
				qWarning() << "Invalid USER entry:"
						   << QString::fromUtf8(params);
			} else {
				int id = args.at(0).toInt();
				QString name{QString::fromUtf8(
					QByteArray::fromPercentEncoding(args.at(1)))};
				idQueue().setIdForName(id, name);
			}

		} else if(cmd == QByteArrayLiteral("OP")) {
			QString authId = QString::fromUtf8(params);
			if(!authId.isEmpty()) {
				SessionHistory::setAuthenticatedOperator(authId, true);
			}

		} else if(cmd == QByteArrayLiteral("DEOP")) {
			QString authId = QString::fromUtf8(params);
			SessionHistory::setAuthenticatedOperator(authId, false);

		} else if(cmd == QByteArrayLiteral("TRUST")) {
			const QString authId = QString::fromUtf8(params);
			if(!authId.isEmpty()) {
				SessionHistory::setAuthenticatedTrust(authId, true);
			}

		} else if(cmd == QByteArrayLiteral("UNTRUST")) {
			QString authId = QString::fromUtf8(params);
			SessionHistory::setAuthenticatedTrust(authId, false);

		} else if(cmd == QByteArrayLiteral("AUTHNAME")) {
			const QList<QByteArray> args = params.split(' ');
			if(args.length() != 2) {
				qWarning() << "Invalid AUTHNAME entry:"
						   << QString::fromUtf8(params);
			} else {
				QString authId = QString::fromUtf8(
					QByteArray::fromPercentEncoding(args.at(0)));
				QString username = QString::fromUtf8(
					QByteArray::fromPercentEncoding(args.at(1)));
				if(!authId.isEmpty() && !username.isEmpty()) {
					SessionHistory::setAuthenticatedUsername(authId, username);
				}
			}

		} else if(cmd == QByteArrayLiteral("CATCHUP")) {
			m_nextCatchupKey =
				qBound(MIN_CATCHUP_KEY, params.toInt(), MAX_CATCHUP_KEY);

		} else if(cmd == QByteArrayLiteral("INV")) {
			QList<QByteArray> args = params.split(' ');
			int argc = args.size();
			if(argc < 3 || argc > 4) {
				qWarning() << "Invalid INV entry:" << QString::fromUtf8(params);
			} else {
				QString secret = QString::fromUtf8(args[0]);
				QString timestamp = QString::fromUtf8(args[1]);
				bool trust = args[2].contains(QByteArrayLiteral(",trust"));
				bool op = args[2].contains(QByteArrayLiteral(",op"));
				if(int pos = args[2].indexOf(','); pos != -1) {
					args[2].truncate(pos);
				}
				bool maxUsesOk;
				int maxUses = args[2].toInt(&maxUsesOk);
				QString createdBy =
					argc < 4 ? QString()
							 : QString::fromUtf8(
								   QByteArray::fromPercentEncoding(args[3]));
				if(!secret.isEmpty() && maxUsesOk) {
					setInvite(secret, createdBy, timestamp, maxUses, trust, op);
				} else {
					qWarning() << "Invalid INV entry data:"
							   << QString::fromUtf8(params);
				}
			}

		} else if(cmd == QByteArrayLiteral("UNV")) {
			QString secret = QString::fromUtf8(params);
			if(!SessionHistory::removeInvite(secret)) {
				qWarning() << "No matching invite for UNV" << secret;
			}

		} else if(cmd == QByteArrayLiteral("USV")) {
			QList<QByteArray> args = params.split(' ');
			if(args.size() == 3) {
				QString secret = QString::fromUtf8(args[0]);
				QString clientKey = QString::fromUtf8(args[1]);
				QString name = QString::fromUtf8(args[2]);
				CheckInviteResult result =
					checkInviteFor(clientKey, name, secret, true);
				if(result != CheckInviteResult::InviteUsed &&
				   result != CheckInviteResult::AlreadyInvitedNameChanged) {
					qWarning() << "Unexpected USV result" << int(result);
				}
			} else {
				qWarning() << "Invalid USV entry:" << QString::fromUtf8(params);
			}

		} else {
			qWarning() << id()
					   << "unknown journal entry:" << QString::fromUtf8(cmd);
		}
	} while(!m_journal->atEnd());

	// The latest recording file must exist
	if(recordingFile.isEmpty()) {
		qWarning() << id() << "content file not set!";
		return false;
	}

	m_recording = new QFile(m_dir.absoluteFilePath(recordingFile), this);

	if(!m_recording->exists()) {
		qWarning() << recordingFile << "not found!";
		return false;
	}

	if(!m_recording->open(QFile::ReadWrite)) {
		qWarning() << recordingFile << m_recording->errorString();
		return false;
	}

	m_reader = DP_binary_reader_new(
		DP_qfile_input_new(m_recording, false, DP_input_new),
		DP_BINARY_READER_FLAG_NO_LENGTH);
	if(!m_reader) {
		qWarning() << recordingFile << "invalid header";
		return false;
	}

	JSON_Object *header =
		json_value_get_object(DP_binary_reader_header(m_reader));

	m_version = protocol::ProtocolVersion::fromString(
		QString::fromUtf8(json_object_get_string(header, "version")));
	if(!m_version.isValid()) {
		qWarning() << recordingFile << "invalid protocol version";
		return false;
	}

	if(m_version.serverVersion() !=
	   protocol::ProtocolVersion::current().serverVersion()) {
		qWarning() << recordingFile << "incompatible server version";
		return false;
	}

	qint64 startOffset = m_recording->pos();
	Q_ASSERT(!m_writer);
	m_writer = DP_binary_writer_new(
		DP_qfile_output_new(m_recording, false, DP_output_new));

	// Scan the recording file and build the index of blocks
	if(!scanBlocks()) {
		qWarning() << recordingFile << "error occurred during indexing";
		return false;
	}

	const Block &b = m_blockCache.lastBlock();
	historyLoaded(b.endOffset - startOffset, b.startIndex + b.count);

	// If a loaded session is empty, the server expects the first joining client
	// to supply the initial content, while the client is expecting to join
	// an existing session.
	if(sizeInBytes() == 0) {
		qWarning() << recordingFile << "empty session!";
		return false;
	}

	return true;
}

bool FiledHistory::scanBlocks()
{
	Q_ASSERT(m_blockCache.isEmpty());
	// Note: m_recording should be at the start of the recording

	m_blockCache.addBlock(m_recording->pos(), firstIndex());

	QSet<uint8_t> users;

	do {
		uint8_t msgType, ctxId;
		int msglen = DP_binary_reader_skip_message(m_reader, &msgType, &ctxId);
		if(msglen < 0) {
			// Truncated message encountered.
			// Rewind back to the end of the previous message
			qint64 offset = m_blockCache.lastBlock().endOffset;
			qWarning(
				"Recording '%s' truncated at %lld",
				qUtf8Printable(m_recording->fileName()),
				static_cast<long long>(offset));
			m_recording->seek(offset);
			break;
		}
		m_blockCache.incrementLastBlock(msglen);
		Q_ASSERT(m_blockCache.lastBlock().endOffset == m_recording->pos());

		switch(msgType) {
		case DP_MSG_JOIN:
			users.insert(ctxId);
			break;
		case DP_MSG_LEAVE:
			users.remove(ctxId);
			idQueue().reserveId(ctxId);
			break;
		}
	} while(!m_recording->atEnd());

	// There should be no users at the end of the recording.
	for(const uint8_t user : users) {
		net::Message msg = net::makeLeaveMessage(user);
		m_blockCache.incrementLastBlock(msg.length());
		if(DP_binary_writer_write_message(m_writer, msg.get()) == 0) {
			return false;
		}
		idQueue().reserveId(user);
	}
	return true;
}

void FiledHistory::terminate()
{
	discardResetStream();
	DP_binary_reader_free(m_reader);
	m_reader = nullptr;
	DP_binary_writer_free(m_writer);
	m_writer = nullptr;
	m_recording->close();
	m_journal->close();
	removeOrArchive(m_journal);
	removeOrArchive(m_recording);
}

void FiledHistory::closeBlock()
{
	// Flush the output files just to be safe
	flushRecording();
	flushJournal();
	m_blockCache.closeLastBlock();
}

void FiledHistory::setFounderName(const QString &founder)
{
	if(m_founder != founder) {
		m_founder = founder;
		writeStringToJournal(QStringLiteral("FOUNDER %1\n").arg(m_founder));
	}
}

void FiledHistory::setPasswordHash(const QByteArray &password)
{
	if(m_password != password) {
		m_password = password;
		writeBytesToJournal(
			QByteArrayLiteral("PASSWORD ") + m_password +
			QByteArrayLiteral("\n"));
	}
}

void FiledHistory::setOpwordHash(const QByteArray &opword)
{
	m_opword = opword;
	writeBytesToJournal(
		QByteArrayLiteral("OPWORD ") + m_opword + QByteArrayLiteral("\n"));
}

void FiledHistory::setMaxUsers(int max)
{
	const int newMax = qBound(1, max, 254);
	if(newMax != m_maxUsers) {
		m_maxUsers = newMax;
		writeStringToJournal(QStringLiteral("MAXUSERS %1\n").arg(newMax));
	}
}

void FiledHistory::setAutoResetThreshold(size_t limit)
{
	const size_t newLimit =
		sizeLimit() == 0 ? limit : qMin(size_t(sizeLimit() * 0.9), limit);
	if(newLimit != m_autoResetThreshold) {
		m_autoResetThreshold = newLimit;
		writeStringToJournal(QStringLiteral("AUTORESET %1\n").arg(newLimit));
	}
}

int FiledHistory::nextCatchupKey()
{
	int result = incrementNextCatchupKey(m_nextCatchupKey);
	writeStringToJournal(QStringLiteral("CATCHUP %1\n").arg(m_nextCatchupKey));
	return result;
}

bool FiledHistory::setTitle(const QString &title)
{
	if(title != m_title) {
		m_title = title;
		writeStringToJournal(QStringLiteral("TITLE %1\n").arg(title));
		return true;
	} else {
		return false;
	}
}

void FiledHistory::setFlags(Flags f)
{
	if(m_flags != f) {
		m_flags = f;
		QStringList fstr;
		if(f.testFlag(Persistent)) {
			fstr.append(QStringLiteral("persistent"));
		}
		if(f.testFlag(PreserveChat)) {
			fstr.append(QStringLiteral("preserveChat"));
		}
		if(f.testFlag(Nsfm)) {
			fstr.append(QStringLiteral("nsfm"));
		}
		if(f.testFlag(Deputies)) {
			fstr.append(QStringLiteral("deputies"));
		}
		if(f.testFlag(AuthOnly)) {
			fstr.append(QStringLiteral("authonly"));
		}
		if(f.testFlag(IdleOverride)) {
			fstr.append(QStringLiteral("idleoverride"));
		}
		if(f.testFlag(AllowWeb)) {
			fstr.append(QStringLiteral("allowweb"));
		}
		if(f.testFlag(AutoTitle)) {
			fstr.append(QStringLiteral("autotitle"));
		}
		if(f.testFlag(Invites)) {
			fstr.append(QStringLiteral("invites"));
		}
		writeStringToJournal(QStringLiteral("FLAGS %1\n").arg(fstr.join(' ')));
	}
}

void FiledHistory::joinUser(uint8_t id, const QString &name)
{
	SessionHistory::joinUser(id, name);
	writeBytesToJournal(
		QByteArrayLiteral("USER ") + QByteArray::number(int(id)) +
		QByteArrayLiteral(" ") +
		name.toUtf8().toPercentEncoding(QByteArray(), QByteArrayLiteral(" ")) +
		QByteArrayLiteral("\n"));
}

bool FiledHistory::isStreamResetIoAvailable() const
{
	return m_recording && m_resetStreamRecording && m_recording->atEnd() &&
		   m_resetStreamRecording->atEnd();
}

qint64 FiledHistory::resetStreamForkPos() const
{
	return m_resetStreamForkPos;
}

qint64 FiledHistory::resetStreamHeaderPos() const
{
	return m_resetStreamHeaderPos;
}

std::tuple<net::MessageList, long long>
FiledHistory::getBatch(long long after) const
{
	Block &b = m_blockCache.findBlock(after);
	long long idxOffset = qMax(0LL, after - b.startIndex + 1LL);
	if(idxOffset >= b.count) {
		return std::make_tuple(
			net::MessageList(), b.startIndex + b.count - 1LL);
	}

	if(b.messages.isEmpty() && b.count > 0) {
		// Load the block worth of messages to memory if not already loaded
		const qint64 prevPos = m_recording->pos();
		m_recording->seek(b.startOffset);
		for(int m = 0; m < b.count; ++m) {
			DP_Message *msg;
			DP_BinaryReaderResult result =
				DP_binary_reader_read_message(m_reader, false, &msg);
			if(result != DP_BINARY_READER_SUCCESS) {
				qWarning() << m_recording->fileName() << "read error!";
				m_recording->close();
				break;
			}
			b.messages.append(net::Message::noinc(msg));
		}

		m_recording->seek(prevPos);
	}
	Q_ASSERT(b.messages.size() == b.count);
	return std::make_tuple(
		b.messages.mid(idxOffset), b.startIndex + b.count - 1LL);
}

void FiledHistory::historyAdd(const net::Message &msg)
{
	size_t len = DP_binary_writer_write_message(m_writer, msg.get());
	m_blockCache.addToLastBlock(msg, len);
}

void FiledHistory::historyReset(const net::MessageList &newHistory)
{
	QFile *oldRecording = m_recording;
	oldRecording->close();

	m_recording = nullptr;
	DP_binary_reader_free(m_reader);
	m_reader = nullptr;
	DP_binary_writer_free(m_writer);
	m_writer = nullptr;
	m_blockCache.clear();
	initRecording();

	removeOrArchive(oldRecording);
	delete oldRecording;

	for(const net::Message &msg : newHistory) {
		historyAdd(msg);
	}
}

void FiledHistory::cleanupBatches(long long before)
{
	m_blockCache.cleanup(before);
}

void FiledHistory::historyAddBan(
	int id, const QString &username, const QHostAddress &ip,
	const QString &extAuthId, const QString &sid, const QString &bannedBy)
{
	const QByteArray space = QByteArrayLiteral(" ");
	writeBytesToJournal(
		QByteArrayLiteral("BAN ") + QByteArray::number(id) + space +
		username.toUtf8().toPercentEncoding(QByteArray(), space) + space +
		ip.toString().toUtf8() + space +
		extAuthId.toUtf8().toPercentEncoding(QByteArray(), space) + space +
		bannedBy.toUtf8().toPercentEncoding(QByteArray(), space) + space +
		sid.toUtf8().toPercentEncoding(QByteArray(), space) +
		QByteArrayLiteral("\n"));
}

void FiledHistory::historyRemoveBan(int id)
{
	writeBytesToJournal(
		QByteArrayLiteral("UNBAN ") + QByteArray::number(id) +
		QByteArrayLiteral("\n"));
}

StreamResetStartResult
FiledHistory::openResetStream(const net::MessageList &serverSideStateMessages)
{
	Q_ASSERT(!m_resetStreamRecording);
	Q_ASSERT(!m_resetStreamReader);
	Q_ASSERT(!m_resetStreamWriter);

	m_resetStreamFileCount = ++m_fileCount;
	m_resetStreamFileName =
		uniqueRecordingFilename(m_dir, id(), m_resetStreamFileCount);
	m_resetStreamForkPos = m_recording->pos();
	if(!openRecording(
		   m_resetStreamFileName, true, &m_resetStreamRecording,
		   &m_resetStreamReader, &m_resetStreamWriter)) {
		return StreamResetStartResult::WriteError;
	}

	m_resetStreamHeaderPos = m_resetStreamRecording->pos();
	m_blockCache.closeLastBlock();
	m_resetStreamBlockIndex = m_blockCache.size() - 1;
	m_resetStreamBlockCache.clear();
	m_resetStreamBlockCache.addBlock(m_resetStreamHeaderPos, 0LL);

	for(const net::Message &msg : serverSideStateMessages) {
		StreamResetAddResult addResult = addResetStreamMessage(msg);
		if(addResult != StreamResetAddResult::Ok) {
			discardResetStream();
			return addResult == StreamResetAddResult::OutOfSpace
					   ? StreamResetStartResult::OutOfSpace
					   : StreamResetStartResult::WriteError;
		}
	}

	return StreamResetStartResult::Ok;
}

StreamResetAddResult
FiledHistory::addResetStreamMessage(const net::Message &msg)
{
	Q_ASSERT(m_resetStreamWriter);
	size_t len = DP_binary_writer_write_message(m_resetStreamWriter, msg.get());
	if(len > 0) {
		m_resetStreamBlockCache.incrementLastBlock(len);
		return StreamResetAddResult::Ok;
	} else {
		return StreamResetAddResult::WriteError;
	}
}

StreamResetPrepareResult FiledHistory::prepareResetStream()
{
	Q_ASSERT(m_resetStreamRecording);
	if(m_resetStreamRecording->flush()) {
		return StreamResetPrepareResult::Ok;
	} else {
		qWarning(
			"Error flushing stream recording: %s",
			qUtf8Printable(m_resetStreamRecording->errorString()));
		return StreamResetPrepareResult::WriteError;
	}
}

bool FiledHistory::resolveResetStream(
	long long newFirstIndex, long long &outMessageCount, size_t &outSizeInBytes,
	QString &outError)
{
	Q_ASSERT(m_resetStreamRecording);

	qint64 prevPos = m_recording->pos();
	qint64 endPos = m_recording->size();
	qint64 streamPos = m_resetStreamRecording->pos();
	qint64 streamEnd = m_resetStreamRecording->size();
	if(prevPos != endPos || endPos < m_resetStreamForkPos ||
	   streamPos != streamEnd || streamPos < m_resetStreamHeaderPos) {
		outError =
			QStringLiteral("invalid stream offsets, recording pos %1 size %2 "
						   "fork %3, stream pos %4 size %5 fork %6")
				.arg(prevPos)
				.arg(endPos)
				.arg(m_resetStreamForkPos)
				.arg(streamPos)
				.arg(streamEnd)
				.arg(m_resetStreamHeaderPos);
		return false;
	}

	size_t sizeLimitInBytes = sizeLimit();
	if(sizeLimitInBytes != 0) {
		size_t sizeInBytes = (endPos - m_resetStreamForkPos) +
							 (streamPos - m_resetStreamHeaderPos);
		if(sizeInBytes > sizeLimitInBytes) {
			outError = QStringLiteral("total size %1 exceeds limit %2")
						   .arg(sizeInBytes)
						   .arg(sizeLimitInBytes);
			return false;
		}
	}

	if(!copyForkMessagesToResetStream(outError)) {
		if(!m_recording->seek(prevPos)) {
			qWarning(
				"Error seeking back recording: %s",
				qUtf8Printable(m_recording->errorString()));
		}
		discardResetStream();
		return false;
	}

	writeFileEntryToJournal(m_resetStreamFileName);

	DP_binary_reader_free(m_reader);
	m_reader = m_resetStreamReader;
	m_resetStreamReader = nullptr;

	DP_binary_writer_free(m_writer);
	m_writer = m_resetStreamWriter;
	m_resetStreamWriter = nullptr;

	removeOrArchive(m_recording);
	delete m_recording;
	m_recording = m_resetStreamRecording;
	m_resetStreamRecording = nullptr;

	m_blockCache.replaceWithResetStream(
		m_resetStreamBlockCache, m_resetStreamBlockIndex, newFirstIndex);

	outMessageCount = m_blockCache.totalMessageCount();
	outSizeInBytes = m_recording->pos() - m_resetStreamHeaderPos;
	return true;
}

bool FiledHistory::copyForkMessagesToResetStream(QString &outError)
{
	if(!m_recording->seek(m_resetStreamForkPos)) {
		outError = QStringLiteral("Error seeking recording to stream start: %1")
					   .arg(m_recording->errorString());
		return false;
	}

	QByteArray buffer;
	buffer.resize(BUFSIZ);
	while(true) {
		qint64 read = m_recording->read(buffer.data(), buffer.size());
		if(read > 0) {
			qint64 written =
				m_resetStreamRecording->write(buffer.constData(), read);
			if(written < 0) {
				outError =
					QStringLiteral("Error writing to stream recording: %1")
						.arg(m_resetStreamRecording->errorString());
				return false;
			} else if(written != read) {
				outError =
					QStringLiteral("Tried to write %1 byte(s), but wrote %2")
						.arg(read)
						.arg(written);
				return false;
			}
		} else if(read == 0) {
			break;
		} else {
			outError = QStringLiteral("Error reading from recording: %1")
						   .arg(m_recording->errorString());
			return false;
		}
	}

	if(!m_resetStreamRecording->flush()) {
		outError = QStringLiteral("Error flushing stream recording: %1")
					   .arg(m_resetStreamRecording->errorString());
		return false;
	}

	return true;
}

void FiledHistory::discardResetStream()
{
	if(m_resetStreamFileCount == m_fileCount) {
		--m_fileCount;
	}
	m_resetStreamFileCount = -1;
	m_resetStreamBlockCache.clear();

	if(m_resetStreamRecording) {
		DP_ASSERT(m_resetStreamWriter);
		DP_binary_writer_free(m_resetStreamWriter);
		m_resetStreamWriter = nullptr;

		DP_ASSERT(m_resetStreamReader);
		DP_binary_reader_free(m_resetStreamReader);
		m_resetStreamReader = nullptr;

		m_resetStreamRecording->remove();
		delete m_resetStreamRecording;
		m_resetStreamRecording = nullptr;
	} else {
		DP_ASSERT(!m_resetStreamWriter);
		DP_ASSERT(!m_resetStreamReader);
	}
}

void FiledHistory::timerEvent(QTimerEvent *)
{
	flushRecording();
}

void FiledHistory::addAnnouncement(const QString &url)
{
	if(!m_announcements.contains(url)) {
		m_announcements << url;
		writeStringToJournal(QStringLiteral("ANNOUNCE %1\n").arg(url));
	}
}

void FiledHistory::removeAnnouncement(const QString &url)
{
	if(m_announcements.contains(url)) {
		m_announcements.removeAll(url);
		writeStringToJournal(QStringLiteral("UNANNOUNCE %1\n").arg(url));
	}
}

void FiledHistory::setAuthenticatedOperator(const QString &authId, bool op)
{
	bool currentlyOp = isOperator(authId);
	if(op && !currentlyOp) {
		writeStringToJournal(QStringLiteral("OP %1\n").arg(authId));
	} else if(!op && currentlyOp) {
		writeStringToJournal(QStringLiteral("DEOP %1\n").arg(authId));
	}
	SessionHistory::setAuthenticatedOperator(authId, op);
}

void FiledHistory::setAuthenticatedTrust(const QString &authId, bool trusted)
{
	bool currentlyTrusted = isTrusted(authId);
	if(trusted && !currentlyTrusted) {
		writeStringToJournal(QStringLiteral("TRUST %1\n").arg(authId));
	} else if(!trusted && currentlyTrusted) {
		writeStringToJournal(QStringLiteral("UNTRUST %1\n").arg(authId));
	}
	SessionHistory::setAuthenticatedTrust(authId, trusted);
}

void FiledHistory::setAuthenticatedUsername(
	const QString &authId, const QString &username)
{
	const QString *currentUsername = authenticatedUsernameFor(authId);
	if(!currentUsername || *currentUsername != username) {
		writeBytesToJournal(
			QByteArrayLiteral("AUTHNAME ") +
			authId.toUtf8().toPercentEncoding() + QByteArrayLiteral(" ") +
			username.toUtf8().toPercentEncoding() + QByteArrayLiteral("\n"));
	}
	SessionHistory::setAuthenticatedUsername(authId, username);
}

Invite *FiledHistory::createInvite(
	const QString &createdBy, int maxUses, bool trust, bool op)
{
	Invite *invite =
		SessionHistory::createInvite(createdBy, maxUses, trust, op);
	if(invite) {
		QByteArray bytes = QByteArrayLiteral("INV ") + invite->secret.toUtf8() +
						   QByteArrayLiteral(" ") + invite->at.toUtf8() +
						   QByteArrayLiteral(" ") +
						   QByteArray::number(invite->maxUses);
		if(invite->trust) {
			bytes.append(QByteArrayLiteral(",trust"));
		}
		if(invite->op) {
			bytes.append(QByteArrayLiteral(",op"));
		}
		if(!invite->creator.isEmpty()) {
			bytes.append(QByteArrayLiteral(" "));
			bytes.append(invite->creator.toUtf8().toPercentEncoding());
		}
		bytes.append(QByteArrayLiteral("\n"));
		writeBytesToJournal(bytes);
	}
	return invite;
}

bool FiledHistory::removeInvite(const QString &secret)
{
	bool removed = SessionHistory::removeInvite(secret);
	if(removed) {
		writeBytesToJournal(
			QByteArrayLiteral("UNV ") + secret.toUtf8() +
			QByteArrayLiteral("\n"));
	}
	return removed;
}

CheckInviteResult FiledHistory::checkInvite(
	Client *client, const QString &secret, bool use, QString *outClientKey,
	Invite **outInvite, InviteUse **outInviteUse)
{
	QString clientKey;
	InviteUse *inviteUse = nullptr;
	CheckInviteResult result = SessionHistory::checkInvite(
		client, secret, use, &clientKey, outInvite, &inviteUse);

	if(outClientKey) {
		*outClientKey = clientKey;
	}
	if(outInviteUse) {
		*outInviteUse = inviteUse;
	}

	if(result == CheckInviteResult::InviteUsed ||
	   result == CheckInviteResult::AlreadyInvitedNameChanged) {
		QString name = inviteUse ? inviteUse->name : QString();
		writeBytesToJournal(
			QByteArrayLiteral("USV ") + secret.toUtf8() +
			QByteArrayLiteral(" ") + clientKey.toUtf8() +
			QByteArrayLiteral(" ") +
			(name.isEmpty() ? QByteArrayLiteral("?")
							: name.toUtf8().toPercentEncoding()) +
			QByteArrayLiteral("\n"));
	}

	return result;
}


FiledHistory::Block &FiledHistory::BlockCache::findBlock(long long after)
{
	int i = m_blocks.size() - 1;
	for(; i > 0; --i) {
		const Block &b = m_blocks.at(i - 1);
		if(b.startIndex + b.count - 1LL <= after) {
			break;
		}
	}
	return m_blocks[i];
}

void FiledHistory::BlockCache::addBlock(qint64 offset, long long index)
{
	m_blocks.append(Block(offset, index));
}

void FiledHistory::BlockCache::addToLastBlock(
	const net::Message &msg, size_t len)
{
	Block &b = m_blocks.last();
	// Add message to cache, if already active (if cache is empty, it will be
	// loaded from disk when needed)
	if(!b.messages.isEmpty()) {
		b.messages.append(msg);
	}
	incrementBlock(b, len);
}

void FiledHistory::BlockCache::incrementLastBlock(size_t len)
{
	Block &b = m_blocks.last();
	Q_ASSERT(b.messages.isEmpty());
	incrementBlock(b, len);
}

void FiledHistory::BlockCache::closeLastBlock()
{
	// Check if anything needs to be done
	Block &b = m_blocks.last();
	if(b.count != 0) {
		// Mark last block as closed and start a new one
		m_blocks.append(Block(b.endOffset, b.startIndex + b.count));
	}
}

void FiledHistory::BlockCache::cleanup(long long before)
{
	for(Block &b : m_blocks) {
		if(b.startIndex + b.count >= before) {
			break;
		} else if(!b.messages.isEmpty()) {
			qDebug(
				"Releasing history block cache from %lld to %lld", b.startIndex,
				b.startIndex + b.count - 1LL);
			b.messages = net::MessageList();
		}
	}
}

long long FiledHistory::BlockCache::totalMessageCount() const
{
	compat::sizetype count = m_blocks.size();
	if(count > 0) {
		const Block &b = m_blocks[count - 1];
		return b.startIndex + b.count - m_blocks[0].startIndex;
	} else {
		return 0LL;
	}
}

void FiledHistory::BlockCache::replaceWithResetStream(
	BlockCache &streamCache, compat::sizetype blockIndex,
	long long newFirstIndex)
{
	// Reindex blocks in the stream reset image based on the new first index.
	long long nextStartIndex = newFirstIndex;
	for(Block &b : streamCache.m_blocks) {
		b.startIndex = nextStartIndex;
		nextStartIndex = b.startIndex + b.count;
	}

	// Move forked blocks over to the new stream cache.
	compat::sizetype count = m_blocks.size();
	for(compat::sizetype i = blockIndex; i < count; ++i) {
		Block b = m_blocks[i];
		b.startIndex = nextStartIndex;
		nextStartIndex = b.startIndex + b.count;
		qint64 offsetSize = b.endOffset - b.startOffset;
		b.startOffset = streamCache.m_blocks.last().endOffset;
		b.endOffset = b.startOffset + offsetSize;
		streamCache.m_blocks.append(b);
	}

	// Replace ourselves.
	m_blocks.clear();
	m_blocks.swap(streamCache.m_blocks);
}

void FiledHistory::BlockCache::incrementBlock(Block &b, size_t len)
{
	++b.count;
	b.endOffset += len;
	if(b.endOffset - b.startOffset > MAX_BLOCK_SIZE) {
		m_blocks.append(Block(b.endOffset, b.startIndex + b.count));
	}
}

}
