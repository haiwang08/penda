#pragma once
#include <windows.h>
#include <textstor.h>
#include <wrl/client.h>
#include <string>
#include <algorithm>

// In-memory TSF text store used only by integration tests. It grants real TSF edit locks.
class Store final : public ITextStoreACP {
    ULONG refs_ = 1; Microsoft::WRL::ComPtr<ITextStoreACPSink> sink_;
    DWORD lock_ = 0, pending_ = 0;
public:
    std::wstring text;
    TS_SELECTION_ACP selection{0,0,{TS_AE_NONE,FALSE}};
    bool readOnly = false;
    HWND window = nullptr;
    bool noLayout = false;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override { *out = nullptr; if (iid != IID_IUnknown && iid != IID_ITextStoreACP) return E_NOINTERFACE; *out = static_cast<ITextStoreACP*>(this); AddRef(); return S_OK; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { auto count = --refs_; if (!count) delete this; return count; }
    HRESULT STDMETHODCALLTYPE AdviseSink(REFIID iid,IUnknown* unknown,DWORD) override { if (iid != IID_ITextStoreACPSink) return E_NOINTERFACE; return unknown->QueryInterface(IID_PPV_ARGS(&sink_)); }
    HRESULT STDMETHODCALLTYPE UnadviseSink(IUnknown*) override { sink_.Reset(); return S_OK; }
    HRESULT STDMETHODCALLTYPE RequestLock(DWORD flags,HRESULT* result) override {
        if (!sink_) return E_UNEXPECTED;
        if (lock_) { if (flags & TS_LF_SYNC) *result = TS_E_SYNCHRONOUS; else { pending_ = flags; *result = TS_S_ASYNC; } return S_OK; }
        lock_ = flags; *result = sink_->OnLockGranted(flags); lock_ = 0;
        while (pending_) { auto next = pending_; pending_ = 0; lock_ = next; sink_->OnLockGranted(next); lock_ = 0; }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetStatus(TS_STATUS* status) override { *status = {readOnly ? static_cast<DWORD>(TS_SD_READONLY) : 0u,TS_SS_NOHIDDENTEXT}; return S_OK; }
    HRESULT STDMETHODCALLTYPE QueryInsert(LONG start,LONG end,ULONG,LONG* outStart,LONG* outEnd) override { *outStart = start; *outEnd = end; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetSelection(ULONG,ULONG count,TS_SELECTION_ACP* output,ULONG* fetched) override { *fetched = count ? 1 : 0; if (count) *output = selection; return S_OK; }
    HRESULT STDMETHODCALLTYPE SetSelection(ULONG count,const TS_SELECTION_ACP* value) override { if (!count) return E_INVALIDARG; selection = value[0]; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetText(LONG start,LONG end,WCHAR* plain,ULONG capacity,ULONG* plainCount,TS_RUNINFO* runs,ULONG runCapacity,ULONG* runCount,LONG* next) override {
        if (end == -1) end = static_cast<LONG>(text.size());
        if (start < 0 || end < start || end > static_cast<LONG>(text.size())) return TS_E_INVALIDPOS;
        auto count = plain ? std::min<ULONG>(end-start,capacity) : static_cast<ULONG>(end-start);
        if (plain && count) std::copy_n(text.data()+start,count,plain);
        if (plainCount) *plainCount = plain ? count : 0;
        if (runCount) *runCount = runCapacity && count ? 1 : 0;
        if (runs && runCapacity && count) *runs = {count,TS_RT_PLAIN};
        if (next) *next = start + count;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetText(DWORD,LONG start,LONG end,const WCHAR* value,ULONG count,TS_TEXTCHANGE* change) override {
        if (readOnly) return TS_E_READONLY;
        if (start < 0 || end < start || end > static_cast<LONG>(text.size())) return TS_E_INVALIDPOS;
        text.replace(start,end-start,value ? std::wstring(value,count) : L""); *change = {start,end,start+static_cast<LONG>(count)}; selection = {change->acpNewEnd,change->acpNewEnd,{TS_AE_NONE,FALSE}}; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE InsertTextAtSelection(DWORD flags,const WCHAR* value,ULONG count,LONG* start,LONG* end,TS_TEXTCHANGE* change) override {
        if (start) *start = selection.acpStart; if (end) *end = selection.acpEnd;
        if (flags & TS_IAS_QUERYONLY) return S_OK;
        auto hr = SetText(0,selection.acpStart,selection.acpEnd,value,count,change); if (end) *end = change->acpNewEnd; return hr;
    }
    HRESULT STDMETHODCALLTYPE GetFormattedText(LONG,LONG,IDataObject**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetEmbedded(LONG,REFGUID,REFIID,IUnknown**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE QueryInsertEmbedded(const GUID*,const FORMATETC*,BOOL* insertable) override { *insertable = FALSE; return S_OK; }
    HRESULT STDMETHODCALLTYPE InsertEmbedded(DWORD,LONG,LONG,IDataObject*,TS_TEXTCHANGE*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE InsertEmbeddedAtSelection(DWORD,IDataObject*,LONG*,LONG*,TS_TEXTCHANGE*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE RequestSupportedAttrs(DWORD,ULONG,const TS_ATTRID*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE RequestAttrsAtPosition(LONG,ULONG,const TS_ATTRID*,DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE RequestAttrsTransitioningAtPosition(LONG,ULONG,const TS_ATTRID*,DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE FindNextAttrTransition(LONG,LONG halt,ULONG,const TS_ATTRID*,DWORD,LONG* next,BOOL* found,LONG* offset) override { *next = halt; *found = FALSE; *offset = 0; return S_OK; }
    HRESULT STDMETHODCALLTYPE RetrieveRequestedAttrs(ULONG,TS_ATTRVAL*,ULONG* count) override { *count = 0; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetEndACP(LONG* end) override { *end = static_cast<LONG>(text.size()); return S_OK; }
    HRESULT STDMETHODCALLTYPE GetActiveView(TsViewCookie* view) override { *view = 1; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetACPFromPoint(TsViewCookie,const POINT*,DWORD,LONG* position) override { *position = selection.acpEnd; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetTextExt(TsViewCookie,LONG start,LONG end,RECT* rect,BOOL* clipped) override { if (noLayout) return TS_E_NOLAYOUT; *rect = {100+start*8,100,101+end*8,125}; *clipped = FALSE; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetScreenExt(TsViewCookie,RECT* rect) override { *rect = {0,0,1200,800}; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetWnd(TsViewCookie,HWND* hwnd) override { *hwnd = window; return S_OK; }
};
