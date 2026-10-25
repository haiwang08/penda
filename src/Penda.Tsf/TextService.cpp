#include "TextService.h"
#include "CandidatePanel.h"
#include "Learning.h"
#include "LanguageButton.h"
#include "CandidateUi.h"
#include <initguid.h>
#include <inputscope.h>
#include <algorithm>
#include <mutex>
#include <new>

namespace penda {
namespace {
class Session final : public ITfEditSession {
    std::atomic<ULONG> refs_{1}; TextService* owner_; ComPtr<ITfContext> context_;
    std::function<HRESULT(TfEditCookie)> action_;
public:
    Session(TextService* owner, ITfContext* context, std::function<HRESULT(TfEditCookie)> action) : owner_(owner), context_(context), action_(std::move(action)) { owner_->AddRef(); ++objects; }
    ~Session() { owner_->Release(); --objects; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override { if (!out) return E_POINTER; *out = nullptr; if (iid != IID_IUnknown && iid != IID_ITfEditSession) return E_NOINTERFACE; *out = static_cast<ITfEditSession*>(this); AddRef(); return S_OK; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { auto count = --refs_; if (!count) delete this; return count; }
    HRESULT STDMETHODCALLTYPE DoEditSession(TfEditCookie cookie) override { try { return action_(cookie); } catch (...) { return E_FAIL; } }
};
bool Down(int key) { return (GetKeyState(key) & 0x8000) != 0; }
bool Compartment(ITfContext* context, REFGUID guid) {
    ComPtr<ITfCompartmentMgr> manager; ComPtr<ITfCompartment> compartment;
    if (FAILED(context->QueryInterface(IID_PPV_ARGS(&manager))) || FAILED(manager->GetCompartment(guid, &compartment))) return false;
    VARIANT value; VariantInit(&value); bool result = SUCCEEDED(compartment->GetValue(&value)) && value.vt == VT_I4 && value.lVal != 0; VariantClear(&value); return result;
}
std::wstring Punctuation(WPARAM key, bool shift) {
    switch (key) {
    case VK_OEM_1: return shift ? L":" : L";"; case VK_OEM_PLUS: return shift ? L"+" : L"=";
    case VK_OEM_COMMA: return shift ? L"<" : L","; case VK_OEM_MINUS: return shift ? L"_" : L"-";
    case VK_OEM_PERIOD: return shift ? L">" : L"."; case VK_OEM_2: return shift ? L"?" : L"/";
    case VK_OEM_3: return shift ? L"~" : L"`"; case VK_OEM_4: return shift ? L"{" : L"[";
    case VK_OEM_5: return shift ? L"|" : L"\\"; case VK_OEM_6: return shift ? L"}" : L"]";
    case VK_OEM_7: return shift ? L"\"" : L"'";
    default: if (shift && key >= L'0' && key <= L'9') return std::wstring(1, L")!@#$%^&*("[key - L'0']); return {};
    }
}
std::wstring ChineseSymbol(std::wstring value,bool& doubleQuote,bool& singleQuote) {
    if(value==L",") return L"，"; if(value==L".") return L"。"; if(value==L"?") return L"？"; if(value==L"!") return L"！";
    if(value==L";") return L"；"; if(value==L":") return L"："; if(value==L"(") return L"（"; if(value==L")") return L"）";
    if(value==L"[") return L"【"; if(value==L"]") return L"】"; if(value==L"<") return L"《"; if(value==L">") return L"》"; if(value==L"/") return L"、";
    if(value==L"\"") { doubleQuote=!doubleQuote; return doubleQuote ? L"“" : L"”"; }
    if(value==L"'") { singleQuote=!singleQuote; return singleQuote ? L"‘" : L"’"; }
    return value;
}
std::wstring JapaneseSymbol(std::wstring value) {
    if(value==L",")return L"、";if(value==L".")return L"。";
    if(value==L"[")return L"「";if(value==L"]")return L"」";
    if(value==L"{")return L"『";if(value==L"}")return L"』";
    if(value==L"/")return L"・";if(value==L"~")return L"〜";
    for(auto& c:value)if(c>=0x21&&c<=0x7E)c+=0xFEE0;
    return value;
}
struct EngineCache { std::mutex mutex; std::shared_ptr<const Engine> current; FILETIME stamp{}; std::wstring path; bool loading = false; };
EngineCache cache;
// Record only load metadata and a fixed regression probe, never typed text.
void RecordEngineStatus(const Engine& engine,const std::wstring& path) noexcept {
    try {
        wchar_t temp[32768]{};if(!GetTempPathW(32768,temp))return;
        auto folder=std::filesystem::path(temp)/L"Penda-diagnostics";
        std::filesystem::create_directories(folder);
        auto rows=engine.Query(L"ogoru");bool kanji=std::any_of(rows.begin(),rows.end(),[](const Candidate& row){return row.language==1&&row.text==L"奢る";});
        std::wstring physical=path;HANDLE fileHandle=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
        if(fileHandle!=INVALID_HANDLE_VALUE){wchar_t name[32768]{};auto count=GetFinalPathNameByHandleW(fileHandle,name,32768,FILE_NAME_NORMALIZED);if(count&&count<32768)physical=name;CloseHandle(fileHandle);}
        std::wstring report=L"Penda TSF load diagnostic 0.9.8\r\nPID: "+std::to_wstring(GetCurrentProcessId())+L"\r\nDictionary: "+path+L"\r\nPhysical dictionary: "+physical+L"\r\nChinese: "+std::to_wstring(engine.words[0].size())+L"\r\nJapanese: "+std::to_wstring(engine.words[1].size())+L"\r\nEnglish: "+std::to_wstring(engine.words[2].size())+L"\r\nFixed ogoru kanji probe: "+(kanji?L"PASS":L"FAIL")+L"\r\n";
        std::ofstream file(folder/(L"engine-"+std::to_wstring(GetCurrentProcessId())+L".txt"),std::ios::binary|std::ios::trunc);const wchar_t bom=0xFEFF;file.write(reinterpret_cast<const char*>(&bom),2);file.write(reinterpret_cast<const char*>(report.data()),report.size()*2);
    } catch(...) {}
}
struct ReloadJob { std::wstring path; FILETIME stamp; HMODULE pin; };
void CALLBACK ReloadEngine(PTP_CALLBACK_INSTANCE instance, void* parameter) {
    HMODULE pin = nullptr;
    {
        std::unique_ptr<ReloadJob> job(static_cast<ReloadJob*>(parameter)); pin = job->pin;
        try {
            auto next = std::make_shared<Engine>(); const bool loaded = next->Load(job->path);
            std::lock_guard lock(cache.mutex);
            if (loaded) { RecordEngineStatus(*next,job->path); cache.current = next; cache.path = job->path; cache.stamp = job->stamp; }
            cache.loading = false;
        } catch (...) { std::lock_guard lock(cache.mutex); cache.loading = false; }
    }
    --objects;
    FreeLibraryWhenCallbackReturns(instance,pin);
}
std::shared_ptr<const Engine> LoadEngine() {
    std::lock_guard lock(cache.mutex);
    auto path = UserSnapshotPath(); WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) { path = ModuleDirectory() + L"\\engine.pdat"; GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data); }
    if (!cache.current) {
        auto next = std::make_shared<Engine>();
        if (next->Load(path)) { RecordEngineStatus(*next,path); cache.current = next; cache.path = path; cache.stamp = data.ftLastWriteTime; }
        else if (!cache.current) { auto fallback=ModuleDirectory()+L"\\engine.pdat"; next->Load(fallback); RecordEngineStatus(*next,fallback); cache.current = next; }
    } else if (!cache.loading && (cache.path != path || CompareFileTime(&cache.stamp, &data.ftLastWriteTime) != 0)) {
        HMODULE pin = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&ReloadEngine),&pin)) {
            auto job = new(std::nothrow) ReloadJob{path,data.ftLastWriteTime,pin};
            if (job) {
                cache.loading = true; ++objects;
                if (!TrySubmitThreadpoolCallback(ReloadEngine,job,nullptr)) { cache.loading = false; --objects; delete job; FreeLibrary(pin); }
            } else FreeLibrary(pin);
        }
    }
    return cache.current;
}
}
TextService::TextService() { ++objects; }
TextService::~TextService() { Deactivate(); --objects; }
HRESULT TextService::QueryInterface(REFIID iid, void** out) {
    if (!out) return E_POINTER; *out = nullptr;
    if (iid == IID_IUnknown || iid == IID_ITfTextInputProcessor || iid == IID_ITfTextInputProcessorEx) *out = static_cast<ITfTextInputProcessorEx*>(this);
    else if (iid == IID_ITfFunctionProvider) *out = static_cast<ITfFunctionProvider*>(this);
    else if (iid == IID_ITfKeyEventSink) *out = static_cast<ITfKeyEventSink*>(this);
    else if (iid == IID_ITfCompositionSink) *out = static_cast<ITfCompositionSink*>(this);
    else if (iid == IID_ITfThreadMgrEventSink) *out = static_cast<ITfThreadMgrEventSink*>(this);
    else if (iid == IID_ITfThreadFocusSink) *out = static_cast<ITfThreadFocusSink*>(this);
    else if (iid == IID_ITfTextLayoutSink) *out = static_cast<ITfTextLayoutSink*>(this);
    else if (iid == IID_ITfTextEditSink) *out = static_cast<ITfTextEditSink*>(this);
    else return E_NOINTERFACE;
    AddRef(); return S_OK;
}
HRESULT TextService::GetType(GUID* type) { if(!type)return E_POINTER; *type=ServiceId;return S_OK; }
HRESULT TextService::GetDescription(BSTR* text) { if(!text)return E_POINTER;*text=SysAllocString(L"Penda 设置与词库");return *text?S_OK:E_OUTOFMEMORY; }
HRESULT TextService::GetFunction(REFGUID function,REFIID iid,IUnknown** out) {
    if(!out)return E_POINTER;*out=nullptr;if(function!=GUID_NULL || (iid!=IID_ITfFnConfigure&&iid!=IID_ITfFunction&&iid!=IID_IUnknown&&iid!=IID_ITfFnGetPreferredTouchKeyboardLayout))return E_NOINTERFACE;
    auto settings=new(std::nothrow) SettingsFunction();if(!settings)return E_OUTOFMEMORY;
    auto hr=settings->QueryInterface(iid,reinterpret_cast<void**>(out));settings->Release();return hr;
}
HRESULT TextService::ActivateEx(ITfThreadMgr* manager, TfClientId client, DWORD flags) {
    if (!manager || manager_) return E_INVALIDARG;
    // Secure-desktop activation is not supported; immersive/UI-less hosts use the standard UI element.
    if (flags & TF_TMAE_SECUREMODE) return E_ACCESSDENIED;
    manager_ = manager; client_ = client; manager_.As(&uiManager_);
    try { RefreshEngine(); } catch (...) { Deactivate(); return E_FAIL; }
    ComPtr<ITfKeystrokeMgr> keys; HRESULT hr = manager_.As(&keys);
    if (SUCCEEDED(hr)) hr = keys->AdviseKeyEventSink(client_, this, TRUE);
    if (FAILED(hr)) { Deactivate(); return hr; } keySink_ = true;
    ComPtr<ITfSource> source; hr = manager_.As(&source);
    if (SUCCEEDED(hr)) hr = source->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this), &threadCookie_);
    if (SUCCEEDED(hr)) hr = source->AdviseSink(IID_ITfThreadFocusSink, static_cast<ITfThreadFocusSink*>(this), &focusCookie_);
    if (FAILED(hr)) { Deactivate(); return hr; }
    ComPtr<ITfSourceSingle> functions; if(SUCCEEDED(manager_.As(&functions))) functionProvider_=SUCCEEDED(functions->AdviseSingleSink(client_,IID_ITfFunctionProvider,static_cast<ITfFunctionProvider*>(this)));
    ComPtr<ITfLangBarItemMgr> bar;
    if (SUCCEEDED(manager_.As(&bar))) { languageButton_.Attach(new LanguageButton()); if (FAILED(bar->AddItem(languageButton_.Get()))) languageButton_.Reset(); else languageButton_->Show(TRUE); }
    ComPtr<ITfDocumentMgr> document; if (SUCCEEDED(manager_->GetFocus(&document)) && document) { ComPtr<ITfContext> context; if (SUCCEEDED(document->GetTop(&context))) Watch(context.Get()); }
    return S_OK;
}
HRESULT TextService::Deactivate() {
    EndPending(); Watch(nullptr);
    if (manager_) {
        ComPtr<ITfSourceSingle> functions; if(functionProvider_ && SUCCEEDED(manager_.As(&functions))) functions->UnadviseSingleSink(client_,IID_ITfFunctionProvider); functionProvider_=false;
        ComPtr<ITfLangBarItemMgr> bar; if(languageButton_ && SUCCEEDED(manager_.As(&bar))) bar->RemoveItem(languageButton_.Get()); languageButton_.Reset();
        ComPtr<ITfSource> source; if (SUCCEEDED(manager_.As(&source))) { if (threadCookie_ != TF_INVALID_COOKIE) source->UnadviseSink(threadCookie_); if (focusCookie_ != TF_INVALID_COOKIE) source->UnadviseSink(focusCookie_); }
        ComPtr<ITfKeystrokeMgr> keys; if (keySink_ && SUCCEEDED(manager_.As(&keys))) keys->UnadviseKeyEventSink(client_);
    }
    uiManager_.Reset();
    threadCookie_ = focusCookie_ = TF_INVALID_COOKIE; keySink_ = false; manager_.Reset(); client_ = TF_CLIENTID_NULL;
    if (popup_) { DestroyWindow(popup_); popup_ = nullptr; UnregisterClassW(L"Penda.Tsf.Candidates.0.2",module); }
    return S_OK;
}
HRESULT TextService::Request(ITfContext* context, DWORD flags, std::function<HRESULT(TfEditCookie)> action) {
    if (!context || client_ == TF_CLIENTID_NULL) return E_UNEXPECTED;
    auto session = new (std::nothrow) Session(this, context, std::move(action)); if (!session) return E_OUTOFMEMORY;
    HRESULT result = E_FAIL; HRESULT hr = context->RequestEditSession(client_, session, flags, &result);
    session->Release(); return FAILED(hr) ? hr : result;
}
bool TextService::Writable(ITfContext* context) {
    if (!context || Compartment(context, GUID_COMPARTMENT_KEYBOARD_DISABLED) || Compartment(context, GUID_COMPARTMENT_EMPTYCONTEXT)) return false;
    TF_STATUS status{}; if (FAILED(context->GetStatus(&status)) || (status.dwDynamicFlags & TS_SD_READONLY)) return false;
    bool allowed = false;
    auto hr = Request(context, TF_ES_SYNC | TF_ES_READ, [&](TfEditCookie cookie) -> HRESULT {
        TF_SELECTION selection{}; ULONG fetched = 0;
        if (FAILED(context->GetSelection(cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched)) || !fetched) return S_OK;
        ComPtr<ITfRange> range; range.Attach(selection.range);
        allowed = true;
        ComPtr<ITfReadOnlyProperty> property;
        if (SUCCEEDED(context->GetAppProperty(GUID_PROP_INPUTSCOPE, &property))) {
            VARIANT value; VariantInit(&value);
            if (SUCCEEDED(property->GetValue(cookie, range.Get(), &value)) && value.vt == VT_UNKNOWN && value.punkVal) {
                ComPtr<ITfInputScope> scope; if (SUCCEEDED(value.punkVal->QueryInterface(IID_PPV_ARGS(&scope)))) {
                    InputScope* values = nullptr; UINT count = 0;
                    if (SUCCEEDED(scope->GetInputScopes(&values, &count))) { for (UINT i = 0; i < count; ++i) if (values[i] == IS_PASSWORD || values[i] == IS_PRIVATE) allowed = false; CoTaskMemFree(values); }
                }
            }
            VariantClear(&value);
        }
        return S_OK;
    });
    return SUCCEEDED(hr) && allowed;
}
bool TextService::WantsKey(ITfContext* context, WPARAM key) {
    if (key > 255 || !Writable(context)) return false;
    if (Down(VK_CONTROL) && Down(VK_MENU) && (key == VK_SPACE || (key >= L'1' && key <= L'3'))) return true;
    if (!enabled_ || Down(VK_CONTROL) || Down(VK_MENU) || Down(VK_LWIN) || Down(VK_RWIN)) return false;
    if (key >= L'A' && key <= L'Z') return true;
    if (input_.empty()) return (activeLanguage_!=2 && !Punctuation(key,Down(VK_SHIFT)).empty()) || (!Down(VK_SHIFT) && engine_ && engine_->numbers && key >= L'0' && key <= L'9');
    if (key == VK_TAB || key == VK_BACK || key == VK_SPACE || key == VK_RETURN || key == VK_ESCAPE || key == VK_UP || key == VK_DOWN || key == VK_LEFT || key == VK_RIGHT || key == VK_PRIOR || key == VK_NEXT || key == VK_OEM_7) return true;
    return (key >= L'0' && key <= L'9') || !Punctuation(key, Down(VK_SHIFT)).empty();
}
HRESULT TextService::OnTestKeyDown(ITfContext* context, WPARAM key, LPARAM, BOOL* eaten) { if (!eaten) return E_POINTER; try { *eaten = WantsKey(context, key); } catch (...) { *eaten = FALSE; } return S_OK; }
HRESULT TextService::OnTestKeyUp(ITfContext*, WPARAM key, LPARAM, BOOL* eaten) { if (!eaten) return E_POINTER; *eaten = key < eatenKeys_.size() && eatenKeys_[key]; return S_OK; }
HRESULT TextService::OnKeyUp(ITfContext*, WPARAM key, LPARAM, BOOL* eaten) { OnTestKeyUp(nullptr, key, 0, eaten); if (key < eatenKeys_.size()) eatenKeys_[key] = false; return S_OK; }
HRESULT TextService::OnKeyDown(ITfContext* context, WPARAM key, LPARAM flags, BOOL* eaten) {
    if (!eaten) return E_POINTER; *eaten = FALSE;
    try {
        if (!WantsKey(context, key)) return S_OK;
        const bool shift = Down(VK_SHIFT), shortcut = Down(VK_CONTROL) && Down(VK_MENU), upper = shift ^ ((GetKeyState(VK_CAPITAL) & 1) != 0);
        if (shortcut && (flags & (1LL << 30))) { *eaten = TRUE; return S_OK; }
        const auto epoch = epoch_;
        auto action = [this, context, key, shift, shortcut, upper, epoch](TfEditCookie cookie) -> HRESULT {
            if (!manager_ || epoch != epoch_) return S_OK;
            inEdit_ = true;
            HRESULT hr = E_FAIL; try { hr = Key(context, cookie, key, shift, shortcut, upper); } catch (...) { inEdit_ = false; throw; }
            inEdit_ = false; return hr;
        };
        auto hr = Request(context, TF_ES_SYNC | TF_ES_READWRITE, action);
        if (hr == TF_E_SYNCHRONOUS || hr == TF_E_LOCKED || hr == TS_E_NOLOCK) hr = Request(context, TF_ES_ASYNC | TF_ES_READWRITE, std::move(action));
        if (SUCCEEDED(hr)) { *eaten = TRUE; eatenKeys_[key] = true; }
    } catch (...) { if (popup_) ShowWindow(popup_, SW_HIDE); }
    return S_OK;
}
void TextService::RefreshEngine() { int preferred=ReadPriority(); if(preferred!=priority_) { priority_=preferred; languageOrder_[0]=preferred; int n=1; for(int i=0;i<3;++i)if(i!=preferred)languageOrder_[n++]=i; activeLanguage_=preferred; } engine_ = LoadEngine(); ReloadChineseLearning(); if (engine_.get() != lastEngine_) { languages_ = engine_->languages; lastEngine_ = engine_.get(); } }
void TextService::RefreshCandidates() {
    candidates_ = engine_->Query(input_, languages_); selected_ = 0; page_ = 0;
    ReloadChineseLearning(); RankUsage(candidates_,input_,languages_);
    if (!(languages_ & (1u<<activeLanguage_))) for (int language : languageOrder_) if (languages_ & (1u<<language)) { activeLanguage_ = language; break; }
    int first = CandidatePanel(candidates_).At(activeLanguage_,0,0); if (first >= 0) selected_ = first;
}
HRESULT TextService::Key(ITfContext* context, TfEditCookie cookie, WPARAM key, bool shift, bool shortcut, bool upper) {
    if (shortcut) { if (key == VK_SPACE) { if (composition_) Finish(cookie, confirmed_+input_); enabled_ = !enabled_; } else { if (composition_) Finish(cookie, confirmed_+input_); languages_ ^= 1u << (key - L'1'); } return S_OK; }
    if (input_.empty()) RefreshEngine();
    if (context_ && context_.Get() != context) return E_UNEXPECTED;
    if ((key >= L'A' && key <= L'Z') || (key == VK_OEM_7 && !shift && !input_.empty()) || (key == VK_OEM_MINUS && !shift && !input_.empty() && activeLanguage_ == 1)) {
        if (input_.size() >= 512) { auto hr=Finish(cookie,confirmed_+input_); if(FAILED(hr)) return hr; }
        auto previous = input_; input_ += key == VK_OEM_7 ? L'\'' : key == VK_OEM_MINUS ? L'-' : static_cast<wchar_t>(key + (upper ? 0 : 32));
        HRESULT hr = UpdateText(context, cookie); if (FAILED(hr)) { input_ = previous; if (previous.empty() && composition_) { composition_->EndComposition(cookie); Reset(); } return hr; }
        RefreshCandidates(); Present(cookie); return S_OK;
    }
    if(input_.empty() && activeLanguage_!=2 && !Punctuation(key,shift).empty()) {
        input_=activeLanguage_==1 ? JapaneseSymbol(Punctuation(key,shift)) : ChineseSymbol(Punctuation(key,shift),doubleQuote_,singleQuote_); auto hr=UpdateText(context,cookie); if(FAILED(hr)) { Reset(); return hr; } return Finish(cookie,input_);
    }
    if (input_.empty() && !shift && key >= L'0' && key <= L'9') {
        input_ = std::wstring(1, static_cast<wchar_t>(key)); auto hr = UpdateText(context, cookie); if (FAILED(hr)) { Reset(); return hr; } return Finish(cookie, engine_->Width(input_));
    }
    if (key == VK_ESCAPE) return Finish(cookie, L"");
    if (key == VK_TAB) {
        page_ = 0;
        for (int offset = 1; offset <= 3; ++offset) { int position=static_cast<int>(std::find(languageOrder_.begin(),languageOrder_.end(),activeLanguage_)-languageOrder_.begin()); int language = languageOrder_[(position+(shift ? 3-offset : offset))%3]; if (languages_ & (1u<<language)) { activeLanguage_ = language; break; } }
        int first = CandidatePanel(candidates_,page_).At(activeLanguage_,0,0); if (first >= 0) selected_ = first;
        Present(cookie); return S_OK;
    }
    if (key == VK_RETURN) {
        const auto text=activeLanguage_==1 && (languages_&2u) ? engine_->Hiragana(input_) : input_;
        return Finish(cookie, confirmed_+engine_->Width(text));
    }
    if (key == VK_BACK) {
        input_.pop_back();
        if(input_.empty() && !confirmedSegments_.empty()) { auto segment=confirmedSegments_.back(); confirmedSegments_.pop_back(); confirmedKey_.resize(confirmedKey_.size()-segment.first.size()); confirmed_.resize(confirmed_.size()-segment.second.size()); input_=segment.first; }
        if (input_.empty()) return Finish(cookie, L""); auto hr = UpdateText(context, cookie); if (SUCCEEDED(hr)) { RefreshCandidates(); Present(cookie); } return hr;
    }
    if (key == VK_UP || key == VK_DOWN || key == VK_LEFT || key == VK_RIGHT || key == VK_PRIOR || key == VK_NEXT) {
        CandidatePanel panel(candidates_,page_);
        if (key == VK_PRIOR || key == VK_NEXT) {
            int pages = std::max(1,(static_cast<int>(panel.groups[activeLanguage_].size())+9)/10);
            page_ = std::clamp(page_ + (key == VK_PRIOR ? -1 : 1),0,pages-1);
            int first = CandidatePanel(candidates_,page_).At(activeLanguage_,0,0); if (first >= 0) selected_ = first;
        } else {
            for (int language = 0; language < 3; ++language) for (int row = 0; row < 2; ++row) for (int column = 0; column < 5; ++column) if (language == activeLanguage_ && panel.At(language,row,column) == selected_) {
                int next = key == VK_UP || key == VK_DOWN ? panel.At(language,key == VK_UP ? row-1 : row+1,column) : panel.At(language,row,column+(key == VK_LEFT ? -1 : 1));
                if (next >= 0) { selected_ = next; Present(cookie); return S_OK; }
            }
        }
        Present(cookie); return S_OK;
    }
    if (key == VK_SPACE) return candidates_.empty() ? Finish(cookie,confirmed_+engine_->Width(input_)) : candidates_[selected_].language == activeLanguage_ ? Accept(cookie,selected_) : S_OK;
    if (key >= L'1' && key <= L'5') { int index = CandidatePanel(candidates_,page_).Digit(static_cast<int>(key-L'0'),shift,activeLanguage_); return index >= 0 ? Accept(cookie,index) : S_OK; }
    if (!shift && key >= L'0' && key <= L'9') return Finish(cookie, confirmed_+engine_->Width(input_ + static_cast<wchar_t>(key)));
    auto punctuation = Punctuation(key, shift);
    if (!punctuation.empty()) {
        if(activeLanguage_==0) punctuation=ChineseSymbol(punctuation,doubleQuote_,singleQuote_);
        else if(activeLanguage_==1) punctuation=JapaneseSymbol(punctuation);
        if(!candidates_.empty() && candidates_[selected_].language==activeLanguage_) return Accept(cookie,selected_,punctuation);
        return Finish(cookie,confirmed_+engine_->Width(input_)+punctuation);
    }
    return S_OK;
}
HRESULT TextService::UpdateText(ITfContext* context, TfEditCookie cookie) {
    ComPtr<ITfRange> range;
    if (!composition_) {
        ComPtr<ITfInsertAtSelection> insertion; HRESULT hr = context->QueryInterface(IID_PPV_ARGS(&insertion));
        if (FAILED(hr)) return hr;
        hr = insertion->InsertTextAtSelection(cookie, TF_IAS_QUERYONLY, nullptr, 0, &range); if (FAILED(hr)) return hr;
        ComPtr<ITfContextComposition> cc; hr = context->QueryInterface(IID_PPV_ARGS(&cc)); if (FAILED(hr)) return hr;
        hr = cc->StartComposition(cookie, range.Get(), this, &composition_); if (FAILED(hr) || !composition_) return FAILED(hr) ? hr : E_FAIL;
        context_ = context; Watch(context);
    }
    auto hr = composition_->GetRange(&range); if (FAILED(hr)) return hr;
    auto display=confirmed_+input_;
    hr = range->SetText(cookie, 0, display.c_str(), static_cast<LONG>(display.size())); if (FAILED(hr)) return hr;
    ComPtr<ITfRange> caret; hr = range->Clone(&caret); if (FAILED(hr)) return hr;
    hr = caret->Collapse(cookie, TF_ANCHOR_END); if (FAILED(hr)) return hr;
    TF_SELECTION selection{caret.Get(),{TF_AE_NONE,FALSE}};
    return context->SetSelection(cookie, 1, &selection);
}
HRESULT TextService::Finish(TfEditCookie cookie, const std::wstring& text) {
    if (!composition_) { Reset(); return S_OK; }
    const std::wstring stable = text; const int committedLanguage=activeLanguage_; auto composition = composition_; auto context = context_;
    ComPtr<ITfRange> range; auto hr = composition->GetRange(&range); if (FAILED(hr)) return hr;
    hr = range->SetText(cookie, 0, stable.c_str(), static_cast<LONG>(stable.size())); if (FAILED(hr)) return hr;
    ComPtr<ITfRange> caret; if (SUCCEEDED(range->Clone(&caret)) && SUCCEEDED(caret->Collapse(cookie, TF_ANCHOR_END))) { TF_SELECTION selection{caret.Get(),{TF_AE_NONE,FALSE}}; context->SetSelection(cookie, 1, &selection); }
    hr = composition->EndComposition(cookie);
    if (SUCCEEDED(hr)) { Reset(); if(!stable.empty()) {
        if(committedLanguage!=priority_)WritePriority(committedLanguage);
        priority_=committedLanguage;activeLanguage_=committedLanguage;languageOrder_[0]=committedLanguage;int index=1;for(int language=0;language<3;++language)if(language!=committedLanguage)languageOrder_[index++]=language;
    } }
    return hr;
}
HRESULT TextService::Accept(TfEditCookie cookie,int index,const std::wstring& suffix) {
    if(index<0 || index>=static_cast<int>(candidates_.size())) return S_OK;
    auto candidate=candidates_[index]; auto key=input_; for(auto& c:key) if(c>=L'A'&&c<=L'Z') c+=32; std::erase(key,L'\'');
    if(candidate.language==0 && suffix.empty() && !candidate.key.empty() && candidate.key.size()<key.size() && key.starts_with(candidate.key)) {
        confirmedSegments_.push_back({candidate.key,candidate.text}); confirmed_+=candidate.text; confirmedKey_+=candidate.key; input_=key.substr(candidate.key.size()); auto hr=UpdateText(context_.Get(),cookie); if(SUCCEEDED(hr)) { RefreshCandidates(); Present(cookie); } return hr;
    }
    auto learningKey=confirmedKey_+key; auto text=confirmed_+candidate.text;
    if(candidate.language==0 && !suffix.empty() && candidate.key.size()<key.size() && key.starts_with(candidate.key)) text+=key.substr(candidate.key.size());
    auto segments=confirmedSegments_;
    auto hr=Finish(cookie,text+suffix); if(SUCCEEDED(hr) && candidate.language==0) { for(const auto& segment:segments) LearnChinese(segment.first,segment.second); LearnChinese(candidate.key,candidate.text); if(!segments.empty())LearnChinese(learningKey,text); } else if(SUCCEEDED(hr)) LearnWord(candidate.language,candidate.key,candidate.text); return hr;
}
void TextService::Reset() { EndUi(); composition_.Reset(); context_.Reset(); input_.clear(); confirmed_.clear(); confirmedKey_.clear(); confirmedSegments_.clear(); candidates_.clear(); selected_ = 0; page_ = 0; layoutRetries_ = 0; if (popup_) { KillTimer(popup_,1); KillTimer(popup_,2); ShowWindow(popup_, SW_HIDE); } }
HRESULT TextService::OnCompositionTerminated(TfEditCookie, ITfComposition* composition) { if (composition_.Get() == composition) Reset(); return S_OK; }
void TextService::EndPending() {
    ++epoch_;
    if (popup_) { KillTimer(popup_,1); KillTimer(popup_,2); }
    if (composition_ && context_) {
        auto context = context_; auto expected = composition_;
        auto action = [this, expected](TfEditCookie cookie) -> HRESULT {
            if (composition_ != expected) return S_OK;
            // The inline raw text is already present. Do not move the user's new caret on focus loss.
            auto hr = expected->EndComposition(cookie); if (SUCCEEDED(hr)) Reset(); return hr;
        };
        auto hr = Request(context.Get(), TF_ES_SYNC | TF_ES_READWRITE, action);
        if (hr == TF_E_SYNCHRONOUS || hr == TF_E_LOCKED || hr == TS_E_NOLOCK) Request(context.Get(), TF_ES_ASYNC | TF_ES_READWRITE, std::move(action));
    }
    if (popup_) ShowWindow(popup_, SW_HIDE);
}
void TextService::Watch(ITfContext* context) {
    if (watched_.Get() == context) return;
    if (watched_) { ComPtr<ITfSource> source; if (SUCCEEDED(watched_.As(&source))) { if (layoutCookie_ != TF_INVALID_COOKIE) source->UnadviseSink(layoutCookie_); if (editCookie_ != TF_INVALID_COOKIE) source->UnadviseSink(editCookie_); } }
    watched_ = context; layoutCookie_ = editCookie_ = TF_INVALID_COOKIE;
    if (watched_) { ComPtr<ITfSource> source; if (SUCCEEDED(watched_.As(&source))) { source->AdviseSink(IID_ITfTextLayoutSink, static_cast<ITfTextLayoutSink*>(this), &layoutCookie_); source->AdviseSink(IID_ITfTextEditSink, static_cast<ITfTextEditSink*>(this), &editCookie_); } }
}
HRESULT TextService::OnSetFocus(ITfDocumentMgr* document, ITfDocumentMgr*) { EndPending(); ComPtr<ITfContext> context; if (document) document->GetTop(&context); Watch(context.Get()); return S_OK; }
HRESULT TextService::OnPushContext(ITfContext* context) { EndPending(); Watch(context); return S_OK; }
HRESULT TextService::OnPopContext(ITfContext* context) { if (watched_.Get() == context) { EndPending(); Watch(nullptr); } return S_OK; }
HRESULT TextService::OnSetFocus(BOOL foreground) { if (!foreground && popup_) { KillTimer(popup_,1); KillTimer(popup_,2); ShowWindow(popup_, SW_HIDE); } return S_OK; }
HRESULT TextService::OnSetThreadFocus() { if (context_ && composition_) { auto context = context_; Request(context.Get(), TF_ES_ASYNC | TF_ES_READ, [this,context](TfEditCookie cookie) { if (context_ == context) Present(cookie); return S_OK; }); } return S_OK; }
HRESULT TextService::OnKillThreadFocus() { EndPending(); return S_OK; }
HRESULT TextService::OnLayoutChange(ITfContext* context, TfLayoutCode code, ITfContextView*) {
    if (code == TF_LC_DESTROY) { if (popup_) ShowWindow(popup_, SW_HIDE); }
    else if (context_.Get() == context && composition_ && !inEdit_) Request(context, TF_ES_ASYNC | TF_ES_READ, [this,context](TfEditCookie cookie) { if (context_.Get() == context) Present(cookie); return S_OK; });
    return S_OK;
}
HRESULT TextService::OnEndEdit(ITfContext* context, TfEditCookie cookie, ITfEditRecord* record) {
    if (inEdit_ || !composition_ || context_.Get() != context) return S_OK;
    BOOL changed = FALSE; if (FAILED(record->GetSelectionStatus(&changed)) || !changed) return S_OK;
    TF_SELECTION selection{}; ULONG fetched = 0; if (FAILED(context->GetSelection(cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched)) || !fetched) return S_OK;
    ComPtr<ITfRange> caret; caret.Attach(selection.range); ComPtr<ITfRange> range;
    LONG start = 0, end = 0;
    if (SUCCEEDED(composition_->GetRange(&range)) && SUCCEEDED(caret->CompareStart(cookie, range.Get(), TF_ANCHOR_START, &start)) && SUCCEEDED(caret->CompareEnd(cookie, range.Get(), TF_ANCHOR_END, &end)) && (start < 0 || end > 0)) EndPending();
    return S_OK;
}
void TextService::EndUi() {
    auto ui=candidateUi_;auto id=uiId_;uiId_=static_cast<DWORD>(-1);candidateUi_.Reset();
    if(ui)ui->Detach();if(uiManager_ && id!=static_cast<DWORD>(-1))uiManager_->EndUIElement(id);uiAllowed_=true;
}
void TextService::UpdateUi() {
    if(!uiManager_||uiStarting_||!context_)return;
    if(!candidateUi_){
        candidateUi_.Attach(new CandidateUi());
        candidateUi_->visibility=[this](BOOL show){uiAllowed_=show!=FALSE;if(!show&&popup_)ShowWindow(popup_,SW_HIDE);else if(show&&!uiStarting_&&context_){auto context=context_;Request(context.Get(),TF_ES_ASYNC|TF_ES_READ,[this](TfEditCookie cookie){Present(cookie);return S_OK;});}};
        candidateUi_->select=[this](UINT index)->HRESULT{
            if(!candidateUi_||index>=candidateUi_->indices.size()||!context_)return E_INVALIDARG;
            int chosen=candidateUi_->indices[index];auto context=context_;auto epoch=epoch_;
            return Request(context.Get(),TF_ES_ASYNC|TF_ES_READ,[this,chosen,epoch](TfEditCookie cookie){if(epoch!=epoch_||chosen>=static_cast<int>(candidates_.size()))return S_OK;selected_=chosen;page_=CandidatePanel(candidates_).PageOf(chosen);Present(cookie);return S_OK;});
        };
        candidateUi_->finalize=[this]()->HRESULT{if(!context_||!composition_)return E_UNEXPECTED;Pick(selected_);return S_OK;};
        candidateUi_->abort=[this]()->HRESULT{if(!context_)return E_UNEXPECTED;auto context=context_;auto epoch=epoch_;return Request(context.Get(),TF_ES_ASYNC|TF_ES_READWRITE,[this,epoch](TfEditCookie cookie){return epoch==epoch_?Finish(cookie,L""):S_OK;});};
    }
    auto& ui=*candidateUi_.Get();ui.document.Reset();context_->GetDocumentMgr(&ui.document);
    std::vector<std::wstring> strings;std::vector<int> indices;UINT selection=0;
    for(int i=0;i<static_cast<int>(candidates_.size());++i)if(candidates_[i].language==activeLanguage_){if(i==selected_)selection=static_cast<UINT>(strings.size());strings.push_back(candidates_[i].text);indices.push_back(i);}
    if(strings!=ui.strings){ui.pages.clear();for(UINT i=0;i<strings.size();i+=10)ui.pages.push_back(i);}
    ui.strings=std::move(strings);ui.indices=std::move(indices);ui.selection=selection;
    if(uiId_==static_cast<DWORD>(-1)){
        BOOL show=TRUE;uiStarting_=true;auto hr=uiManager_->BeginUIElement(candidateUi_.Get(),&show,&uiId_);uiStarting_=false;
        if(FAILED(hr)){uiId_=static_cast<DWORD>(-1);uiAllowed_=true;return;}uiAllowed_=show!=FALSE;
    }
    ui.shown=uiAllowed_&&popup_&&IsWindowVisible(popup_);uiManager_->UpdateUIElement(uiId_);
}
void TextService::Present(TfEditCookie cookie) {
    if (!context_ || !composition_ || input_.empty()) return;
    UpdateUi();
    if(!uiAllowed_) {
        if(!popup_) { WNDCLASSEXW cls{sizeof(cls)};cls.hInstance=module;cls.lpszClassName=L"Penda.Tsf.Candidates.0.2";cls.lpfnWndProc=WindowProc;RegisterClassExW(&cls);popup_=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,L"Penda 三语候选",WS_POPUP,0,0,0,0,nullptr,nullptr,module,this); }
        if(popup_){ShowWindow(popup_,SW_HIDE);SetTimer(popup_,2,150,nullptr);}return;
    }
    ComPtr<ITfContextView> view; ComPtr<ITfRange> range;
    if (FAILED(context_->GetActiveView(&view)) || FAILED(composition_->GetRange(&range))) return;
    HWND owner = nullptr; view->GetWnd(&owner);
    if (!popup_) {
        WNDCLASSEXW cls{sizeof(cls)}; cls.hInstance = module; cls.lpszClassName = L"Penda.Tsf.Candidates.0.2"; cls.lpfnWndProc = WindowProc; cls.hCursor = LoadCursor(nullptr, IDC_ARROW); cls.style = CS_DROPSHADOW;
        RegisterClassExW(&cls);
        popup_ = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, cls.lpszClassName, L"Penda 三语候选", WS_POPUP, 0,0,0,0,owner,nullptr,module,this);
    }
    if (!popup_) return;
    // During SetText the application may temporarily have no layout. Hiding here used to
    // flash the popup on every keystroke. Keep the current frame until a bounded retry.
    range->Collapse(cookie, TF_ANCHOR_START); RECT caret{}; BOOL clipped = FALSE;
    const auto layout = view->GetTextExt(cookie, range.Get(), &caret, &clipped);
    if (layout == TS_E_NOLAYOUT) {
        if (IsWindowVisible(popup_)) RedrawWindow(popup_,nullptr,nullptr,RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
        if (++layoutRetries_ <= 10) SetTimer(popup_,1,20,nullptr);
        else { KillTimer(popup_,1); ShowWindow(popup_,SW_HIDE); }
        return;
    }
    KillTimer(popup_,1); layoutRetries_ = 0;
    if (FAILED(layout) || clipped || (caret.left == 0 && caret.top == 0 && caret.right == 0 && caret.bottom == 0)) { ShowWindow(popup_,SW_HIDE); return; }
    dpi_ = owner ? static_cast<int>(GetDpiForWindow(owner)) : 96; if (!dpi_) dpi_ = 96;
    auto scale = [this](int x) { return MulDiv(x, dpi_, 96); };
    MONITORINFO monitor{sizeof(monitor)}; GetMonitorInfoW(MonitorFromRect(&caret, MONITOR_DEFAULTTONEAREST), &monitor);
    int languageCount = 0; for (unsigned bit = 0; bit < 3; ++bit) if (languages_ & (1u<<bit)) ++languageCount;
    int width = std::min(scale(760),static_cast<int>(monitor.rcWork.right-monitor.rcWork.left)-16), height = scale(132+118*std::max(1,languageCount));
    int x = std::clamp(static_cast<int>(caret.left), static_cast<int>(monitor.rcWork.left), std::max(static_cast<int>(monitor.rcWork.left), static_cast<int>(monitor.rcWork.right) - width));
    int y = caret.bottom + scale(8); if (y + height > monitor.rcWork.bottom) y = caret.top - height - scale(8); y = std::max(y, static_cast<int>(monitor.rcWork.top));
    RECT current{}; GetWindowRect(popup_,&current);
    UINT flags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOREDRAW;
    if (current.left == x && current.top == y) flags |= SWP_NOMOVE;
    if (current.right-current.left == width && current.bottom-current.top == height) flags |= SWP_NOSIZE;
    SetWindowPos(popup_, HWND_TOPMOST, x,y,width,height,flags);
    // Finish the double-buffered frame before showing a new popup. Avoid an erase/resize
    // repaint followed by a second content repaint on an already visible popup.
    RedrawWindow(popup_,nullptr,nullptr,RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
    if (!IsWindowVisible(popup_)) ShowWindow(popup_,SW_SHOWNOACTIVATE);
    SetTimer(popup_,2,150,nullptr);
    if(candidateUi_)candidateUi_->shown=TRUE;
}
RECT TextService::CellRect(int language, int row, int column) const {
    RECT bounds{}; GetClientRect(popup_,&bounds);
    auto s = [this](int value) { return MulDiv(value,dpi_,96); };
    int count = 0, position = 0;
    bool before=true; for (int i : languageOrder_) { if(i==language)before=false; if (languages_ & (1u<<i)) { if (before) ++position; ++count; } }
    if (!(languages_ & (1u<<language)) || count == 0) return {};
    int cellWidth = (bounds.right-s(24))/5;
    int left = s(12)+column*cellWidth;
    return {left,s(124+position*118+row*46),left+cellWidth-s(4),s(166+position*118+row*46)};
}
void TextService::Paint() {
    PAINTSTRUCT paint; HDC target = BeginPaint(popup_, &paint); RECT bounds; GetClientRect(popup_, &bounds);
    HDC dc = CreateCompatibleDC(target); HBITMAP bitmap = CreateCompatibleBitmap(target,bounds.right,bounds.bottom); auto oldBitmap = SelectObject(dc,bitmap);
    auto s = [this](int x) { return MulDiv(x,dpi_,96); };
    auto fill = [&](RECT rect, COLORREF color) { HBRUSH brush = CreateSolidBrush(color); FillRect(dc,&rect,brush); DeleteObject(brush); };
    fill(bounds,RGB(22,26,36)); HBRUSH border = CreateSolidBrush(RGB(61,72,94)); FrameRect(dc,&bounds,border); DeleteObject(border); SetBkMode(dc,TRANSPARENT);
    HFONT smallFont = CreateFontW(-s(12),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    HFONT large = CreateFontW(-s(18),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    auto oldFont = SelectObject(dc,smallFont);
    auto text = [&](const std::wstring& value, RECT rect, COLORREF color, HFONT font) { SelectObject(dc,font); SetTextColor(dc,color); DrawTextW(dc,value.c_str(),static_cast<int>(value.size()),&rect,DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX); };
    text(L"PENDA  /  三语输入",{s(18),s(8),bounds.right-s(18),s(30)},RGB(156,170,191),smallFont);
    text(input_ + L"│",{s(18),s(30),bounds.right-s(18),s(62)},RGB(240,244,250),large);
    if(!candidates_.empty() && candidates_[selected_].language==activeLanguage_) text(L"本组："+candidates_[selected_].text,{s(18),s(62),bounds.right-s(18),s(92)},RGB(127,176,255),large);
    CandidatePanel panel(candidates_,page_);
    const wchar_t* labels[] = {L"中文",L"日本語",L"English"};
    COLORREF colors[] = {RGB(133,205,173),RGB(223,171,244),RGB(132,186,255)};
    for (int language = 0; language < 3; ++language) {
        if (!(languages_ & (1u<<language))) continue;
        auto first = CellRect(language,0,0), last = CellRect(language,0,4);
        if (language == activeLanguage_) fill({first.left,first.top-s(26),last.right,first.top-s(2)},RGB(38,45,62));
        text(std::wstring(labels[language])+(language == activeLanguage_ ? L"  ● 当前语言 · 1–5 / Shift+1–5" : L"  Tab 切换到此组"),{first.left,first.top-s(26),last.right,first.top-s(2)},colors[language],smallFont);
        for (int row = 0; row < 2; ++row) for (int column = 0; column < 5; ++column) {
            auto cell = CellRect(language,row,column); int index = panel.At(language,row,column);
            if (index == selected_ && language == activeLanguage_) { fill(cell,RGB(38,45,62)); fill({cell.left,cell.bottom-s(3),cell.right,cell.bottom},colors[language]); }
            int digit = column+1;
            text((row ? L"⇧" : L"") + std::to_wstring(digit),{cell.left+s(4),cell.top,cell.left+s(28),cell.bottom},colors[language],smallFont);
            text(index < 0 ? L"—" : candidates_[index].text,{cell.left+s(29),cell.top,cell.right-s(4),cell.bottom},index < 0 ? RGB(90,102,124) : RGB(240,244,250),large);
        }
    }
    text(L"最近输入语言优先 · Tab 换组 · 1–5 / Shift+1–5 选词 · 空格确认 · PgUp/PgDn 翻页   " + std::to_wstring(page_+1) + L"/" + std::to_wstring(std::max(1,(static_cast<int>(panel.groups[activeLanguage_].size())+9)/10)),{s(18),bounds.bottom-s(30),bounds.right-s(12),bounds.bottom-s(7)},RGB(156,170,191),smallFont);
    BitBlt(target,0,0,bounds.right,bounds.bottom,dc,0,0,SRCCOPY); SelectObject(dc,oldFont); DeleteObject(smallFont); DeleteObject(large); SelectObject(dc,oldBitmap); DeleteObject(bitmap); DeleteDC(dc); EndPaint(popup_,&paint);
}
void TextService::Pick(int index) {
    if (!context_ || index < 0 || index >= static_cast<int>(candidates_.size())) return;
    auto text = candidates_[index].text; auto expected = composition_; auto context = context_; auto epoch = epoch_;
    Request(context.Get(),TF_ES_ASYNC | TF_ES_READWRITE,[this,index,expected,epoch](TfEditCookie cookie) { return epoch_ == epoch && composition_ == expected ? Accept(cookie,index) : S_OK; });
}
LRESULT TextService::WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto self = reinterpret_cast<TextService*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if (message == WM_NCCREATE) { self = static_cast<TextService*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams); SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self)); }
    try {
        if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
        if (message == WM_ERASEBKGND) return 1;
        if (self && message == WM_PAINT) { self->Paint(); return 0; }
        if(self && message==WM_TIMER && wparam==2) {
            if((!IsWindowVisible(window)&&self->uiAllowed_)||self->input_.empty()){KillTimer(window,2);return 0;}
            bool changed=ReloadChineseLearning();auto next=LoadEngine();
            if(changed || next.get()!=self->engine_.get()){
                if(next.get()!=self->engine_.get()){self->engine_=next;self->lastEngine_=next.get();self->languages_=next->languages;}
                auto context=self->context_;auto epoch=self->epoch_;
                self->Request(context.Get(),TF_ES_ASYNC|TF_ES_READ,[self,context,epoch](TfEditCookie cookie){if(self->epoch_==epoch && self->context_==context){self->RefreshCandidates();self->Present(cookie);}return S_OK;});
            }return 0;
        }
        if (self && message == WM_TIMER && wparam == 1) {
            KillTimer(window,1);
            if (self->context_ && self->composition_) {
                auto context = self->context_; auto epoch = self->epoch_;
                self->Request(context.Get(),TF_ES_ASYNC | TF_ES_READ,[self,context,epoch](TfEditCookie cookie) { if (self->epoch_ == epoch && self->context_ == context) self->Present(cookie); return S_OK; });
            }
            return 0;
        }
        if (self && message == WM_LBUTTONDOWN) {
            POINT point{static_cast<short>(LOWORD(lparam)),static_cast<short>(HIWORD(lparam))}; CandidatePanel panel(self->candidates_,self->page_);
            for (int language = 0; language < 3; ++language) for (int row = 0; row < 2; ++row) for (int column = 0; column < 5; ++column) { auto cell = self->CellRect(language,row,column); if (PtInRect(&cell,point)) { int index=panel.At(language,row,column); if(index>=0){self->activeLanguage_ = language; self->Pick(index);} } }
            return 0;
        }
    } catch (...) { ShowWindow(window,SW_HIDE); }
    return DefWindowProcW(window,message,wparam,lparam);
}
}
