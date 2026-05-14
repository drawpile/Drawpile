#include <cstdint>
#include <objbase.h>
#include <shlwapi.h>
#include <stdio.h>
#include <thumbcache.h>
#include <windows.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

static const CLSID CLSID_ThumbProvider = {
	0x3C98D577,
	0x6E3D,
	0x4C7F,
	{0xB4, 0xA2, 0x1D, 0x9F, 0x82, 0xE5, 0xC3, 0x01}};

static void SaveBmp(const wchar_t *path, HBITMAP hbm)
{
	BITMAP bm{};
	if(!GetObject(hbm, sizeof(bm), &bm))
		return;

	const LONG w = bm.bmWidth;
	const LONG h = abs(bm.bmHeight);
	const uint32_t pixBytes = static_cast<uint32_t>(w * h * 4);

	BITMAPINFOHEADER bih{};
	bih.biSize = sizeof(bih);
	bih.biWidth = w;
	bih.biHeight = h;
	bih.biPlanes = 1;
	bih.biBitCount = 32;
	bih.biCompression = BI_RGB;

	BITMAPFILEHEADER bfh{};
	bfh.bfType = 0x4D42;
	bfh.bfSize = static_cast<DWORD>(sizeof(bfh) + sizeof(bih) + pixBytes);
	bfh.bfOffBits = sizeof(bfh) + sizeof(bih);

	void *pixels = HeapAlloc(GetProcessHeap(), 0, pixBytes);
	if(!pixels)
		return;

	BITMAPINFO bi{};
	bi.bmiHeader = bih;
	HDC hdc = GetDC(nullptr);
	GetDIBits(hdc, hbm, 0, static_cast<UINT>(h), pixels, &bi, DIB_RGB_COLORS);
	ReleaseDC(nullptr, hdc);

	HANDLE hf = CreateFileW(
		path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
		nullptr);
	if(hf != INVALID_HANDLE_VALUE) {
		DWORD written;
		WriteFile(hf, &bfh, sizeof(bfh), &written, nullptr);
		WriteFile(hf, &bih, sizeof(bih), &written, nullptr);
		WriteFile(hf, pixels, pixBytes, &written, nullptr);
		CloseHandle(hf);
		wprintf(L"Saved %s (%ldx%ld)\n", path, w, h);
	}
	HeapFree(GetProcessHeap(), 0, pixels);
}

int wmain()
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if(FAILED(hr)) {
		wprintf(L"CoInitializeEx: 0x%08lX\n", static_cast<unsigned long>(hr));
		return 1;
	}

	// All ComPtrs live inside this lambda so they're Released before
	// CoUninitialize().
	const int exitCode = []() -> int {
		HRESULT hr;

		ComPtr<IInitializeWithStream> pInit;
		hr = CoCreateInstance(
			CLSID_ThumbProvider, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&pInit));
		if(FAILED(hr)) {
			wprintf(
				L"CoCreateInstance: 0x%08lX\n", static_cast<unsigned long>(hr));
			return 1;
		}

		ComPtr<IStream> pStream;
		hr = SHCreateStreamOnFileEx(
			L"test.dpcs", STGM_READ | STGM_SHARE_DENY_WRITE, 0, FALSE, nullptr,
			&pStream);
		if(FAILED(hr)) {
			wprintf(
				L"SHCreateStreamOnFileEx: 0x%08lX\n",
				static_cast<unsigned long>(hr));
			return 1;
		}

		hr = pInit->Initialize(pStream.Get(), STGM_READ);
		if(FAILED(hr)) {
			wprintf(L"Initialize: 0x%08lX\n", static_cast<unsigned long>(hr));
			return 1;
		}

		ComPtr<IThumbnailProvider> pThumb;
		hr = pInit.As(&pThumb);
		if(FAILED(hr)) {
			wprintf(
				L"QI(IThumbnailProvider): 0x%08lX\n",
				static_cast<unsigned long>(hr));
			return 1;
		}

		constexpr unsigned int kSize = 256;
		HBITMAP hbm = nullptr;
		WTS_ALPHATYPE alphaType = WTSAT_UNKNOWN;
		hr = pThumb->GetThumbnail(kSize, &hbm, &alphaType);
		if(FAILED(hr) || !hbm) {
			wprintf(L"GetThumbnail: 0x%08lX\n", static_cast<unsigned long>(hr));
			return 1;
		}

		wprintf(
			L"GetThumbnail OK  size=%u  alphaType=%d\n", kSize,
			static_cast<int>(alphaType));
		SaveBmp(L"thumbnail.bmp", hbm);
		DeleteObject(hbm);
		return 0;
	}();

	CoUninitialize();
	return exitCode;
}
