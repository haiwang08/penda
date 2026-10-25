#pragma once
#include "TextService.h"
#include <algorithm>
namespace penda {
// Snapshot exposed to Windows and UI-less hosts; callbacks are detached before teardown.
class CandidateUi final : public ITfCandidateListUIElementBehavior {
    std::atomic<ULONG> refs_{1};
public:
    std::vector<std::wstring> strings;std::vector<int> indices;std::vector<UINT> pages;
    ComPtr<ITfDocumentMgr> document;UINT selection=0;BOOL shown=FALSE;
    std::function<HRESULT(UINT)> select;std::function<HRESULT()> finalize,abort;std::function<void(BOOL)> visibility;
    CandidateUi(){++objects;}~CandidateUi(){--objects;}
    void Detach(){select={};finalize={};abort={};visibility={};document.Reset();shown=FALSE;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override {if(!out)return E_POINTER;*out=nullptr;if(iid!=IID_IUnknown&&iid!=IID_ITfUIElement&&iid!=IID_ITfCandidateListUIElement&&iid!=IID_ITfCandidateListUIElementBehavior)return E_NOINTERFACE;*out=static_cast<ITfCandidateListUIElementBehavior*>(this);AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs_;}ULONG STDMETHODCALLTYPE Release() override{auto n=--refs_;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE GetDescription(BSTR* text) override{if(!text)return E_POINTER;*text=SysAllocString(L"Penda 候选词");return *text?S_OK:E_OUTOFMEMORY;}
    HRESULT STDMETHODCALLTYPE GetGUID(GUID* id) override{if(!id)return E_POINTER;*id={0xd7273dd7,0xe70f,0x49c2,{0xaf,0x41,0x91,0xbc,0x22,0x2f,0x78,0x63}};return S_OK;}
    HRESULT STDMETHODCALLTYPE Show(BOOL show) override{shown=show;if(visibility)visibility(show);return S_OK;}
    HRESULT STDMETHODCALLTYPE IsShown(BOOL* show) override{if(!show)return E_POINTER;*show=shown;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetUpdatedFlags(DWORD* flags) override{if(!flags)return E_POINTER;*flags=TF_CLUIE_DOCUMENTMGR|TF_CLUIE_COUNT|TF_CLUIE_SELECTION|TF_CLUIE_STRING|TF_CLUIE_PAGEINDEX|TF_CLUIE_CURRENTPAGE;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetDocumentMgr(ITfDocumentMgr** out) override{if(!out)return E_POINTER;*out=document.Get();if(*out)(*out)->AddRef();return *out?S_OK:S_FALSE;}
    HRESULT STDMETHODCALLTYPE GetCount(UINT* count) override{if(!count)return E_POINTER;*count=static_cast<UINT>(strings.size());return S_OK;}
    HRESULT STDMETHODCALLTYPE GetSelection(UINT* index) override{if(!index)return E_POINTER;*index=selection;return selection<strings.size()?S_OK:S_FALSE;}
    HRESULT STDMETHODCALLTYPE GetString(UINT index,BSTR* text) override{if(!text)return E_POINTER;*text=nullptr;if(index>=strings.size())return E_INVALIDARG;*text=SysAllocString(strings[index].c_str());return *text?S_OK:E_OUTOFMEMORY;}
    HRESULT STDMETHODCALLTYPE GetPageIndex(UINT* values,UINT size,UINT* count) override{if(!count||(!values&&size))return E_POINTER;*count=static_cast<UINT>(pages.size());std::copy_n(pages.begin(),std::min<size_t>(size,pages.size()),values);return size<pages.size()?S_FALSE:S_OK;}
    HRESULT STDMETHODCALLTYPE SetPageIndex(UINT* values,UINT count) override{if(!values||!count)return E_INVALIDARG;if(values[0]!=0)return E_INVALIDARG;for(UINT i=0;i<count;++i)if(values[i]>=strings.size()||(i&&values[i]<=values[i-1]))return E_INVALIDARG;pages.assign(values,values+count);return S_OK;}
    HRESULT STDMETHODCALLTYPE GetCurrentPage(UINT* page) override{if(!page)return E_POINTER;auto it=std::upper_bound(pages.begin(),pages.end(),selection);*page=it==pages.begin()?0:static_cast<UINT>(it-pages.begin()-1);return S_OK;}
    HRESULT STDMETHODCALLTYPE SetSelection(UINT index) override{if(index>=strings.size())return E_INVALIDARG;return select?select(index):E_UNEXPECTED;}
    HRESULT STDMETHODCALLTYPE Finalize() override{return finalize?finalize():E_UNEXPECTED;}
    HRESULT STDMETHODCALLTYPE Abort() override{return abort?abort():E_UNEXPECTED;}
};
}
