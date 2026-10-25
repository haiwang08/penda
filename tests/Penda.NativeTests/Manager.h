#pragma once
#include "../../src/Penda.Tsf/TextService.h"

// Only key-sink installation is substituted: Windows accepts it only for an OS-activated TIP.
// All contexts, text stores, locks, compositions, edit sessions and event sources remain real TSF.
// This deliberately does NOT test installed-profile activation or physical keyboard routing.
class TestManager final : public ITfThreadMgr, public ITfKeystrokeMgr, public ITfLangBarItemMgr {
    ULONG refs_ = 1;
    penda::ComPtr<ITfThreadMgr> real_;
    penda::ComPtr<ITfKeyEventSink> sink_;
    penda::ComPtr<ITfLangBarItem> item_;
public:
    explicit TestManager(ITfThreadMgr* real) : real_(real) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfThreadMgr) *out = static_cast<ITfThreadMgr*>(this);
        else if (iid == IID_ITfKeystrokeMgr) *out = static_cast<ITfKeystrokeMgr*>(this);
        else if (iid == IID_ITfLangBarItemMgr) *out = static_cast<ITfLangBarItemMgr*>(this);
        else return real_->QueryInterface(iid,out);
        AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { auto count = --refs_; if (!count) delete this; return count; }
    // Keep manually activated test items separate from the user's OS-activated IME item.
    HRESULT STDMETHODCALLTYPE EnumItems(IEnumTfLangBarItems**) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetItem(REFGUID guid,ITfLangBarItem** out) override { if(!out)return E_POINTER;*out=nullptr;if(!item_)return E_FAIL;TF_LANGBARITEMINFO info{};item_->GetInfo(&info);if(info.guidItem!=guid)return E_FAIL;*out=item_.Get();(*out)->AddRef();return S_OK;}
    HRESULT STDMETHODCALLTYPE AddItem(ITfLangBarItem* item) override{if(item_)return E_FAIL;item_=item;return S_OK;}
    HRESULT STDMETHODCALLTYPE RemoveItem(ITfLangBarItem* item) override{if(item!=item_.Get())return E_FAIL;item_.Reset();return S_OK;}
    HRESULT STDMETHODCALLTYPE AdviseItemSink(ITfLangBarItemSink*,DWORD*,REFGUID) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE UnadviseItemSink(DWORD) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetItemFloatingRect(DWORD,REFGUID,RECT*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetItemsStatus(ULONG,const GUID*,DWORD*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetItemNum(ULONG* count) override{if(!count)return E_POINTER;*count=item_?1:0;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetItems(ULONG,ITfLangBarItem**,TF_LANGBARITEMINFO*,DWORD*,ULONG*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE AdviseItemsSink(ULONG,ITfLangBarItemSink**,const GUID*,DWORD*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE UnadviseItemsSink(ULONG,DWORD*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Activate(TfClientId* client) override { return real_->Activate(client); }
    HRESULT STDMETHODCALLTYPE Deactivate() override { return real_->Deactivate(); }
    HRESULT STDMETHODCALLTYPE CreateDocumentMgr(ITfDocumentMgr** document) override { return real_->CreateDocumentMgr(document); }
    HRESULT STDMETHODCALLTYPE EnumDocumentMgrs(IEnumTfDocumentMgrs** documents) override { return real_->EnumDocumentMgrs(documents); }
    HRESULT STDMETHODCALLTYPE GetFocus(ITfDocumentMgr** document) override { return real_->GetFocus(document); }
    HRESULT STDMETHODCALLTYPE SetFocus(ITfDocumentMgr* document) override { return real_->SetFocus(document); }
    HRESULT STDMETHODCALLTYPE AssociateFocus(HWND window,ITfDocumentMgr* next,ITfDocumentMgr** previous) override { return real_->AssociateFocus(window,next,previous); }
    HRESULT STDMETHODCALLTYPE IsThreadFocus(BOOL* focus) override { return real_->IsThreadFocus(focus); }
    HRESULT STDMETHODCALLTYPE GetFunctionProvider(REFCLSID clsid,ITfFunctionProvider** provider) override { return real_->GetFunctionProvider(clsid,provider); }
    HRESULT STDMETHODCALLTYPE EnumFunctionProviders(IEnumTfFunctionProviders** providers) override { return real_->EnumFunctionProviders(providers); }
    HRESULT STDMETHODCALLTYPE GetGlobalCompartment(ITfCompartmentMgr** compartments) override { return real_->GetGlobalCompartment(compartments); }
    HRESULT STDMETHODCALLTYPE AdviseKeyEventSink(TfClientId,ITfKeyEventSink* sink,BOOL) override { sink_ = sink; return S_OK; }
    HRESULT STDMETHODCALLTYPE UnadviseKeyEventSink(TfClientId) override { sink_.Reset(); return S_OK; }
    HRESULT STDMETHODCALLTYPE GetForeground(CLSID* clsid) override { *clsid = penda::ServiceId; return S_OK; }
    HRESULT STDMETHODCALLTYPE TestKeyDown(WPARAM,LPARAM,BOOL*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE TestKeyUp(WPARAM,LPARAM,BOOL*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE KeyDown(WPARAM,LPARAM,BOOL*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE KeyUp(WPARAM,LPARAM,BOOL*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetPreservedKey(ITfContext*,const TF_PRESERVEDKEY*,GUID*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE IsPreservedKey(REFGUID,const TF_PRESERVEDKEY*,BOOL*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE PreserveKey(TfClientId,REFGUID,const TF_PRESERVEDKEY*,const WCHAR*,ULONG) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE UnpreserveKey(REFGUID,const TF_PRESERVEDKEY*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPreservedKeyDescription(REFGUID,const WCHAR*,ULONG) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetPreservedKeyDescription(REFGUID,BSTR*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SimulatePreservedKey(ITfContext*,REFGUID,BOOL*) override { return E_NOTIMPL; }
};
