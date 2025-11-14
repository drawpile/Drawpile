// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/file.h>
#include <dpengine/compress.h>
}
#include "libclient/drawdance/compress.h"
#include "libshared/util/qtcompat.h"

namespace drawdance {

static void *slurpFile(const QString &path, size_t *outLength)
{

	QByteArray pathBytes = path.toUtf8();
	return DP_file_slurp(pathBytes.constData(), outLength);
}

static unsigned char *getDecompressBuffer(size_t size, void *user)
{
	QByteArray *buffer = static_cast<QByteArray *>(user);
	buffer->resize(compat::castSize(size));
	return reinterpret_cast<unsigned char *>(buffer->data());
}

QString decompressZstdFile(const QString &path)
{
	size_t length;
	void *data = slurpFile(path, &length);
	if(!data) {
		return QString();
	}

	QByteArray buffer;
	bool ok = DP_decompress_zstd(
		nullptr, static_cast<unsigned char *>(data), length,
		getDecompressBuffer, &buffer);
	DP_free(data);
	if(!ok) {
		return QString();
	}

	return QString::fromUtf8(buffer);
}

}
