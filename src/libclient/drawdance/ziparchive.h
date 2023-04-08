// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_ZIPARCHIVE_H
#define DRAWDANCE_ZIPARCHIVE_H

extern "C" {
#include <dpcommon/common.h>
}

#include <QByteArray>
#include <QString>

struct DP_ZipReader;
struct DP_ZipReaderFile;

namespace drawdance {

class ZipReader final {
public:
	class File final {
		friend class ZipReader;

	public:
		~File();

		File(const File &) = delete;
		File(File &&) = delete;
		File &operator=(const File &) = delete;
		File &operator=(File &&) = delete;

		bool isNull() const;
    	size_t size() const;
        void *content() const;

        QByteArray readBytes() const;
        QString readUtf8() const;

	private:
		explicit File(DP_ZipReaderFile *zrf);

		DP_ZipReaderFile *m_zrf;
	};

	explicit ZipReader(const QString &path);
	~ZipReader();

	ZipReader(const ZipReader &) = delete;
	ZipReader(ZipReader &&) = delete;
	ZipReader &operator=(const ZipReader &) = delete;
	ZipReader &operator=(ZipReader &&) = delete;

	bool isNull() const;

	File readFile(const QString &path) const;

private:
	DP_ZipReader *m_zr;
};

}

#endif
