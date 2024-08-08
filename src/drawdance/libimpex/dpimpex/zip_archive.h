// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_ZIP_ARCHIVE_H
#define DPIMPEX_ZIP_ARCHIVE_H
#include <dpcommon/common.h>

typedef struct DP_ZipReader DP_ZipReader;
typedef struct DP_ZipReaderFile DP_ZipReaderFile;
typedef struct DP_ZipWriter DP_ZipWriter;


DP_ZipReader *DP_zip_reader_new(const char *path);

void DP_zip_reader_free(DP_ZipReader *zr);

DP_ZipReaderFile *DP_zip_reader_read_file(DP_ZipReader *zr, const char *path);


size_t DP_zip_reader_file_size(DP_ZipReaderFile *zrf);

void *DP_zip_reader_file_content(DP_ZipReaderFile *zrf);

void DP_zip_reader_file_free(DP_ZipReaderFile *zrf);


DP_ZipWriter *DP_zip_writer_new(const char *path);

void DP_zip_writer_free_abort(DP_ZipWriter *zw);

bool DP_zip_writer_free_finish(DP_ZipWriter *zw) DP_MUST_CHECK;

bool DP_zip_writer_add_dir(DP_ZipWriter *zw, const char *path) DP_MUST_CHECK;

bool DP_zip_writer_add_file(DP_ZipWriter *zw, const char *path,
                            const void *buffer, size_t size, bool deflate,
                            bool take_buffer) DP_MUST_CHECK;


#endif
