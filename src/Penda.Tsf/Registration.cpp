#include "TextService.h"
#include <new>

namespace penda {
HMODULE module = nullptr;
std::atomic<long> objects{0};
class Factory final : public IClassFactory {
    std::atomic<ULONG> references_{1};
public:
    Factory() { ++objects; } ~Factory() { --objects; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override { if (!out) return E_POINTER; *out = nullptr; if (iid != IID_IUnknown && iid != IID_IClassFactory) return E_NOINTERFACE; *out = static_cast<IClassFactory*>(this); AddRef(); return S_OK; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override { auto count = --references_; if (!count) delete this; return count; }
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID iid, void** out) override {
        if (!out) return E_POINTER; *out = nullptr; if (outer) return CLASS_E_NOAGGREGATION;
        try { auto service = new TextService(); auto hr = service->QueryInterface(iid, out); service->Release(); return hr; } catch (...) { return E_OUTOFMEMORY; }
    }
    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override { if (lock) ++objects; else --objects; return S_OK; }
};
std::wstring ClassPath() { wchar_t guid[40]{}; StringFromGUID2(ServiceId,guid,40); return std::wstring(L"Software\\Classes\\CLSID\\") + guid; }
HRESULT SetValue(const std::wstring& path, const wchar_t* name, const std::wstring& value) {
    HKEY key = nullptr; auto error = RegCreateKeyExW(HKEY_LOCAL_MACHINE,path.c_str(),0,nullptr,0,KEY_WRITE,nullptr,&key,nullptr);
    if (error != ERROR_SUCCESS) return HRESULT_FROM_WIN32(error);
    error = RegSetValueExW(key,name,0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),static_cast<DWORD>((value.size()+1)*sizeof(wchar_t))); RegCloseKey(key); return HRESULT_FROM_WIN32(error);
}
void RemoveCom() { // Exact owned registry keys only; no recursive registry or filesystem deletion.
    auto base = ClassPath(); RegDeleteKeyW(HKEY_LOCAL_MACHINE,(base+L"\\InprocServer32").c_str()); RegDeleteKeyW(HKEY_LOCAL_MACHINE,base.c_str());
}
}
extern "C" BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) { if (reason == DLL_PROCESS_ATTACH) penda::module = instance; return TRUE; }
extern "C" HRESULT __stdcall DllCanUnloadNow() { return penda::objects.load() == 0 ? S_OK : S_FALSE; }
extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID clsid,REFIID iid,void** out) {
    if (!out) return E_POINTER; *out = nullptr; if (clsid != penda::ServiceId) return CLASS_E_CLASSNOTAVAILABLE;
    auto factory = new(std::nothrow) penda::Factory(); if (!factory) return E_OUTOFMEMORY; auto hr = factory->QueryInterface(iid,out); factory->Release(); return hr;
}
extern "C" HRESULT __stdcall DllRegisterServer() {
    using namespace penda;
    const auto initialized = CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return initialized;
    HRESULT hr = E_FAIL;
    try {
        wchar_t path[32768]{}; GetModuleFileNameW(module,path,32768); auto base = ClassPath();
        hr = SetValue(base,nullptr,L"Penda TSF 三语输入");
        if (SUCCEEDED(hr)) hr = SetValue(base+L"\\InprocServer32",nullptr,path);
        if (SUCCEEDED(hr)) hr = SetValue(base+L"\\InprocServer32",L"ThreadingModel",L"Apartment");
        ComPtr<ITfInputProcessorProfiles> profiles; ComPtr<ITfCategoryMgr> categories;
        if (SUCCEEDED(hr)) hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&profiles));
        if (SUCCEEDED(hr)) hr = profiles->Register(ServiceId);
        constexpr wchar_t description[] = L"Penda 中日英";
        if (SUCCEEDED(hr)) hr = profiles->AddLanguageProfile(ServiceId,ProfileLanguage,ProfileId,description,static_cast<ULONG>(std::size(description)-1),path,static_cast<ULONG>(wcslen(path)),0);
        if (SUCCEEDED(hr)) hr = profiles->EnableLanguageProfile(ServiceId,ProfileLanguage,ProfileId,TRUE);
        if (SUCCEEDED(hr)) hr = CoCreateInstance(CLSID_TF_CategoryMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&categories));
        if (SUCCEEDED(hr)) hr = categories->RegisterCategory(ServiceId,GUID_TFCAT_TIP_KEYBOARD,ServiceId);
        if (SUCCEEDED(hr)) hr = categories->RegisterCategory(ServiceId,GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,ServiceId);
        if (SUCCEEDED(hr)) hr = categories->RegisterCategory(ServiceId,GUID_TFCAT_TIPCAP_UIELEMENTENABLED,ServiceId);
        if (SUCCEEDED(hr)) hr = categories->RegisterCategory(ServiceId,GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,ServiceId);
        if (FAILED(hr)) RemoveCom();
    } catch (...) { hr = E_FAIL; }
    if (SUCCEEDED(initialized)) CoUninitialize(); return hr;
}
extern "C" HRESULT __stdcall DllUnregisterServer() {
    using namespace penda;
    const auto initialized = CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return initialized;
    ComPtr<ITfCategoryMgr> categories; ComPtr<ITfInputProcessorProfiles> profiles;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&categories));
    if (SUCCEEDED(hr)) { categories->UnregisterCategory(ServiceId,GUID_TFCAT_TIP_KEYBOARD,ServiceId); categories->UnregisterCategory(ServiceId,GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,ServiceId); categories->UnregisterCategory(ServiceId,GUID_TFCAT_TIPCAP_UIELEMENTENABLED,ServiceId); categories->UnregisterCategory(ServiceId,GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,ServiceId); }
    hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&profiles));
    if (SUCCEEDED(hr)) { profiles->RemoveLanguageProfile(ServiceId,ProfileLanguage,ProfileId); profiles->Unregister(ServiceId); }
    RemoveCom(); if (SUCCEEDED(initialized)) CoUninitialize(); return hr;
}
