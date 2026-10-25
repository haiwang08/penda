#pragma once
#include "Engine.h"
#include <msctf.h>
#include <ctfutb.h>
#include <ctffunc.h>
#include <wrl/client.h>
#include <atomic>
#include <functional>
#include <memory>

namespace penda {
using Microsoft::WRL::ComPtr;
inline constexpr GUID ServiceId = {0x245a7fc9,0x923c,0x4c9c,{0xa8,0x63,0x86,0x02,0xa8,0x3c,0x37,0x52}};
inline constexpr GUID ProfileId = {0xd15542b2,0x92e7,0x48ee,{0xb5,0xca,0x38,0x93,0x3a,0xa6,0x2e,0x9c}};
inline constexpr LANGID ProfileLanguage = 0x0804;
extern std::atomic<long> objects;

class CandidateUi;
class TextService final : public ITfTextInputProcessorEx, public ITfKeyEventSink, public ITfCompositionSink,
    public ITfFunctionProvider, public ITfThreadMgrEventSink, public ITfThreadFocusSink, public ITfTextLayoutSink, public ITfTextEditSink {
public:
    TextService();
    ~TextService();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override;
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override { auto count = --references_; if (!count) delete this; return count; }
    HRESULT STDMETHODCALLTYPE Activate(ITfThreadMgr* manager, TfClientId client) override { return ActivateEx(manager, client, 0); }
    HRESULT STDMETHODCALLTYPE ActivateEx(ITfThreadMgr*, TfClientId, DWORD) override;
    HRESULT STDMETHODCALLTYPE Deactivate() override;
    HRESULT STDMETHODCALLTYPE GetType(GUID*) override;
    HRESULT STDMETHODCALLTYPE GetDescription(BSTR*) override;
    HRESULT STDMETHODCALLTYPE GetFunction(REFGUID,REFIID,IUnknown**) override;
    HRESULT STDMETHODCALLTYPE OnSetFocus(BOOL foreground) override;
    HRESULT STDMETHODCALLTYPE OnTestKeyDown(ITfContext*, WPARAM, LPARAM, BOOL*) override;
    HRESULT STDMETHODCALLTYPE OnTestKeyUp(ITfContext*, WPARAM, LPARAM, BOOL*) override;
    HRESULT STDMETHODCALLTYPE OnKeyDown(ITfContext*, WPARAM, LPARAM, BOOL*) override;
    HRESULT STDMETHODCALLTYPE OnKeyUp(ITfContext*, WPARAM, LPARAM, BOOL*) override;
    HRESULT STDMETHODCALLTYPE OnPreservedKey(ITfContext*, REFGUID, BOOL* eaten) override { *eaten = FALSE; return S_OK; }
    HRESULT STDMETHODCALLTYPE OnCompositionTerminated(TfEditCookie, ITfComposition*) override;
    HRESULT STDMETHODCALLTYPE OnInitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnUninitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnSetFocus(ITfDocumentMgr*, ITfDocumentMgr*) override;
    HRESULT STDMETHODCALLTYPE OnPushContext(ITfContext*) override;
    HRESULT STDMETHODCALLTYPE OnPopContext(ITfContext*) override;
    HRESULT STDMETHODCALLTYPE OnSetThreadFocus() override;
    HRESULT STDMETHODCALLTYPE OnKillThreadFocus() override;
    HRESULT STDMETHODCALLTYPE OnLayoutChange(ITfContext*, TfLayoutCode, ITfContextView*) override;
    HRESULT STDMETHODCALLTYPE OnEndEdit(ITfContext*, TfEditCookie, ITfEditRecord*) override;
private:
    std::atomic<ULONG> references_{1};
    ComPtr<ITfThreadMgr> manager_;
    ComPtr<ITfLangBarItem> languageButton_;
    ComPtr<ITfUIElementMgr> uiManager_;
    ComPtr<CandidateUi> candidateUi_;
    DWORD uiId_=static_cast<DWORD>(-1);
    bool uiAllowed_=true, uiStarting_=false;
    void UpdateUi();
    void EndUi();
    int priority_ = -1;
    std::array<int,3> languageOrder_{0,1,2};
    ComPtr<ITfContext> context_, watched_;
    ComPtr<ITfComposition> composition_;
    TfClientId client_ = TF_CLIENTID_NULL;
    DWORD threadCookie_ = TF_INVALID_COOKIE, focusCookie_ = TF_INVALID_COOKIE, layoutCookie_ = TF_INVALID_COOKIE, editCookie_ = TF_INVALID_COOKIE;
    bool functionProvider_ = false;
    bool keySink_ = false, enabled_ = true, inEdit_ = false;
    std::array<bool,256> eatenKeys_{};
    std::shared_ptr<const Engine> engine_;
    unsigned languages_ = 7;
    const Engine* lastEngine_ = nullptr;
    std::wstring input_;
    std::wstring confirmed_, confirmedKey_;
    std::vector<std::pair<std::wstring,std::wstring>> confirmedSegments_;
    std::vector<Candidate> candidates_;
    int selected_ = 0;
    int page_ = 0;
    int activeLanguage_ = 0;
    bool doubleQuote_ = false, singleQuote_ = false;
    HWND popup_ = nullptr;
    unsigned epoch_ = 0;
    int dpi_ = 96;
    unsigned layoutRetries_ = 0;
    HRESULT Request(ITfContext*, DWORD, std::function<HRESULT(TfEditCookie)>);
    bool Writable(ITfContext*);
    bool WantsKey(ITfContext*, WPARAM);
    HRESULT Key(ITfContext*, TfEditCookie, WPARAM, bool, bool, bool);
    HRESULT UpdateText(ITfContext*, TfEditCookie);
    HRESULT Finish(TfEditCookie, const std::wstring&);
    HRESULT Accept(TfEditCookie,int,const std::wstring& suffix = L"");
    void Reset();
    void EndPending();
    void Watch(ITfContext*);
    void RefreshEngine();
    void RefreshCandidates();
    void Present(TfEditCookie);
    void Paint();
    void Pick(int);
    RECT CellRect(int language, int row, int column) const;
    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
};
}
