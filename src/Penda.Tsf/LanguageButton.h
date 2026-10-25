#pragma once
#include "TextService.h"
#include <ctfutb.h>
#include <ctffunc.h>
#include <filesystem>
#include <fstream>
#include <shellapi.h>
#include <olectl.h>
namespace penda {
inline std::wstring PriorityPath() {
    wchar_t test[32768]{}; if(GetEnvironmentVariableW(L"PENDA_PREFERENCE_PATH",test,32768)) return test;
    return (std::filesystem::path(UserDataDirectory())/L"preferred-language.txt").wstring();
}
inline int ReadPriority() { std::ifstream file{std::filesystem::path(PriorityPath())}; int value=0; file>>value; return value>=0&&value<3 ? value : 0; }
inline bool WritePriority(int value) {
    try {
        auto path=PriorityPath(); std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        auto temp=path+L"."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetCurrentThreadId())+L".tmp";
        { std::ofstream file(std::filesystem::path(temp),std::ios::trunc); file<<value; file.close(); if(!file) return false; }
        return MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    } catch(...) { return false; }
}
inline const GUID& MenuId=GUID_LBI_INPUTMODE;
class SettingsFunction final : public ITfFnConfigure, public ITfFnGetPreferredTouchKeyboardLayout {
    std::atomic<ULONG> refs_{1};
public:
    SettingsFunction(){++objects;} ~SettingsFunction(){--objects;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override { if(!out)return E_POINTER;*out=nullptr;if(id==IID_ITfFnGetPreferredTouchKeyboardLayout)*out=static_cast<ITfFnGetPreferredTouchKeyboardLayout*>(this);else if(id==IID_IUnknown||id==IID_ITfFunction||id==IID_ITfFnConfigure)*out=static_cast<ITfFnConfigure*>(this);else return E_NOINTERFACE;AddRef();return S_OK; }
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs_;} ULONG STDMETHODCALLTYPE Release() override{auto n=--refs_;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE GetDisplayName(BSTR* text) override {if(!text)return E_POINTER;*text=SysAllocString(L"Penda 设置与词库");return *text?S_OK:E_OUTOFMEMORY;}
    HRESULT STDMETHODCALLTYPE GetLayout(TKBLayoutType* type,WORD* layout) override {if(!type||!layout)return E_POINTER;*type=TKBLT_CLASSIC;*layout=0;return S_OK;}
    HRESULT STDMETHODCALLTYPE Show(HWND parent,LANGID language,REFGUID profile) override {
        if((language!=0 && language!=ProfileLanguage)||(profile!=GUID_NULL && profile!=ProfileId))return E_INVALIDARG;
        auto exe=(std::filesystem::path(ModuleDirectory()).parent_path()/L"settings"/L"Penda.SettingsHost.exe").wstring();
        auto result=ShellExecuteW(parent,L"open",exe.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        return reinterpret_cast<INT_PTR>(result)>32?S_OK:E_FAIL;
    }
};
class LanguageButton final : public ITfLangBarItemButton, public ITfSource {
    std::atomic<ULONG> refs_{1}; ComPtr<ITfLangBarItemSink> sink_; DWORD status_=0;
public:
    LanguageButton() { ++objects; } ~LanguageButton() { --objects; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override { if(!out)return E_POINTER; *out=nullptr; if(id==IID_IUnknown||id==IID_ITfLangBarItem||id==IID_ITfLangBarItemButton)*out=static_cast<ITfLangBarItemButton*>(this); else if(id==IID_ITfSource)*out=static_cast<ITfSource*>(this); else return E_NOINTERFACE; AddRef();return S_OK; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; } ULONG STDMETHODCALLTYPE Release() override { auto n=--refs_;if(!n)delete this;return n; }
    HRESULT STDMETHODCALLTYPE GetInfo(TF_LANGBARITEMINFO* info) override { if(!info)return E_POINTER; *info={};info->clsidService=ServiceId;info->guidItem=MenuId;info->dwStyle=TF_LBI_STYLE_BTN_BUTTON|TF_LBI_STYLE_BTN_MENU|TF_LBI_STYLE_SHOWNINTRAY;wcscpy_s(info->szDescription,L"Penda 设置与首选语言");return S_OK; }
    HRESULT STDMETHODCALLTYPE GetStatus(DWORD* value) override {if(!value)return E_POINTER;*value=status_;return S_OK;}
    HRESULT STDMETHODCALLTYPE Show(BOOL show) override {status_=show?0:TF_LBI_STATUS_HIDDEN;if(sink_)sink_->OnUpdate(TF_LBI_STATUS);return S_OK;}
    HRESULT STDMETHODCALLTYPE GetTooltipString(BSTR* value) override {return GetText(value);}
    HRESULT STDMETHODCALLTYPE GetText(BSTR* value) override {if(!value)return E_POINTER;*value=SysAllocString(L"Penda 设置与首选语言");return *value?S_OK:E_OUTOFMEMORY;}
    HRESULT STDMETHODCALLTYPE GetIcon(HICON* icon) override {if(!icon)return E_POINTER;*icon=static_cast<HICON>(LoadImageW(module,MAKEINTRESOURCEW(101),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),0));return *icon?S_OK:E_FAIL;}
    HRESULT STDMETHODCALLTYPE OnClick(TfLBIClick,POINT point,const RECT*) override {
        HMENU menu=CreatePopupMenu(); if(!menu)return E_OUTOFMEMORY;
        AppendMenuW(menu,MF_STRING,1,L"设置与词库…"); AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
        const wchar_t* labels[]={L"中文排在第一行",L"日语排在第一行",L"英语排在第一行"};
        for(int i=0;i<3;++i)AppendMenuW(menu,MF_STRING|(ReadPriority()==i?MF_CHECKED:0),10+i,labels[i]);
        // The foreground window can belong to Explorer. A popup menu needs an owner on this thread.
        auto window=CreateWindowExW(WS_EX_TOOLWINDOW,L"STATIC",L"Penda.Tsf.MenuOwner",WS_POPUP,0,0,0,0,nullptr,nullptr,module,nullptr);
        if(!window) { DestroyMenu(menu); return HRESULT_FROM_WIN32(GetLastError()); }
        SetForegroundWindow(window);
        auto id=TrackPopupMenuEx(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,point.x,point.y,window,nullptr);
        PostMessageW(window,WM_NULL,0,0); DestroyMenu(menu); DestroyWindow(window);
        return id?OnMenuSelect(id):S_OK;
    }
    HRESULT STDMETHODCALLTYPE InitMenu(ITfMenu* menu) override {
        if(!menu)return E_POINTER; const wchar_t* labels[]={L"中文排在第一行",L"日语排在第一行",L"英语排在第一行"};
        HRESULT hr=menu->AddMenuItem(1,0,nullptr,nullptr,L"设置与词库…",6,nullptr);if(FAILED(hr))return hr;
        for(int i=0;i<3;++i){hr=menu->AddMenuItem(10+i,ReadPriority()==i?TF_LBMENUF_CHECKED:0,nullptr,nullptr,labels[i],static_cast<ULONG>(wcslen(labels[i])),nullptr);if(FAILED(hr))return hr;}return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnMenuSelect(UINT id) override {
        if(id>=10&&id<=12) return WritePriority(id-10)?S_OK:E_FAIL;
        if(id!=1)return E_INVALIDARG;
        auto exe=(std::filesystem::path(ModuleDirectory()).parent_path()/L"settings"/L"Penda.SettingsHost.exe").wstring();
        auto result=ShellExecuteW(nullptr,L"open",exe.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        if(reinterpret_cast<INT_PTR>(result)<=32){MessageBoxW(nullptr,L"无法打开 Penda 设置，请检查安装目录中的 settings\\Penda.SettingsHost.exe。",L"Penda",MB_OK|MB_ICONERROR);return E_FAIL;}return S_OK;
    }
    HRESULT STDMETHODCALLTYPE AdviseSink(REFIID id,IUnknown* unknown,DWORD* cookie) override {if(!cookie||!unknown)return E_POINTER;if(id!=IID_ITfLangBarItemSink)return CONNECT_E_CANNOTCONNECT;if(sink_)return CONNECT_E_ADVISELIMIT;auto hr=unknown->QueryInterface(IID_PPV_ARGS(&sink_));if(SUCCEEDED(hr))*cookie=1;return hr;}
    HRESULT STDMETHODCALLTYPE UnadviseSink(DWORD cookie) override {if(cookie!=1||!sink_)return CONNECT_E_NOCONNECTION;sink_.Reset();return S_OK;}
};
}
