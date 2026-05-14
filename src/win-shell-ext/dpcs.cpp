#include "dpcs.h"
#include <algorithm>
#include <cstdint>
#include <intsafe.h>
#include <wincodec.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

HRESULT IStreamToBuffer(IStream *stream, std::vector<unsigned char> &out)
{
	HRESULT hr;

	STATSTG stats;
	if(FAILED(hr = stream->Stat(&stats, STATFLAG_NONAME)))
		return hr;

	out.resize(stats.cbSize.QuadPart);

	ULONG bytesRead;
	if(FAILED(
		   hr = stream->Read(
			   out.data(), static_cast<uint32_t>(out.size()), &bytesRead)))
		return hr;

	if(out.size() > bytesRead) {
		out.resize(bytesRead);
	}

	return S_OK;
}

HRESULT OpenSqlite(std::vector<unsigned char> &databaseBuffer, sqlite3 **out)
{
	int rc;
	rc = sqlite3_open(":memory:", out);
	if(rc != SQLITE_OK) {
		fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(*out));
		sqlite3_close(*out);
		return E_FAIL;
	}

	rc = sqlite3_deserialize(
		*out, nullptr, databaseBuffer.data(), databaseBuffer.size(),
		databaseBuffer.size(), SQLITE_DESERIALIZE_READONLY);
	if(rc != SQLITE_OK) {
		fprintf(
			stderr, "Fail to deserialized database: %s\n",
			sqlite3_errmsg(*out));
		sqlite3_close(*out);
		return E_FAIL;
	}

	return S_OK;
}

HRESULT ExtractJPEG(sqlite3 *db, std::vector<unsigned char> &out)
{
	sqlite3_stmt *stmt;
	int rc;
	rc = sqlite3_prepare_v2(
		db, "SELECT thumbnail FROM snapshots ORDER BY taken_at DESC LIMIT 1",
		-1, &stmt, nullptr);
	if(rc != SQLITE_OK) {
		fprintf(stderr, "Fail to query database: %s\n", sqlite3_errmsg(db));
		return E_FAIL;
	}
	rc = sqlite3_step(stmt);
	if(rc != SQLITE_ROW) {
		fprintf(stderr, "Fail to query database: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return E_FAIL;
	}

	const uint8_t *rowData =
		static_cast<const uint8_t *>(sqlite3_column_blob(stmt, 0));
	int rowSize = sqlite3_column_bytes(stmt, 0);

	// Copy result to buffer
	out.assign(rowData, rowData + rowSize);

	// rowData is freed by sqlite3_finalize
	sqlite3_finalize(stmt);

	return S_OK;
}

HRESULT
JpegToHBitmap(unsigned char *data, size_t size, uint32_t cx, HBITMAP *phbmp)
{
	if(!data || !size || !phbmp)
		return E_INVALIDARG;
	*phbmp = nullptr;

	ComPtr<IWICImagingFactory> factory;
	HRESULT hr = CoCreateInstance(
		CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&factory));
	if(FAILED(hr))
		return hr;

	ComPtr<IWICStream> wicStream;
	if(FAILED(hr = factory->CreateStream(&wicStream)))
		return hr;
	// InitializeFromMemory takes non-const BYTE* but does not modify the buffer
	if(FAILED(
		   hr = wicStream->InitializeFromMemory(
			   data, static_cast<int32_t>(size))))
		return hr;

	ComPtr<IWICBitmapDecoder> decoder;
	if(FAILED(
		   hr = factory->CreateDecoderFromStream(
			   wicStream.Get(), nullptr, WICDecodeMetadataCacheOnLoad,
			   &decoder)))
		return hr;

	ComPtr<IWICBitmapFrameDecode> frame;
	if(FAILED(hr = decoder->GetFrame(0, &frame)))
		return hr;

	uint32_t srcW = 0, srcH = 0;
	if(FAILED(hr = frame->GetSize(&srcW, &srcH)))
		return hr;

	// Scale to fit within cx×cx, maintaining aspect ratio
	uint32_t outW, outH;
	if(srcW >= srcH) {
		outW = cx;
		outH = std::max(1u, srcH * cx / srcW);
	} else {
		outH = cx;
		outW = std::max(1u, srcW * cx / srcH);
	}

	ComPtr<IWICBitmapScaler> scaler;
	if(FAILED(hr = factory->CreateBitmapScaler(&scaler)))
		return hr;
	if(FAILED(
		   hr = scaler->Initialize(
			   frame.Get(), outW, outH, WICBitmapInterpolationModeFant)))
		return hr;

	ComPtr<IWICFormatConverter> converter;
	if(FAILED(hr = factory->CreateFormatConverter(&converter)))
		return hr;
	if(FAILED(
		   hr = converter->Initialize(
			   scaler.Get(), GUID_WICPixelFormat32bppBGRA,
			   WICBitmapDitherTypeNone, nullptr, 0.0,
			   WICBitmapPaletteTypeCustom)))
		return hr;

	BITMAPINFO bmi{};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = static_cast<int32_t>(outW);
	bmi.bmiHeader.biHeight = -static_cast<int32_t>(outH);
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void *pvBits = nullptr;
	HDC hdc = GetDC(nullptr);
	HBITMAP hbm =
		CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
	ReleaseDC(nullptr, hdc);
	if(!hbm)
		return E_OUTOFMEMORY;

	const uint32_t stride = outW * 4;

	hr = converter->CopyPixels(
		nullptr, stride, stride * outH, static_cast<uint8_t *>(pvBits));
	if(FAILED(hr)) {
		DeleteObject(hbm);
		return hr;
	}

	*phbmp = hbm;
	return S_OK;
}
