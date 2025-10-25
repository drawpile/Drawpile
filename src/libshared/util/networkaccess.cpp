// SPDX-License-Identifier: GPL-3.0-or-later

#include "libshared/util/networkaccess.h"
#include "libshared/util/qtcompat.h"
#include <dpcommon/platform_qt.h>
#include <QNetworkReply>
#include <QMutexLocker>
#include <QHash>
#include <QThread>
#include <QDebug>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QBuffer>
#include <QCoreApplication>

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
		nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
		QMetaObject::Connection conn = t->connect(t, &QThread::finished, [nam, t]() {
			qDebug() << "thread" << t << "ended. Removing NetworkAccessManager";
			nam->deleteLater();
			QMutexLocker guard(&MANAGERS.mutex);
			MANAGERS.perThreadManagers.remove(t);
		});
		// Don't fiddle with statics during global destruction.
		nam->connect(
			QCoreApplication::instance(), &QCoreApplication::aboutToQuit, nam,
			[nam, conn] {
				QObject::disconnect(conn);
				nam->deleteLater();
			});
		MANAGERS.perThreadManagers[t] = nam;
	}
	return nam;
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

#ifdef HAVE_QT_COMPAT_HASH_LENGTH
	if(hash.length() > 0 && QCryptographicHash::hashLength(algorithm) != hash.length()) {
		qWarning() << "Expected hash length" << hash.length() << "not valid for" << algorithm;
		m_expectedHash = QByteArray();
		m_hash.reset();
	}
#endif
}
void FileDownload::start(const QUrl &url)
{
	Q_ASSERT(!m_reply);

	QNetworkRequest req(url);

	m_reply = getInstance()->get(req);
	m_reply->setParent(this);

	if(!m_expectedType.isEmpty())
		connect(m_reply, &QNetworkReply::metaDataChanged, this, &FileDownload::onMetaDataChanged);

	connect(m_reply, &QNetworkReply::downloadProgress, this, &FileDownload::progress);
	connect(m_reply, &QNetworkReply::readyRead, this, &FileDownload::onReadyRead);
	connect(m_reply, &QNetworkReply::finished, this, &FileDownload::onFinished);
}

void FileDownload::cancel()
{
	m_reply->abort();
}

void FileDownload::onMetaDataChanged()
{
	const QString mimetype = m_reply->header(QNetworkRequest::ContentTypeHeader).toString();
	if(!mimetype.startsWith(m_expectedType)) {
		m_errorMessage = tr("Unexpected content type (%1)").arg(mimetype);
		m_reply->abort();
	}
}

void FileDownload::onReadyRead()
{
	if(!m_file) {
		bool ok;
		qint64 expectedSize = m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong(&ok);
		if(ok) {
			if(m_maxSize > 0 && expectedSize > m_maxSize) {
				m_errorMessage = tr("Downloaded file is too big");
				m_reply->abort();
				return;
			}

		} else {
			expectedSize = m_maxSize;
		}

		if(expectedSize > 0 && expectedSize < 1024 * 1024) {
			qDebug() << m_reply->url() << "opening temporary buffer. Expecting" << expectedSize << "bytes.";
			m_file = new QBuffer(this);
		} else {
			qDebug() << m_reply->url() << "opening temporary file. Expecting" << (expectedSize/(1024.0*1024.0)) << "megabytes.";
			m_file = new QTemporaryFile(this);
		}
	}

	Q_ASSERT(m_file);

	if(!m_file->isOpen()) {
		if(!m_file->open(m_file->inherits("QSaveFile") ? DP_QT_WRITE_FLAGS : QIODevice::ReadWrite)) {
			m_errorMessage = m_file->errorString();
			m_reply->abort();
			return;
		}
	}

	const auto buffer = m_reply->readAll();

	m_size += buffer.length();
	if(m_maxSize>0 && m_size > m_maxSize) {
		m_errorMessage = tr("Downloaded file is too big");
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
		qWarning() << m_reply->url() << "error:" << m_reply->errorString();
		emit finished(m_errorMessage.isEmpty() ? m_reply->errorString() : m_errorMessage);
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
