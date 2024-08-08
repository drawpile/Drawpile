// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include "zip_archive.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
}
#include <KZip>


struct DP_ZipReaderFile {
    QByteArray data;
};


extern "C" DP_ZipReader *DP_zip_reader_new(const char *path)
{
    KZip *kz = new KZip{path};
    if (kz->open(QIODevice::ReadOnly)) {
        return reinterpret_cast<DP_ZipReader *>(kz);
    }
    else {
        DP_error_set("Error opening '%s': %s", path,
                     qUtf8Printable(kz->errorString()));
        delete kz;
        return nullptr;
    }
}

extern "C" void DP_zip_reader_free(DP_ZipReader *zr)
{
    KZip *kz = reinterpret_cast<KZip *>(zr);
    if (kz) {
        delete kz;
    }
}

extern "C" DP_ZipReaderFile *DP_zip_reader_read_file(DP_ZipReader *zr,
                                                     const char *path)
{
    KZip *kz = reinterpret_cast<KZip *>(zr);
    const KArchiveFile *file = kz->directory()->file(QString::fromUtf8(path));
    if (file) {
        DP_ZipReaderFile *zrf = new DP_ZipReaderFile{file->data()};
        return zrf;
    }
    else {
        return nullptr;
    }
}


extern "C" size_t DP_zip_reader_file_size(DP_ZipReaderFile *zrf)
{
    return size_t(zrf->data.size());
}

extern "C" void *DP_zip_reader_file_content(DP_ZipReaderFile *zrf)
{
    return reinterpret_cast<void *>(zrf->data.data());
}

extern "C" void DP_zip_reader_file_free(DP_ZipReaderFile *zrf)
{
    delete zrf;
}


extern "C" DP_ZipWriter *DP_zip_writer_new(const char *path)
{
    KZip *kz = new KZip{path};
    if (kz->open(QIODevice::WriteOnly)) {
        return reinterpret_cast<DP_ZipWriter *>(kz);
    }
    else {
        DP_error_set("Error opening '%s': %s", path,
                     qUtf8Printable(kz->errorString()));
        delete kz;
        return nullptr;
    }
}

extern "C" void DP_zip_writer_free_abort(DP_ZipWriter *zw)
{
    delete reinterpret_cast<KZip *>(zw);
}

extern "C" bool DP_zip_writer_free_finish(DP_ZipWriter *zw)
{
    KZip *kz = reinterpret_cast<KZip *>(zw);
    bool ok = kz->close();
    if (!ok) {
        DP_error_set("Error closing zip archive: %s",
                     qUtf8Printable(kz->errorString()));
    }
    delete kz;
    return ok;
}

extern "C" bool DP_zip_writer_add_dir(DP_ZipWriter *zw, const char *path)
{
    KZip *kz = reinterpret_cast<KZip *>(zw);
    if (kz->writeDir(QString::fromUtf8(path))) {
        return true;
    }
    else {
        DP_error_set("Error creating directory '%s': %s", path,
                     qUtf8Printable(kz->errorString()));
        return false;
    }
}

extern "C" bool DP_zip_writer_add_file(DP_ZipWriter *zw, const char *path,
                                       const void *buffer, size_t size,
                                       bool deflate, bool take_buffer)
{
    KZip *kz = reinterpret_cast<KZip *>(zw);

    kz->setCompression(deflate ? KZip::DeflateCompression
                               : KZip::NoCompression);

    QByteArray data = QByteArray::fromRawData(static_cast<const char *>(buffer),
                                              DP_size_to_int(size));
    bool ok = kz->writeFile(QString::fromUtf8(path), data);
    if (take_buffer) {
        DP_free(const_cast<void *>(buffer));
    }

    if (ok) {
        return true;
    }
    else {
        DP_error_set("Error storing '%s' in zip: %s", path,
                     qUtf8Printable(kz->errorString()));
        return false;
    }
}
