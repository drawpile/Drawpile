#include <cstdint>
#include <shlobj.h>
#include <sqlite3.h>
#include <stdio.h>
#include <windows.h>
#include <wrl/module.h>

using namespace Microsoft::WRL;

static HMODULE g_hModule = nullptr;

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID)
{
	if(dwReason == DLL_PROCESS_ATTACH) {
		int rc = sqlite3_initialize();
		if(rc != SQLITE_OK) {
			fprintf(stderr, "Cannot initialize SQLite: %d\n", rc);
			return FALSE;
		}

		g_hModule = static_cast<HMODULE>(hInstance);
		DisableThreadLibraryCalls(hInstance);
	}
	return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
	return Module<InProc>::GetModule().GetClassObject(rclsid, riid, ppv);
}

STDAPI DllCanUnloadNow()
{
	return Module<InProc>::GetModule().Terminate() == 0 ? S_OK : S_FALSE;
}

// regsvr32 functions
static HRESULT SetRegValue(
	HKEY hRoot, const wchar_t *subKey, const wchar_t *valueName,
	const wchar_t *data)
{
	HKEY hk = nullptr;
	LONG r = RegCreateKeyExW(
		hRoot, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE,
		nullptr, &hk, nullptr);
	if(r != ERROR_SUCCESS)
		return HRESULT_FROM_WIN32(r);
	uint32_t cb = static_cast<uint32_t>((wcslen(data) + 1) * sizeof(wchar_t));
	r = RegSetValueExW(
		hk, valueName, 0, REG_SZ, reinterpret_cast<const BYTE *>(data), cb);
	RegCloseKey(hk);
	return HRESULT_FROM_WIN32(r);
}

static constexpr wchar_t kClsidStr[] =
	L"{3C98D577-6E3D-4C7F-B4A2-1D9F82E5C301}";
static constexpr wchar_t kFriendlyName[] = L"Drawpile Thumbnail Provider";
static constexpr wchar_t kThreading[] = L"Apartment";

static constexpr wchar_t kKeyClsid[] =
	L"Software\\Classes\\CLSID\\{3C98D577-6E3D-4C7F-B4A2-1D9F82E5C301}";
static constexpr wchar_t kKeyInproc[] =
	L"Software\\Classes\\CLSID\\{3C98D577-6E3D-4C7F-B4A2-1D9F82E5C301}"
	L"\\InprocServer32";
static constexpr wchar_t kKeyShellEx[] =
	L"Software\\Classes\\.dpcs\\ShellEx\\{E357FCCD-A995-4576-B01F-"
	L"234630154E96}";

STDAPI DllRegisterServer()
{
	wchar_t szPath[MAX_PATH] = {};
	if(!GetModuleFileNameW(g_hModule, szPath, MAX_PATH))
		return HRESULT_FROM_WIN32(GetLastError());

	HRESULT hr;
	if(FAILED(
		   hr = SetRegValue(
			   HKEY_LOCAL_MACHINE, kKeyClsid, nullptr, kFriendlyName)))
		return hr;
	if(FAILED(
		   hr = SetRegValue(HKEY_LOCAL_MACHINE, kKeyInproc, nullptr, szPath)))
		return hr;
	if(FAILED(
		   hr = SetRegValue(
			   HKEY_LOCAL_MACHINE, kKeyInproc, L"ThreadingModel", kThreading)))
		return hr;
	if(FAILED(
		   hr = SetRegValue(
			   HKEY_LOCAL_MACHINE, kKeyShellEx, nullptr, kClsidStr)))
		return hr;

	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
	return S_OK;
}

STDAPI DllUnregisterServer()
{
	RegDeleteTreeW(HKEY_LOCAL_MACHINE, kKeyClsid);
	RegDeleteTreeW(HKEY_LOCAL_MACHINE, kKeyShellEx);

	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
	return S_OK;
}
