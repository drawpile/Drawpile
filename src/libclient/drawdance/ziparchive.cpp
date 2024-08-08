// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpimpex/zip_archive.h>
}

#include "libclient/drawdance/ziparchive.h"
#include "libshared/util/qtcompat.h"


namespace drawdance {


ZipReader::File::~File()
{
	DP_zip_reader_file_free(m_zrf);
}

bool ZipReader::File::isNull() const
{
	return m_zrf == nullptr;
}

size_t ZipReader::File::size() const
{
	return m_zrf ? DP_zip_reader_file_size(m_zrf) : 0;
}

void *ZipReader::File::content() const
{
	return m_zrf ? DP_zip_reader_file_content(m_zrf) : nullptr;
}

QByteArray ZipReader::File::readBytes() const
{
	if(m_zrf) {
		return QByteArray::fromRawData(
			static_cast<const char *>(DP_zip_reader_file_content(m_zrf)),
			compat::castSize(DP_zip_reader_file_size(m_zrf)));
	} else {
		return QByteArray{};
	}
}

QString ZipReader::File::readUtf8() const
{
	if(m_zrf) {
		return QString::fromUtf8(
			static_cast<const char *>(DP_zip_reader_file_content(m_zrf)),
			compat::castSize(DP_zip_reader_file_size(m_zrf)));
	} else {
		return QString{};
	}
}

ZipReader::File::File(DP_ZipReaderFile *zrf)
	: m_zrf{zrf}
{
}


ZipReader::ZipReader(const QString &path)
	: m_zr{DP_zip_reader_new(path.toUtf8())}
{
}

ZipReader::~ZipReader()
{
	DP_zip_reader_free(m_zr);
}

bool ZipReader::isNull() const
{
	return m_zr == nullptr;
}

ZipReader::File ZipReader::readFile(const QString &path) const
{
	return File{m_zr ? DP_zip_reader_read_file(m_zr, path.toUtf8()) : nullptr};
}


ZipWriter::ZipWriter(const QString &path)
	: m_zw{DP_zip_writer_new(path.toUtf8())}
{
}

ZipWriter::~ZipWriter()
{
	abort();
}

bool ZipWriter::isNull() const
{
	return m_zw == nullptr;
}

bool ZipWriter::addDir(const QString &path)
{
	QByteArray pathBytes = path.toUtf8();
	return DP_zip_writer_add_dir(m_zw, pathBytes.constData());
}

bool ZipWriter::addFile(
	const QString &path, const QByteArray &bytes, bool deflate)
{
	QByteArray pathBytes = path.toUtf8();
	size_t size = bytes.size();
	char *buffer = static_cast<char *>(DP_malloc(size));
	memcpy(buffer, bytes.constData(), size);
	return DP_zip_writer_add_file(
		m_zw, pathBytes.constData(), buffer, size, deflate, true);
}

bool ZipWriter::finish()
{
	bool ok = DP_zip_writer_free_finish(m_zw);
	m_zw = nullptr;
	return ok;
}

void ZipWriter::abort()
{
	if(m_zw) {
		DP_zip_writer_free_abort(m_zw);
		m_zw = nullptr;
	}
}


}
