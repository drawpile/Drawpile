/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2009-2010 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#include <QDebug>
#include <QIODevice>
#include <QFile>
#include <QDateTime>
#include <QApplication>

#include "zip.h"
#include "unzip.h"

#include "zipfile.h"

/**
 * A specialization of QIODevice for reading from ZIP archives.
 */
class ZipIO : public QIODevice {
	public:
		ZipIO(unzFile f) : file(f) {
			unz_file_info info;
			error = unzGetCurrentFileInfo(file, &info, 0, 0, 0, 0, 0, 0);
			if(error != UNZ_OK)
				return;

			bytes = info.uncompressed_size;
			error = unzOpenCurrentFile(file);
			if(error == UNZ_OK)
				open(ReadOnly);
		}

		~ZipIO() {
			if(file!=0)
				unzCloseCurrentFile(file);
		}

		void close()
		{
			QIODevice::close();
			unzCloseCurrentFile(file);
			file = 0;
		}

		bool isSequential() { return true; }

		qint64 bytesAvailable() const
		{
			return bytes;
		}

		/** Get the unzip error code */
		int getUnzError() {
			return error;
		}

	protected:
		qint64 readData(char *data, qint64 maxSize)
		{
			int read = unzReadCurrentFile(file, data, maxSize);
			if(read>0)
				bytes -= read;
			return read;
		}

		qint64 writeData(const char *data, qint64 maxSize)
	   	{
			Q_UNUSED(data);
			Q_UNUSED(maxSize);
			return -1;
		}
	private:
		unzFile file;
		int error;
		quint64 bytes;
};

struct ZipfileImpl {
	ZipfileImpl(const QString &p, Zipfile::Mode m)
		: mode(m), path(p), error(0) { }

	//! The mode in which the file is opened
	Zipfile::Mode mode;

	//! Path to the file
	QString path;

	//! The zip file
	union {
		unzFile open; // When mode==READ
		zipFile save; // When mode!=READ
	} zip;

	//! Latest error code from minizip
	int error;
};

Zipfile::Zipfile(const QString& path, Zipfile::Mode mode)
	: priv(new ZipfileImpl(path, mode))
{
}

Zipfile::~Zipfile()
{
	if(priv->zip.open!=0)
		unzClose(priv->zip.open);
	delete priv;
}

/**
 * Open the file. In case of error, check errorMessage()
 * @return false on error
 */
bool Zipfile::open()
{
	if(priv->mode==READ) {
		priv->zip.open = unzOpen(priv->path.toLocal8Bit().constData());
		return priv->zip.open != 0;
	} else {
		if(priv->mode==WRITE) {
			// Check that the file doesn't exist already
			if(QFile::exists(priv->path))
				return false;
		}

		priv->zip.save = zipOpen(priv->path.toLocal8Bit().constData(), APPEND_STATUS_CREATE);
		return priv->zip.save != 0;
	}
}

/**
 * Close the file. In case of error, check errorMessage()
 * @return false on error
 */
bool Zipfile::close()
{
	bool ok;
	if(priv->mode==READ) {
		Q_ASSERT(priv->zip.open);
		ok = unzClose(priv->zip.open) == 0;
		if(ok)
			priv->zip.open = 0;
	} else {
		Q_ASSERT(priv->zip.save);
		ok = zipClose(priv->zip.save, 0) == 0;
		if(ok)
			priv->zip.save = 0;
	}
	return ok;
}

/**
 * @return error message
 */
QString Zipfile::errorMessage() const {
	switch(priv->error) {
		case 0: return QApplication::tr("No error"); break;
		default: return QApplication::tr("Unknown error (%1)").arg(priv->error);
	}
}

/**
 * The source IO device is deleted after use.
 * @param name name of the file to add
 * @param source the source of bytes to be added
 * @return false on error
 */
bool Zipfile::addFile(const QString& name, QIODevice *source, Zipfile::Method method)
{
	if(priv->mode == READ) {
		qCritical("Cannt add files to ZIP opened in read mode!");
		return false;
	}
	Q_ASSERT(priv->zip.save);

	// Set up file metadata
	zip_fileinfo info;
	QDateTime datetime = QDateTime::currentDateTime();
	QDate date = datetime.date();
	QTime time = datetime.time();
	info.tmz_date.tm_year = date.year();
	info.tmz_date.tm_mon = date.month() - 1;
	info.tmz_date.tm_mday = date.day();
	info.tmz_date.tm_hour = time.hour();
	info.tmz_date.tm_min = time.minute();
	info.tmz_date.tm_sec = time.second();
	info.dosDate = 0;
	info.internal_fa = 0;
	info.external_fa = 0;

	// Open file for writing
	int compmethod = 0;
	switch(method) {
		case DEFLATE:
		case DEFAULT: compmethod = Z_DEFLATED; break;
		case STORE: compmethod = 0; break;
	}

	priv->error = zipOpenNewFileInZip(priv->zip.save, name.toUtf8().constData(),
			&info, 0, 0, 0, 0, 0, compmethod, Z_BEST_SPEED);
	if(priv->error != ZIP_OK)
		return false;

	// Write file contents
	if(source->isOpen())
		source->reset();
	else
		source->open(QIODevice::ReadOnly);
	char buffer[1024];
	int len;
	while((len=source->read(buffer, sizeof buffer))>0) {
		priv->error = zipWriteInFileInZip(priv->zip.save, buffer, len);
		if(priv->error != ZIP_OK) {
			return false;
		}
	}

	// Done. Close file
	priv->error = zipCloseFileInZip(priv->zip.save);
	return priv->error == ZIP_OK;
}

QIODevice *Zipfile::getFile(const QString& name)
{
	if(priv->mode==Zipfile::READ) {
		Q_ASSERT(priv->zip.open);
		// Find the named file
		if( (priv->error = unzLocateFile(priv->zip.open, name.toUtf8().constData(), 1)) != UNZ_OK) {
			return 0;
		}

		// Create an IO device for reading it.
		// Note that we can't call getFile again until this
		// device has been closed.
		ZipIO *zip = new ZipIO(priv->zip.open);
		if(zip->getUnzError() != UNZ_OK) {
			priv->error = zip->getUnzError();
			delete zip;
			return 0;
		}
		return zip;
	} else {
		// Implement this if we ever need it
		qCritical() << "Getting files from write mode ZIPs not implemented!";
		return 0;
	}
}

