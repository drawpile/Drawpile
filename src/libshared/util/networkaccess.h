// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NETWORKACCESS_H
#define NETWORKACCESS_H

#include <QObject>
#include <QCryptographicHash>
#include <QScopedPointer>

class QNetworkAccessManager;
class QNetworkReply;
class QString;
class QUrl;
class QIODevice;

namespace networkaccess {

/**
 * @brief Get a shared instance of a QNetworkAccessManager
 *
 * The returned instance will be unique to the current thread
 */
QNetworkAccessManager *getInstance();

/**
 * @brief A file download wrapper that emits progress reports
 */
class FileDownload final : public QObject {
	Q_OBJECT
public:
	explicit FileDownload(QObject *parent=nullptr);

	/**
	 * Set target file path
	 *
	 * This must be called before *start*. If no target file is set,
	 * a temporary file (or a QBuffer if the file is small) is used.
	 *
	 * @param path
	 */
	void setTarget(const QString &path);

	/**
	 * Set the maximum size of the download
	 *
	 * This must be called before *start*
	 */
	void setMaxSize(qint64 maxSize) { m_maxSize = maxSize; }

	/**
	 * Set the expected type of the file
	 *
	 * This must be called before *start*
	 *
	 * @param mimetype
	 */
	void setExpectedType(const QString &mimetype) { m_expectedType = mimetype; }

	/**
	 * Set the expected checksum of the file
	 *
	 * This must be called before *start*
	 *
	 * @param hash the hash (in binary form)
	 * @param algorithm hashing algorithm used
	 */
	void setExpectedHash(const QByteArray &hash, QCryptographicHash::Algorithm algorithm);

	//! Start downloading
	void start(const QUrl &url);

	/**
	 * Get access to the file device
	 *
	 * This is either a temporary file, a buffer or---if a target filename was given, a regular QFile.
	 *
	 * Do not call until finished() is emitted with no error.
	 */
	QIODevice *file() { return m_file; }

signals:
	//! Download progress report
	void progress(qint64 received, qint64 total);

	//! Download finished. Error message will be non-empty if the download failed
	void finished(const QString &errorMessage);

private slots:
	void onMetaDataChanged();
	void onReadyRead();
	void onFinished();

private:
	QIODevice *m_file;
	QNetworkReply *m_reply;
	QString m_expectedType;
	QByteArray m_expectedHash;
	qint64 m_maxSize;
	qint64 m_size;

	QScopedPointer<QCryptographicHash> m_hash;
	QString m_errorMessage;
};
}

#endif
