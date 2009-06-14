/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2009 Calle Laakkonen

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

#include <zip.h>

#include "zipfile.h"

/**
 * A specialization of QIODevice for reading from ZIP archives.
 */
class ZipIO : public QIODevice {
	public:
		ZipIO(zip_file *f, struct zip_stat stat) : file(f) {
			bytes = stat.size;
			open(ReadOnly);
		}

		~ZipIO() {
			if(file!=0)
				zip_fclose(file);
		}

		void close()
		{
			QIODevice::close();
			zip_fclose(file);
			file = 0;
		}

		bool isSequential() { return true; }

		qint64 bytesAvailable() const
		{
			return bytes + QIODevice::bytesAvailable();
		}

	protected:
		qint64 readData(char *data, qint64 maxSize)
		{
			qint64 read = zip_fread(file, data, maxSize);
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
		zip_file *file;
		quint64 bytes;
};

struct ZipfileImpl {
	ZipfileImpl(const QString &p, Zipfile::Mode m)
		: mode(m), path(p), zipfile(0), error(0) { }

	//! The mode in which the file is opened
	Zipfile::Mode mode;

	//! Path to the file
	QString path;

	//! libzip structure
	zip *zipfile;
	//! Latest error code from libzip
	int error;
};

Zipfile::Zipfile(const QString& path, Zipfile::Mode mode)
	: priv(new ZipfileImpl(path, mode))
{
}

Zipfile::~Zipfile()
{
	if(priv->zipfile!=0)
		zip_close(priv->zipfile);
	delete priv;
}

/**
 * Open the file. In case of error, check errorMessage()
 * @return false on error
 */
bool Zipfile::open()
{
	int flags = 0;
	switch(priv->mode) {
		case READ: break;
		case WRITE: flags = ZIP_CREATE;
		case OVERWRITE: flags = ZIP_CREATE | ZIP_EXCL;
	}
	priv->zipfile = zip_open(priv->path.toLocal8Bit().constData(), flags, &priv->error);

	// In overwrite mode we must delete the old file if it exists
	if(priv->mode == OVERWRITE && priv->zipfile==0 && priv->error==ZIP_ER_EXISTS) {
		if(QFile::remove(priv->path))
			return open();
		priv->error = ZIP_ER_OPEN;
		return false;
	}

	return priv->zipfile != 0;
}

/**
 * Close the file. In case of error, check errorMessage()
 * @return false on error
 */
bool Zipfile::close()
{
	Q_ASSERT(priv->zipfile);
	bool ok = zip_close(priv->zipfile) == 0;
	if(ok)
		priv->zipfile = 0;
	return ok;
}

/**
 * @return error message
 */
const char *Zipfile::errorMessage() const {
	switch(priv->error) {
		case 0: return QT_TR_NOOP("No error"); break;
		case ZIP_ER_EXISTS: return QT_TR_NOOP("File already exists"); break;
		case ZIP_ER_INCONS: return QT_TR_NOOP("ZIP file inconsistent"); break;
		case ZIP_ER_INVAL: return QT_TR_NOOP("File path missing"); break;
		case ZIP_ER_MEMORY: return QT_TR_NOOP("Cannot allocate memory"); break;
		case ZIP_ER_NOENT: return QT_TR_NOOP("No such file"); break;
		case ZIP_ER_NOZIP: return QT_TR_NOOP("Not a ZIP file"); break;
		case ZIP_ER_OPEN: return QT_TR_NOOP("File cannot be opened"); break;
		case ZIP_ER_READ: return QT_TR_NOOP("A read error occurred"); break;
		case ZIP_ER_SEEK: return QT_TR_NOOP("Cannot seek in file"); break;
		case ZIP_ER_COMPNOTSUPP: return QT_TR_NOOP("Unsupported compression method"); break;
		case ZIP_ER_CHANGED: return QT_TR_NOOP("Data has changed"); break;
		case ZIP_ER_ZLIB: return QT_TR_NOOP("Couldn't initialize zlib"); break;
		default: return QT_TR_NOOP("Unknown error");
	}
}

struct iodev_context {
	QIODevice *io;
	unsigned int size;
	time_t mtime;
	int method;
};

/**
 * Source adapter for an IODevice
 */
ssize_t iodev_source(void *state, void *data, size_t len, zip_source_cmd cmd)
{
	iodev_context *ctx = static_cast<iodev_context*>(state);
	switch(cmd) {
		case ZIP_SOURCE_OPEN:
			if(ctx->io->isOpen())
				return ctx->io->reset() ? 0 : -1;
			return ctx->io->open(QIODevice::ReadOnly) ? 0 : -1;
		case ZIP_SOURCE_READ:
			return ctx->io->read(static_cast<char*>(data), len);
		case ZIP_SOURCE_CLOSE:
			ctx->io->close();
			break;
		case ZIP_SOURCE_STAT: {
			struct zip_stat *stat = static_cast<struct zip_stat*>(data);
			zip_stat_init(stat);
			stat->size = ctx->size;
			stat->mtime = ctx->mtime;
			if(ctx->method != ZIP_CM_DEFAULT)
				stat->comp_method = ctx->method;
			return sizeof(struct zip_stat);
		  }
		case ZIP_SOURCE_ERROR:
			// TODO
			return 2 * sizeof(int);
		case ZIP_SOURCE_FREE:
			delete ctx->io;
			delete ctx;
			break;
	}
	return 0;
}

/**
 * The source IO device is deleted after use.
 * @param name name of the file to add
 * @param source the source of bytes to be added
 * @return false on error
 */
bool Zipfile::addFile(const QString& name, QIODevice *source, unsigned int length, time_t modified, Zipfile::Method method)
{
	Q_ASSERT(priv->zipfile);

	iodev_context *ctx = new iodev_context;
	ctx->io = source;
	ctx->size = length;
	ctx->mtime = modified;
	switch(method) {
		case DEFAULT: ctx->method = ZIP_CM_DEFAULT; break;
		case STORE: ctx->method = ZIP_CM_STORE; break;
		case DEFLATE: ctx->method = ZIP_CM_DEFLATE; break;
	}
	zip_source *src = zip_source_function(priv->zipfile, iodev_source, ctx);
	if(src==0) {
		delete ctx;
		return false;
	}

	bool ok = zip_add(priv->zipfile, name.toUtf8().constData(), src) > -1;
	if(!ok)
		zip_source_free(src);
	return ok;
}

QIODevice *Zipfile::getFile(const QString& name)
{
	Q_ASSERT(priv->zipfile);
	int index = zip_name_locate(priv->zipfile, name.toUtf8().constData(), 0);
	if(index<0)
		return 0;
	return getFile(index);
}

QIODevice *Zipfile::getFile(int index)
{
	Q_ASSERT(priv->zipfile);
	struct zip_stat stat;

	if(zip_stat_index(priv->zipfile, index, 0, &stat)==-1)
		return 0;

	zip_file *file = zip_fopen_index(priv->zipfile, index, 0);
	if(file==0)
		return 0;

	return new ZipIO(file, stat);
}

