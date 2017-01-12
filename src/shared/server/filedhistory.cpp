/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#include "filedhistory.h"
#include "../shared/util/passwordhash.h"
#include "../shared/util/filename.h"
#include "../shared/util/logger.h"
#include "../shared/record/header.h"

#include <QFile>
#include <QJsonObject>
#include <QVarLengthArray>

namespace server {

// A block is closed when its size goes above this limit
static const qint64 MAX_BLOCK_SIZE = 0xffff * 10;

FiledHistory::FiledHistory(const QDir &dir, QFile *journal, const QUuid &id, const QString &alias, const protocol::ProtocolVersion &version, const QString &founder, QObject *parent)
	: SessionHistory(id, parent),
	  m_dir(dir),
	  m_journal(journal),
	  m_recording(nullptr),
	  m_alias(alias),
	  m_founder(founder),
	  m_version(version),
	  m_maxUsers(254),
	  m_flags(0)
{
	Q_ASSERT(journal);
}

FiledHistory::FiledHistory(const QDir &dir, QFile *journal, const QUuid &id, QObject *parent)
	: SessionHistory(id, parent),
	  m_dir(dir),
	  m_journal(journal),
	  m_recording(nullptr),
	  m_maxUsers(0),
	  m_flags(0)
{
	Q_ASSERT(journal);
}

FiledHistory::~FiledHistory()
{
}

QString FiledHistory::journalFilename(const QUuid &id)
{
	QString journalFilename = id.toString();
	journalFilename = journalFilename.mid(1, journalFilename.length()-2) + ".session";
	return journalFilename;
}

static QString uniqueRecordingFilename(const QDir &dir, const QUuid &id)
{
	QString idstr = id.toString();
	idstr = idstr.mid(1, idstr.length()-2);

	return utils::uniqueFilename(dir, idstr, "dprec", false);
}

FiledHistory *FiledHistory::startNew(const QDir &dir, const QUuid &id, const QString &alias, const protocol::ProtocolVersion &version, const QString &founder, QObject *parent)
{
	QFile *journal = new QFile(QFileInfo(dir, journalFilename(id)).absoluteFilePath());

	FiledHistory *fh = new FiledHistory(dir, journal, id, alias, version, founder, parent);
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
	const QUuid id = filename;
	if(id.isNull()) {
		logger::error() << "History file name not a valid ID";
		return nullptr;
	}

	QFile *journal = new QFile(path);
	if(!journal->open(QFile::ReadWrite)) {
		logger::error() << path << journal->errorString();
		delete journal;
		return nullptr;
	}

	FiledHistory *fh = new FiledHistory(dir, journal, id, parent);
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
		logger::error() << m_journal->fileName() << "already exists!";
		return false;
	}

	if(!m_journal->open(QFile::WriteOnly)) {
		logger::error() << m_journal->fileName() << m_journal->errorString();
		return false;
	}

	if(!initRecording())
		return false;

	if(!m_alias.isEmpty())
		m_journal->write(QString("ALIAS %1\n").arg(m_alias).toUtf8());
	m_journal->write(QString("FOUNDER %1\n").arg(m_founder).toUtf8());

	return true;
}

bool FiledHistory::initRecording()
{
	Q_ASSERT(m_blocks.isEmpty());

	QString filename = uniqueRecordingFilename(m_dir, id());

	m_recording = new QFile(m_dir.absoluteFilePath(filename), this);
	if(!m_recording->open(QFile::ReadWrite)) {
		logger::error() << filename << m_recording->errorString();
		return false;
	}

	QJsonObject metadata;
	metadata["version"] = m_version.asString(); // the hosting client's protocol version
	recording::writeRecordingHeader(m_recording, metadata);

	m_recording->flush();

	m_journal->write(QString("FILE %1\n").arg(filename).toUtf8());
	m_journal->flush();

	m_blocks << Block {
		m_recording->pos(),
		firstIndex(),
		0,
		m_recording->pos(),
		QList<protocol::MessagePtr>()
		};

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

		//qDebug() << QString::fromUtf8(line.trimmed());

		QByteArray params;
		int sep = line.indexOf(' ');
		if(sep<0) {
			cmd = line;
		} else {
			cmd = line.left(sep).trimmed();
			params = line.mid(sep+1).trimmed();
		}

		if(cmd == "FILE") {
			recordingFile = QString::fromUtf8(params);
			m_blocks.clear();

		} else if(cmd == "ALIAS") {
			if(m_alias.isEmpty())
				m_alias = QString::fromUtf8(params);
			else
				logger::warning() << id().toString() << "alias set twice.";

		} else if(cmd == "FOUNDER") {
			if(m_founder.isEmpty())
				m_founder = QString::fromUtf8(params);
			else
				logger::warning() << id().toString() << "founder set twice.";

		} else if(cmd == "PASSWORD") {
			m_password = params;

		} else if(cmd == "MAXUSERS") {
			m_maxUsers = qBound(1, params.toInt(), 254);

		} else if(cmd == "TITLE") {
			m_title = params;

		} else if(cmd == "FLAGS") {
			Flags flags;
			for(const QByteArray &f : params.split(' ')) {
				if(f == "persistent")
					flags.setFlag(Persistent);
				else if(f == "preserveChat")
					flags.setFlag(PreserveChat);
				else if(f == "nsfm")
					flags.setFlag(Nsfm);
				else
					logger::warning() << id().toString() << "unknown flag:" << QString::fromUtf8(f);
			}
			m_flags = flags;

		} else {
			logger::warning() << id().toString() << "unknown journal entry:" << QString::fromUtf8(cmd);
		}
	} while(!m_journal->atEnd());

	// The latest recording file must exist
	if(recordingFile.isEmpty()) {
		logger::error() << id().toString() << "content file not set!";
		return false;
	}

	m_recording = new QFile(m_dir.absoluteFilePath(recordingFile), this);

	if(!m_recording->exists()) {
		logger::error() << recordingFile << "not found!";
		return false;
	}

	if(!m_recording->open(QFile::ReadWrite)) {
		logger::error() << recordingFile << m_recording->errorString();
		return false;
	}

	// Recording must have a valid header
	QJsonObject header = recording::readRecordingHeader(m_recording);
	if(header.isEmpty()) {
		logger::error() << recordingFile << "invalid header";
		return false;
	}

	m_version = protocol::ProtocolVersion::fromString(header["version"].toString());
	if(!m_version.isValid()) {
		logger::error() << recordingFile << "invalid protocol version";
		return false;
	}

	if(m_version.server() != protocol::ProtocolVersion::current().server()) {
		logger::error() << recordingFile << "incompatible server version";
		return false;
	}

	qint64 startOffset = m_recording->pos();

	// Scan the recording file and build the index of blocks
	if(!scanBlocks()) {
		logger::error() << recordingFile << "error occurred during indexing";
		return false;
	}

	historyLoaded(m_blocks.last().endOffset-startOffset, m_blocks.last().startIndex+m_blocks.last().count);

	return true;
}

bool FiledHistory::scanBlocks()
{
	Q_ASSERT(m_blocks.isEmpty());
	// Note: m_recording should be at the start of the recording

	m_blocks << Block {
		m_recording->pos(),
		firstIndex(),
		0,
		m_recording->pos(),
		QList<protocol::MessagePtr>()
	};

	do {
		Block &b = m_blocks.last();

		const int msglen = recording::skipRecordingMessage(m_recording);
		if(msglen<0) {
			// Truncated message encountered.
			// Rewind back to the end of the previous message
			logger::warning() << m_recording->fileName() << "Recording truncated at" << int(b.endOffset);
			m_recording->seek(b.endOffset);
			break;
		}
		++m_blocks.last().count;

		b.endOffset += msglen;
		Q_ASSERT(b.endOffset == m_recording->pos());

		if(b.endOffset-b.startOffset >= MAX_BLOCK_SIZE) {
			m_blocks << Block {
				b.endOffset,
				b.startIndex+b.count,
				0,
				b.endOffset,
				QList<protocol::MessagePtr>()
			};
		}
	} while(!m_recording->atEnd());

	return true;
}

void FiledHistory::closeBlock()
{
	// Flush the output files just to be safe
	m_recording->flush();
	m_journal->flush();

	// Check if anything needs to be done
	Block &b = m_blocks.last();
	if(b.count==0)
		return;

	// Mark last block as closed and start a new one
	m_blocks << Block {
				b.endOffset,
				b.startIndex+b.count,
				0,
				b.endOffset,
				QList<protocol::MessagePtr>()
	};
}

void FiledHistory::setPassword(const QString &password)
{
	m_password = passwordhash::hash(password);

	m_journal->write("PASSWORD ");
	if(!m_password.isEmpty())
		m_journal->write(m_password);
	m_journal->write("\n");
}

QDateTime FiledHistory::startTime() const
{
	return QFileInfo(*m_journal).created();
}

void FiledHistory::setMaxUsers(int max)
{
	int newMax = qBound(1, max, 254);
	if(newMax != m_maxUsers) {
		m_maxUsers = newMax;
		m_journal->write(QString("MAXUSERS %1\n").arg(newMax).toUtf8());
	}
}

void FiledHistory::setTitle(const QString &title)
{
	if(title != m_title) {
		m_title = title;
		m_journal->write(QString("TITLE %1\n").arg(title).toUtf8());
	}
}

void FiledHistory::setFlags(Flags f)
{
	if(m_flags != f) {
		m_flags = f;
		QStringList fstr;
		if(f.testFlag(Persistent))
			fstr << "persistent";
		if(f.testFlag(PreserveChat))
			fstr << "preserveChat";
		if(f.testFlag(Nsfm))
			fstr << "nsfm";
		m_journal->write(QString("FLAGS %1\n").arg(fstr.join(' ')).toUtf8());
	}
}

std::tuple<QList<protocol::MessagePtr>, int> FiledHistory::getBatch(int after) const
{
	// Find the block that contains the index *after*
	int i=m_blocks.size()-1;
	for(;i>0;--i) {
		const Block &b = m_blocks.at(i-1);
		if(b.startIndex+b.count-1 <= after)
			break;
	}

	const Block &b = m_blocks.at(i);

	const int idxOffset = qMax(0, after - b.startIndex + 1);
	if(idxOffset >= b.count)
		return std::make_tuple(QList<protocol::MessagePtr>(), b.startIndex+b.count-1);

	if(b.messages.isEmpty() && b.count>0 && idxOffset < b.count) {
		// Load the block worth of messages to memory if not already loaded
		const qint64 prevPos = m_recording->pos();
		logger::debug() << m_recording->fileName() << "loading block" << i;
		m_recording->seek(b.startOffset);
		QByteArray buffer;
		for(int m=0;m<b.count;++m) {
			if(!recording::readRecordingMessage(m_recording, buffer)) {
				logger::error() << m_recording->fileName() << "read error!";
				m_recording->close();
				break;
			}
			protocol::Message *msg = protocol::Message::deserialize((const uchar*)buffer.constData(), buffer.length(), false);
			if(!msg) {
				logger::error() << m_recording->fileName() << "Invalid message in block" << i;
				m_recording->close();
				break;
			}
			const_cast<Block&>(b).messages << protocol::MessagePtr(msg);
		}

		m_recording->seek(prevPos);
	}
	return std::make_tuple(b.messages.mid(idxOffset), b.startIndex+b.count-1);
}

void FiledHistory::historyAdd(const protocol::MessagePtr &msg)
{
	QVarLengthArray<char> buf(msg->length());
	const int len = msg->serialize(buf.data());
	Q_ASSERT(len == buf.length());
	m_recording->write(buf.data(), len);

	Block &b = m_blocks.last();
	b.count++;
	b.endOffset += len;
	if(b.endOffset-b.startOffset > MAX_BLOCK_SIZE)
		closeBlock();
}

void FiledHistory::historyReset(const QList<protocol::MessagePtr> &newHistory)
{
	m_recording->close();
	delete m_recording;
	m_blocks.clear();
	initRecording();

	for(const protocol::MessagePtr &msg : newHistory)
		historyAdd(msg);
}

void FiledHistory::cleanupBatches(int before)
{
	for(Block &b : m_blocks) {
		if(b.startIndex+b.count >= before)
			break;
		if(!b.messages.isEmpty())
			b.messages = QList<protocol::MessagePtr>();
	}
}

}
