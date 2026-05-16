// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <propsys.h>
#include <thumbcache.h>
#include <windows.h>
#include <wrl/implements.h>

using namespace Microsoft::WRL;

class __declspec(uuid("3C98D577-6E3D-4C7F-B4A2-1D9F82E5C301"))
ThumbnailProvider final : public RuntimeClass<
							  RuntimeClassFlags<ClassicCom>,
							  IInitializeWithStream, IThumbnailProvider> {
	ComPtr<IStream> m_pStream;

public:
	IFACEMETHOD(Initialize)(IStream *pstream, DWORD grfMode) override;
	IFACEMETHOD(GetThumbnail)(
		UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) override;
};
