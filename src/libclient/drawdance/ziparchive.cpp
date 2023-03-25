extern "C" {
#include <dpengine/zip_archive.h>
}

#include "libclient/drawdance/ziparchive.h"

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
			DP_zip_reader_file_size(m_zrf));
	} else {
		return QByteArray{};
	}
}

QString ZipReader::File::readUtf8() const
{
	if(m_zrf) {
		return QString::fromUtf8(
			static_cast<const char *>(DP_zip_reader_file_content(m_zrf)),
			DP_zip_reader_file_size(m_zrf));
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


}
