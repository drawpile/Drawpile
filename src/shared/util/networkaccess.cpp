/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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

#include "networkaccess.h"

#include <QNetworkReply>
#include <QMutexLocker>
#include <QHash>
#include <QThread>
#include <QDebug>
#include <QSaveFile>
#include <QTemporaryFile>

namespace networkaccess {

struct Managers {
	QMutex mutex;
	QHash<void*, QNetworkAccessManager*> perThreadManagers;
};

static Managers MANAGERS;

QNetworkAccessManager *getInstance()
{
	QMutexLocker lock(&MANAGERS.mutex);

	QThread *t = QThread::currentThread();

	QNetworkAccessManager *nam = MANAGERS.perThreadManagers[t];
	if(!nam) {
		qDebug() << "Creating new NetworkAccessManager for thread" << t;
		nam = new QNetworkAccessManager;
		t->connect(t, &QThread::finished, [nam, t]() {
			qDebug() << "thread" << t << "ended. Removing NetworkAccessManager";
			nam->deleteLater();
			QMutexLocker lock(&MANAGERS.mutex);
			MANAGERS.perThreadManagers.remove(t);
		});
		MANAGERS.perThreadManagers[t] = nam;
	}
	return nam;
}

QNetworkReply *get(const QUrl &url, const QString &expectType)
{
	QNetworkRequest req(url);

	QNetworkReply *reply = getInstance()->get(req);

	reply->connect(reply, &QNetworkReply::metaDataChanged, [reply, expectType]() {
		QVariant mimetype = reply->header(QNetworkRequest::ContentTypeHeader);
		if(mimetype.isValid()) {
			if(mimetype.toString().startsWith(expectType)==false)
				reply->close();
		}
	});
	return reply;
}

FileDownload::FileDownload(QObject *parent)
	: QObject(parent), m_file(nullptr), m_reply(nullptr), m_maxSize(0), m_size(0)
{
}

void FileDownload::setTarget(const QString &path)
{
	Q_ASSERT(!m_file);
	m_file = new QSaveFile(path, this);
}

void FileDownload::setExpectedHash(const QByteArray &hash, QCryptographicHash::Algorithm algorithm)
{
	m_expectedHash = hash;
	m_hash.reset(new QCryptographicHash(algorithm));

#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
	if(hash.length() > 0 && QCryptographicHash::hashLength(algorithm) != hash.length()) {
		qWarning("Expected hash length %d not valid for selected algorithm!", hash.length());
		m_expectedHash = QByteArray();
		m_hash.reset();
	}
#endif
}
void FileDownload::start(const QUrl &url)
{
	Q_ASSERT(!m_reply);

	if(!m_file)
		m_file = new QTemporaryFile(this);

	if(!m_file->open(m_file->inherits("QSaveFile") ? QIODevice::WriteOnly : QIODevice::ReadWrite)) {
		emit finished(m_file->errorString());
		return;
	}

	QNetworkRequest req(url);

	m_reply = getInstance()->get(req);
	m_reply->setParent(this);

	if(!m_expectedType.isEmpty())
		connect(m_reply, &QNetworkReply::metaDataChanged, this, &FileDownload::onMetaDataChanged);

	connect(m_reply, &QNetworkReply::downloadProgress, this, &FileDownload::progress);
	connect(m_reply, &QNetworkReply::readyRead, this, &FileDownload::onReadyRead);
	connect(m_reply, &QNetworkReply::finished, this, &FileDownload::onFinished);
}

void FileDownload::onMetaDataChanged()
{
	QVariant mimetype = m_reply->header(QNetworkRequest::ContentTypeHeader);
	if(mimetype.isValid()) {
		if(!mimetype.toString().startsWith(m_expectedType)) {
			m_reply->abort();
		}
	}
}

void FileDownload::onReadyRead()
{
	const auto buffer = m_reply->readAll();

	m_size += buffer.length();
	if(m_maxSize>0 && m_size > m_maxSize) {
		m_reply->abort();
		return;
	}

	m_file->write(buffer);
	if(!m_hash.isNull())
		m_hash->addData(buffer);
}

void FileDownload::onFinished()
{
	if(m_reply->error()) {
		QString errorMessage;

		if(m_reply->error() == QNetworkReply::OperationCanceledError &&
			!m_reply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith(m_expectedType)) {
			errorMessage = tr("Unexpected content type (%1)").arg(m_reply->header(QNetworkRequest::ContentTypeHeader).toString());

		} else if(m_reply->error() == QNetworkReply::OperationCanceledError &&
			 m_maxSize > 0 && m_size > m_maxSize) {
			 errorMessage = tr("Downloaded file is too big");

		} else {
			errorMessage = m_reply->errorString();
		}

		if(errorMessage.isEmpty())
			errorMessage = "Unknown download error";

		emit finished(errorMessage);
		return;

	}
	// No error during download. Check that the checksum matches (if given)
	if(!m_hash.isNull()) {
		if(m_hash->result() != m_expectedHash) {
			emit finished(tr("Downloaded file's checksum does not match."));
			return;
		}
	}

	// Checksum OK! Finalize download...
	auto *saveFile = qobject_cast<QSaveFile*>(m_file);
	if(saveFile) {
		if(!saveFile->commit()) {
			emit finished(saveFile->errorString());
			return;
		}

		// Reopen for reading.
		const QString filename = saveFile->fileName();
		delete m_file;
		m_file = new QFile(filename, this);
		if(!m_file->open(QIODevice::ReadOnly)) {
			emit finished(m_file->errorString());
			return;
		}

	} else {
		// Not a QSaveFile? Then just rewind back to the beginning to make
		// this file ready to use
		m_file->seek(0);
	}

	emit finished(QString());
}

}
