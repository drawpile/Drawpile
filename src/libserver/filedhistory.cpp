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

	if(!m_alias.isEmpty())
		m_journal->write(QString("ALIAS %1\n").arg(m_alias).toUtf8());
	m_journal->write(QString("FOUNDER %1\n").arg(m_founder).toUtf8());
	m_journal->flush();

	return true;
}

bool FiledHistory::initRecording()
{
	Q_ASSERT(m_blocks.isEmpty());

	const QString filename =
		uniqueRecordingFilename(m_dir, id(), ++m_fileCount);

	m_recording = new QFile(m_dir.absoluteFilePath(filename), this);
	if(!m_recording->open(QFile::ReadWrite)) {
		qWarning() << filename << m_recording->errorString();
		return false;
	}

	Q_ASSERT(!m_reader);
	m_reader = DP_binary_reader_new(
		DP_qfile_input_new(m_recording, false, DP_input_new),
		DP_BINARY_READER_FLAG_NO_LENGTH | DP_BINARY_READER_FLAG_NO_HEADER);
	Q_ASSERT(!m_writer);
	m_writer = DP_binary_writer_new(
		DP_qfile_output_new(m_recording, false, DP_output_new));

	JSON_Value *header_value = json_value_init_object();
	JSON_Object *header_object = json_value_get_object(header_value);
	json_object_set_string( // the hosting client's protocol version
		header_object, "version", qUtf8Printable(m_version.asString()));
	bool ok = DP_binary_writer_write_header(m_writer, header_object);
	json_value_free(header_value);
	if(!ok) {
		qWarning() << filename << DP_error();
		return false;
	}

	m_recording->flush();

	m_journal->write(QString("FILE %1\n").arg(filename).toUtf8());
	m_journal->flush();

	m_blocks << Block{
		m_recording->pos(), firstIndex(), 0, m_recording->pos(),
		net::MessageList()};

	return true;
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
		int sep = line.indexOf(' ');
		if(sep < 0) {
			cmd = line;
		} else {
			cmd = line.left(sep).trimmed();
			params = line.mid(sep + 1).trimmed();
		}

		if(cmd == "FILE") {
			recordingFile = QString::fromUtf8(params);
			++m_fileCount;
			m_blocks.clear();

		} else if(cmd == "ALIAS") {
			if(m_alias.isEmpty())
				m_alias = QString::fromUtf8(params);
			else
				qWarning() << id() << "alias set twice.";

		} else if(cmd == "FOUNDER") {
			m_founder = QString::fromUtf8(params);

		} else if(cmd == "PASSWORD") {
			if(params.isEmpty() || passwordhash::isValidHash(params))
				m_password = params;

		} else if(cmd == "OPWORD") {
			if(params.isEmpty() || passwordhash::isValidHash(params))
				m_opword = params;

		} else if(cmd == "MAXUSERS") {
			m_maxUsers = qBound(1, params.toInt(), 254);

		} else if(cmd == "AUTORESET") {
			m_autoResetThreshold = params.toUInt();

		} else if(cmd == "TITLE") {
			m_title = params;

		} else if(cmd == "FLAGS") {
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
				} else {
					qWarning()
						<< id() << "unknown flag:" << QString::fromUtf8(f);
				}
			}
			m_flags = flags;

		} else if(cmd == "BAN") {
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

		} else if(cmd == "UNBAN") {
			m_banlist.removeBan(params.toInt());

		} else if(cmd == "ANNOUNCE") {
			if(!m_announcements.contains(params))
				m_announcements << params;

		} else if(cmd == "UNANNOUNCE") {
			m_announcements.removeAll(params);

		} else if(cmd == "USER") {
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

		} else if(cmd == "OP") {
			QString authId = QString::fromUtf8(params);
			if(!authId.isEmpty()) {
				SessionHistory::setAuthenticatedOperator(authId, true);
			}

		} else if(cmd == "DEOP") {
			QString authId = QString::fromUtf8(params);
			SessionHistory::setAuthenticatedOperator(authId, false);

		} else if(cmd == "TRUST") {
			const QString authId = QString::fromUtf8(params);
			if(!authId.isEmpty()) {
				SessionHistory::setAuthenticatedTrust(authId, true);
			}

		} else if(cmd == "UNTRUST") {
			QString authId = QString::fromUtf8(params);
			SessionHistory::setAuthenticatedTrust(authId, false);

		} else if(cmd == "AUTHNAME") {
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

		} else if(cmd == "CATCHUP") {
			m_nextCatchupKey =
				qBound(MIN_CATCHUP_KEY, params.toInt(), MAX_CATCHUP_KEY);

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

	historyLoaded(
		m_blocks.last().endOffset - startOffset,
		m_blocks.last().startIndex + m_blocks.last().count);

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
	Q_ASSERT(m_blocks.isEmpty());
	// Note: m_recording should be at the start of the recording

	m_blocks << Block{
		m_recording->pos(), firstIndex(), 0, m_recording->pos(),
		net::MessageList()};

	QSet<uint8_t> users;

	do {
		Block &b = m_blocks.last();
		uint8_t msgType, ctxId;
		int msglen = DP_binary_reader_skip_message(m_reader, &msgType, &ctxId);
		if(msglen < 0) {
			// Truncated message encountered.
			// Rewind back to the end of the previous message
			qWarning() << m_recording->fileName() << "Recording truncated at"
					   << int(b.endOffset);
			m_recording->seek(b.endOffset);
			break;
		}
		++m_blocks.last().count;

		b.endOffset += msglen;
		Q_ASSERT(b.endOffset == m_recording->pos());

		if(b.endOffset - b.startOffset >= MAX_BLOCK_SIZE) {
			m_blocks << Block{
				b.endOffset, b.startIndex + b.count, 0, b.endOffset,
				net::MessageList()};
		}

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
		m_blocks.last().count++;
		m_blocks.last().endOffset += qint64(msg.length());
		if(DP_binary_writer_write_message(m_writer, msg.get()) == 0) {
			return false;
		}
		idQueue().reserveId(user);
	}
	return true;
}

void FiledHistory::terminate()
{
	DP_binary_reader_free(m_reader);
	m_reader = nullptr;
	DP_binary_writer_free(m_writer);
	m_writer = nullptr;
	m_recording->close();
	m_journal->close();

	if(m_archive) {
		m_journal->rename(m_journal->fileName() + ".archived");
		m_recording->rename(m_recording->fileName() + ".archived");
	} else {
		m_recording->remove();
		m_journal->remove();
	}
}

void FiledHistory::closeBlock()
{
	// Flush the output files just to be safe
	m_recording->flush();
	m_journal->flush();

	// Check if anything needs to be done
	Block &b = m_blocks.last();
	if(b.count == 0)
		return;

	// Mark last block as closed and start a new one
	m_blocks << Block{
		b.endOffset, b.startIndex + b.count, 0, b.endOffset,
		net::MessageList()};
}

void FiledHistory::setFounderName(const QString &founder)
{
	if(m_founder != founder) {
		m_founder = founder;
		m_journal->write(QString("FOUNDER %1\n").arg(m_founder).toUtf8());
		m_journal->flush();
	}
}

void FiledHistory::setPasswordHash(const QByteArray &password)
{
	if(m_password != password) {
		m_password = password;

		m_journal->write("PASSWORD ");
		if(!m_password.isEmpty())
			m_journal->write(m_password);
		m_journal->write("\n");
		m_journal->flush();
	}
}

void FiledHistory::setOpwordHash(const QByteArray &opword)
{
	m_opword = opword;

	m_journal->write("OPWORD ");
	if(!m_opword.isEmpty())
		m_journal->write(m_opword);
	m_journal->write("\n");
	m_journal->flush();
}

void FiledHistory::setMaxUsers(int max)
{
	const int newMax = qBound(1, max, 254);
	if(newMax != m_maxUsers) {
		m_maxUsers = newMax;
		m_journal->write(QString("MAXUSERS %1\n").arg(newMax).toUtf8());
		m_journal->flush();
	}
}

void FiledHistory::setAutoResetThreshold(uint limit)
{
	const uint newLimit =
		sizeLimit() == 0 ? limit : qMin(uint(sizeLimit() * 0.9), limit);
	if(newLimit != m_autoResetThreshold) {
		m_autoResetThreshold = newLimit;
		m_journal->write(QString("AUTORESET %1\n").arg(newLimit).toUtf8());
		m_journal->flush();
	}
}

int FiledHistory::nextCatchupKey()
{
	int result = incrementNextCatchupKey(m_nextCatchupKey);
	m_journal->write(QString("CATCHUP %1\n").arg(m_nextCatchupKey).toUtf8());
	m_journal->flush();
	return result;
}

void FiledHistory::setTitle(const QString &title)
{
	if(title != m_title) {
		m_title = title;
		m_journal->write(QString("TITLE %1\n").arg(title).toUtf8());
		m_journal->flush();
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
		m_journal->write(
			QStringLiteral("FLAGS %1\n").arg(fstr.join(' ')).toUtf8());
		m_journal->flush();
	}
}

void FiledHistory::joinUser(uint8_t id, const QString &name)
{
	SessionHistory::joinUser(id, name);
	m_journal->write(
		"USER " + QByteArray::number(int(id)) + " " +
		name.toUtf8().toPercentEncoding(QByteArray(), " ") + "\n");
	m_journal->flush();
}

std::tuple<net::MessageList, int> FiledHistory::getBatch(int after) const
{
	// Find the block that contains the index *after*
	int i = m_blocks.size() - 1;
	for(; i > 0; --i) {
		const Block &b = m_blocks.at(i - 1);
		if(b.startIndex + b.count - 1 <= after)
			break;
	}

	Block &b = m_blocks[i];
	int idxOffset = qMax(0, after - b.startIndex + 1);
	if(idxOffset >= b.count) {
		return std::make_tuple(net::MessageList(), b.startIndex + b.count - 1);
	}

	if(b.messages.isEmpty() && b.count > 0) {
		// Load the block worth of messages to memory if not already loaded
		const qint64 prevPos = m_recording->pos();
		qDebug() << m_recording->fileName() << "loading block" << i;
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
		b.messages.mid(idxOffset), b.startIndex + b.count - 1);
}

void FiledHistory::historyAdd(const net::Message &msg)
{
	size_t len = DP_binary_writer_write_message(m_writer, msg.get());

	Block &b = m_blocks.last();
	b.count++;
	b.endOffset += len;

	// Add message to cache, if already active (if cache is empty, it will be
	// loaded from disk when needed)
	if(!b.messages.isEmpty())
		b.messages.append(msg);

	if(b.endOffset - b.startOffset > MAX_BLOCK_SIZE)
		closeBlock();
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
	m_blocks.clear();
	initRecording();

	// Remove old recording after the new one has been created so
	// that the new file will not have the same name.
	if(m_archive)
		oldRecording->rename(oldRecording->fileName() + ".archived");
	else
		oldRecording->remove();
	delete oldRecording;

	for(const net::Message &msg : newHistory) {
		historyAdd(msg);
	}
}

void FiledHistory::cleanupBatches(int before)
{
	for(Block &b : m_blocks) {
		if(b.startIndex + b.count >= before)
			break;
		if(!b.messages.isEmpty()) {
			qDebug() << "releasing history block cache from" << b.startIndex
					 << "to" << b.startIndex + b.count - 1;
			b.messages = net::MessageList();
		}
	}
}

void FiledHistory::historyAddBan(
	int id, const QString &username, const QHostAddress &ip,
	const QString &extAuthId, const QString &sid, const QString &bannedBy)
{
	const QByteArray include = " ";
	QByteArray entry =
		"BAN " + QByteArray::number(id) + " " +
		username.toUtf8().toPercentEncoding(QByteArray(), include) + " " +
		ip.toString().toUtf8() + " " +
		extAuthId.toUtf8().toPercentEncoding(QByteArray(), include) + " " +
		bannedBy.toUtf8().toPercentEncoding(QByteArray(), include) + " " +
		sid.toUtf8().toPercentEncoding(QByteArray(), include) + "\n";
	m_journal->write(entry);
	m_journal->flush();
}

void FiledHistory::historyRemoveBan(int id)
{
	m_journal->write(QByteArray("UNBAN ") + QByteArray::number(id) + "\n");
	m_journal->flush();
}

void FiledHistory::timerEvent(QTimerEvent *)
{
	if(m_recording)
		m_recording->flush();
}

void FiledHistory::addAnnouncement(const QString &url)
{
	if(!m_announcements.contains(url)) {
		m_announcements << url;
		m_journal->write(QString("ANNOUNCE %1\n").arg(url).toUtf8());
		m_journal->flush();
	}
}

void FiledHistory::removeAnnouncement(const QString &url)
{
	if(m_announcements.contains(url)) {
		m_announcements.removeAll(url);
		m_journal->write(QString("UNANNOUNCE %1\n").arg(url).toUtf8());
		m_journal->flush();
	}
}

void FiledHistory::setAuthenticatedOperator(const QString &authId, bool op)
{
	bool currentlyOp = isOperator(authId);
	if(op && !currentlyOp) {
		m_journal->write("OP " + authId.toUtf8() + "\n");
		m_journal->flush();
	} else if(!op && currentlyOp) {
		m_journal->write("DEOP " + authId.toUtf8() + "\n");
		m_journal->flush();
	}
	SessionHistory::setAuthenticatedOperator(authId, op);
}

void FiledHistory::setAuthenticatedTrust(const QString &authId, bool trusted)
{
	bool currentlyTrusted = isTrusted(authId);
	if(trusted && !currentlyTrusted) {
		m_journal->write("TRUST " + authId.toUtf8() + "\n");
		m_journal->flush();
	} else if(!trusted && currentlyTrusted) {
		m_journal->write("UNTRUST " + authId.toUtf8() + "\n");
		m_journal->flush();
	}
	SessionHistory::setAuthenticatedTrust(authId, trusted);
}

void FiledHistory::setAuthenticatedUsername(
	const QString &authId, const QString &username)
{
	const QString *currentUsername = authenticatedUsernameFor(authId);
	if(!currentUsername || *currentUsername != username) {
		m_journal->write(
			"AUTHNAME " + authId.toUtf8().toPercentEncoding() + " " +
			username.toUtf8().toPercentEncoding() + "\n");
		m_journal->flush();
	}
	SessionHistory::setAuthenticatedUsername(authId, username);
}

}
