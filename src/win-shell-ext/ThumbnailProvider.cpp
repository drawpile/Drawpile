// SPDX-License-Identifier: GPL-3.0-or-later

#include "ThumbnailProvider.h"
#include "dpcs.h"
#include <wrl/module.h>

using namespace Microsoft::WRL;

HRESULT STDMETHODCALLTYPE ThumbnailProvider::Initialize(IStream *pstream, DWORD)
{
	if(!pstream)
		return E_INVALIDARG;
	if(m_pStream)
		return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
	m_pStream = pstream;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE ThumbnailProvider::GetThumbnail(
	UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha)
{
	if(!phbmp || !pdwAlpha)
		return E_POINTER;
	*phbmp = nullptr;
	*pdwAlpha = WTSAT_RGB;

	HRESULT hr;

	std::vector<unsigned char> dpcsBuf;
	if(FAILED(hr = IStreamToBuffer(m_pStream.Get(), dpcsBuf)))
		return hr;

	sqlite3 *db;
	if(FAILED(hr = OpenSqlite(dpcsBuf, &db)))
		return hr;

	std::vector<unsigned char> jpegBuf;
	if(FAILED(hr = ExtractJPEG(db, jpegBuf))) {
		sqlite3_close(db);
		return hr;
	}

	if(FAILED(hr = JpegToHBitmap(jpegBuf.data(), jpegBuf.size(), cx, phbmp))) {
		sqlite3_close(db);
		return hr;
	}

	int rc = sqlite3_close(db);
	if(rc != SQLITE_OK) {
		fprintf(stderr, "Fail to close database: %s\n", sqlite3_errmsg(db));

		return E_FAIL;
	}

	return S_OK;
}

CoCreatableClass(ThumbnailProvider);
