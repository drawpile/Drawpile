// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <cstdint>
#include <objidl.h>
#include <sqlite3.h>
#include <vector>
#include <windows.h>

HRESULT IStreamToBuffer(IStream *stream, std::vector<unsigned char> &out);
HRESULT OpenSqlite(std::vector<unsigned char> &databaseBuffer, sqlite3 **out);
HRESULT ExtractJPEG(sqlite3 *db, std::vector<unsigned char> &out);
HRESULT
JpegToHBitmap(unsigned char *data, size_t size, uint32_t cx, HBITMAP *phbmp);
