#include "../../src/Penda.Tsf/TextService.h"
#include "Store.h"
#include <ctffunc.h>
#include "Manager.h"
#include "../../src/Penda.Tsf/CandidatePanel.h"
#include "../../src/Penda.Tsf/Learning.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <userenv.h>
#include <appmodel.h>
#include <chrono>

namespace penda { HMODULE module = nullptr; }
int checks = 0;
class MenuCapture final : public ITfMenu {
    ULONG refs_=1;
public:
    std::vector<UINT> ids; std::vector<std::wstring> labels;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override {if(!out)return E_POINTER;*out=nullptr;if(iid!=IID_IUnknown&&iid!=IID_ITfMenu)return E_NOINTERFACE;*out=static_cast<ITfMenu*>(this);AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs_;} ULONG STDMETHODCALLTYPE Release() override{return --refs_;}
    HRESULT STDMETHODCALLTYPE AddMenuItem(UINT id,DWORD,HBITMAP,HBITMAP,const WCHAR* text,ULONG length,ITfMenu** submenu) override {if(submenu)*submenu=nullptr;ids.push_back(id);labels.emplace_back(text?std::wstring(text,length):L"");return S_OK;}
};
class UiHost final : public ITfUIElementSink {
    ULONG refs_=1;
public:
    DWORD id=static_cast<DWORD>(-1);unsigned starts=0,updates=0,ends=0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override{if(!out)return E_POINTER;*out=nullptr;if(iid!=IID_IUnknown&&iid!=IID_ITfUIElementSink)return E_NOINTERFACE;*out=static_cast<ITfUIElementSink*>(this);AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs_;}ULONG STDMETHODCALLTYPE Release() override{return --refs_;}
    HRESULT STDMETHODCALLTYPE BeginUIElement(DWORD value,BOOL* show) override{if(!show)return E_POINTER;id=value;*show=FALSE;++starts;return S_OK;}
    HRESULT STDMETHODCALLTYPE UpdateUIElement(DWORD) override{++updates;return S_OK;}
    HRESULT STDMETHODCALLTYPE EndUIElement(DWORD) override{++ends;return S_OK;}
};
bool menuVisible=false; unsigned menuTicks=0;
void CALLBACK CheckMenu(HWND,UINT,UINT_PTR timer,DWORD) {
    GUITHREADINFO info{sizeof(info)}; auto owner=FindWindowW(L"STATIC",L"Penda.Tsf.MenuOwner");
    if(GetGUIThreadInfo(GetCurrentThreadId(),&info) && owner && info.hwndMenuOwner==owner && (info.flags&GUI_INMENUMODE))menuVisible=true;
    if(menuVisible || ++menuTicks>=25) { EndMenu(); KillTimer(nullptr,timer); }
}
void Check(bool condition,const char* label) { if (!condition) throw std::runtime_error(label); ++checks; }
void Hr(HRESULT hr,const char* label) { if (FAILED(hr)) { std::cerr << label << " HRESULT=" << std::hex << hr << '\n'; throw std::runtime_error(label); } }
void Pump() { MSG message; while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); } }
HWND TestPopup() {
    HWND result=nullptr;EnumThreadWindows(GetCurrentThreadId(),[](HWND window,LPARAM value)->BOOL{wchar_t name[128]{};GetClassNameW(window,name,128);if(std::wstring_view(name)==L"Penda.Tsf.Candidates.0.2"){*reinterpret_cast<HWND*>(value)=window;return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&result));return result;
}
int wmain(int argc,wchar_t** argv) {
    try {
        if(argc==3 && std::wstring_view(argv[1])==L"--storage-probe") {
            UINT32 length=0;auto identity=GetCurrentPackageFullName(&length,nullptr);
            penda::Engine engine;bool loaded=engine.Load(argv[2]);
            wchar_t location[32768]{};GetModuleFileNameW(nullptr,location,32768);
            auto reportPath=std::filesystem::path(location).parent_path().parent_path().parent_path()/L"validation"/L"unpackaged-storage.txt";
            std::ofstream report(reportPath);
            report<<"Package identity result: "<<identity<<" (15700 = unpackaged)\nLoaded: "<<loaded<<"\nChinese: "<<engine.words[0].size()<<"\nJapanese: "<<engine.words[1].size()<<"\n";
            if(identity!=APPMODEL_ERROR_NO_PACKAGE)return 109;
            auto rows=engine.Query(L"ogoru");
            return loaded&&engine.words[1].size()>1000000&&std::any_of(rows.begin(),rows.end(),[](const auto& row){return row.language==1&&row.text==L"奢る";})?0:110;
        }
        if(argc==3 && std::wstring_view(argv[1])==L"--desktop-launcher") {
            wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);std::wstring command=L"\""+std::wstring(exe)+L"\" --storage-probe \""+argv[2]+L"\"";
            STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
            if(!CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process))return 113;
            auto waited=WaitForSingleObject(process.hProcess,15000);DWORD result=0;GetExitCodeProcess(process.hProcess,&result);CloseHandle(process.hThread);CloseHandle(process.hProcess);return waited==WAIT_OBJECT_0?static_cast<int>(result):114;
        }
        if(argc==3 && std::wstring_view(argv[1])==L"--unpackaged-check") {
            SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);std::vector<BYTE> buffer(size);auto attributes=reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(buffer.data());
            Check(InitializeProcThreadAttributeList(attributes,1,0,&size)!=FALSE,"initialize desktop-runtime attributes");DWORD policy=PROCESS_CREATION_DESKTOP_APP_BREAKAWAY_ENABLE_PROCESS_TREE;
            Check(UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_DESKTOP_APP_POLICY,&policy,sizeof(policy),nullptr,nullptr)!=FALSE,"set desktop-runtime policy");
            wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);std::wstring command=L"\""+std::wstring(exe)+L"\" --desktop-launcher \""+argv[2]+L"\"";
            STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.lpAttributeList=attributes;PROCESS_INFORMATION process{};
            BOOL created=CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,EXTENDED_STARTUPINFO_PRESENT|CREATE_NO_WINDOW,nullptr,nullptr,&startup.StartupInfo,&process);auto error=GetLastError();DeleteProcThreadAttributeList(attributes);if(!created){std::cerr<<"Desktop-runtime child error="<<error<<"\n";return 111;}
            auto waited=WaitForSingleObject(process.hProcess,15000);DWORD result=0;GetExitCodeProcess(process.hProcess,&result);CloseHandle(process.hThread);CloseHandle(process.hProcess);std::cout<<"Unpackaged storage probe exit="<<result<<"\n";return waited==WAIT_OBJECT_0?static_cast<int>(result):112;
        }
        if(argc==2&&std::wstring_view(argv[1])==L"--compatibility-guids"){for(const auto& guid:{GUID_TFCAT_TIPCAP_UIELEMENTENABLED,GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT}){wchar_t text[40]{};StringFromGUID2(guid,text,40);std::wcout<<text<<L"\n";}return 0;}
        if(argc==3 && std::wstring_view(argv[1])==L"--container-child-read") {
            HANDLE token=nullptr;DWORD container=0,size=0;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return 101;BOOL ok=GetTokenInformation(token,TokenIsAppContainer,&container,sizeof(container),&size);CloseHandle(token);if(!ok||!container)return 102;
            penda::Engine engine;if(!engine.Load(argv[2]))return 103;if(engine.words[0].size()<500000||engine.words[1].size()<1000000)return 104;
            for(const auto& pair:{std::pair{L"ogoru",L"奢る"},std::pair{L"nihongo",L"日本語"}}){auto rows=engine.Query(pair.first);if(std::none_of(rows.begin(),rows.end(),[&](const auto& row){return row.language==1&&row.text==pair.second;}))return 108;}
            penda::Engine resolved;if(!resolved.Load(penda::UserSnapshotPath()))return 105;if(resolved.words[0].size()!=engine.words[0].size()||resolved.words[1].size()!=engine.words[1].size())return 106;
            return 0;
        }
        if(argc==3 && std::wstring_view(argv[1])==L"--container-check") {
            PSID sid=nullptr;auto profile=CreateAppContainerProfile(L"Penda.ReadOnlyDictionary.Probe",L"Penda dictionary compatibility test",L"Network-free read-only dictionary validation",nullptr,0,&sid);if(profile==HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS))profile=DeriveAppContainerSidFromAppContainerName(L"Penda.ReadOnlyDictionary.Probe",&sid);Hr(profile,"prepare isolated test profile");
            SECURITY_CAPABILITIES capabilities{};capabilities.AppContainerSid=sid;SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);std::vector<BYTE> data(bytes);auto attributes=reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(data.data());Check(InitializeProcThreadAttributeList(attributes,1,0,&bytes)!=FALSE,"initialize restricted child attributes");Check(UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,&capabilities,sizeof(capabilities),nullptr,nullptr)!=FALSE,"set app-container capabilities");
            wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);std::wstring command=L"\""+std::wstring(exe)+L"\" --container-child-read \""+argv[2]+L"\"";STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.lpAttributeList=attributes;PROCESS_INFORMATION process{};
            wchar_t system[32768]{};GetSystemDirectoryW(system,32768);BOOL created=CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,EXTENDED_STARTUPINFO_PRESENT|CREATE_NO_WINDOW,nullptr,system,&startup.StartupInfo,&process);auto error=GetLastError();DeleteProcThreadAttributeList(attributes);FreeSid(sid);if(!created){std::cerr<<"Restricted child creation error="<<error<<"\n";throw std::runtime_error("App-container launch failed");}
            auto waited=WaitForSingleObject(process.hProcess,15000);DWORD result=0;GetExitCodeProcess(process.hProcess,&result);if(waited==WAIT_TIMEOUT)TerminateProcess(process.hProcess,107);CloseHandle(process.hThread);CloseHandle(process.hProcess);if(waited!=WAIT_OBJECT_0||result!=0){std::cerr<<"Restricted dictionary child exit="<<result<<"\n";throw std::runtime_error("App-container dictionary verification failed");}std::cout<<"App-container full dictionary read and canonical path resolution passed.\n";return 0;
        }
        if(argc==3 && std::wstring_view(argv[1])==L"--nasal-snapshot") {
            penda::Engine current;Check(current.Load(argv[2]),"load full dictionary for nn disambiguation");
            for(const auto& spelling:{L"funniki",L"fun'iki",L"funn'iki"}){auto rows=current.Query(spelling,2);Check(!rows.empty()&&rows.front().text==L"雰囲気","ambiguous nn resolves to first-page kanji");Check(current.PreferredHiragana(spelling)==L"ふんいき","ambiguous nn resolves Enter kana");}
            for(const auto& pair:{std::pair{L"konnichiha",L"こんにちは"},std::pair{L"onna",L"おんな"},std::pair{L"annai",L"あんない"},std::pair{L"kinniku",L"きんにく"}})Check(current.PreferredHiragana(pair.first)==pair.second,"established nn spellings remain unchanged");
            std::cout<<"Full dictionary nn ambiguity, explicit separators and normal spellings passed.\n";return 0;
        }
        if(argc==3 && std::wstring_view(argv[1])==L"--romaji-snapshot") {
            penda::Engine current;Check(current.Load(argv[2]),"load installed pre-extension dictionary");
            for(const auto& pair:{std::pair{L"fa",L"ファ"},std::pair{L"ofa-",L"オファー"},std::pair{L"firumu",L"フィルム"},std::pair{L"shefu",L"シェフ"},std::pair{L"dhizuni-",L"ディズニー"}}){
                auto rows=current.Query(pair.first,2);Check(std::any_of(rows.begin(),rows.end(),[&](const auto& item){return item.text==pair.second&&item.key==current.Hiragana(pair.first);}),"extended romaji reaches real dictionary entry, not only script fallback");
            }
            std::cout<<"Existing full dictionary: extended romaji and katakana dictionary lookups passed.\n";return 0;
        }
        if(argc==3 && std::wstring_view(argv[1])==L"--abbreviation-snapshot") {
            penda::Engine current;Check(current.Load(argv[2]),"load real abbreviation dictionary");auto rows=current.Query(L"haox",1);size_t rank=rows.size();for(size_t i=0;i<rows.size();++i)if(rows[i].text==L"好像"){rank=i;break;}
            std::cout<<"haox -> haoxiang candidate rank="<<rank<<" / "<<rows.size()<<"\n";Check(rank<10,"haox offers full-word completion on first page");return 0;
        }
        if(argc==3 && std::wstring_view(argv[1])==L"--ranking-snapshot") {
            penda::Engine current;Check(current.Load(argv[2]),"load actual user index");penda::ReloadChineseLearning();auto rows=current.Query(L"gengxin");penda::RankUsage(rows,L"gengxin",current.languages);
            auto first=std::find_if(rows.begin(),rows.end(),[](const auto& item){return item.language==0;});Check(first!=rows.end()&&first->text==L"更新","actual dictionary gengxin prioritizes complete word");std::cout<<"Actual user snapshot: gengxin longest-match ranking passed.\n";return 0;
        }
        if (argc == 3 && std::wstring_view(argv[1]) == L"--japanese-snapshot") {
            penda::Engine japanese; Check(japanese.Load(argv[2]),"load full Japanese snapshot");
            for (const auto& pair : {std::pair{L"nihongo",L"日本語"},std::pair{L"toukyou",L"東京"},std::pair{L"kanji",L"漢字"},std::pair{L"gakkou",L"学校"},std::pair{L"taberu",L"食べる"},std::pair{L"tabeta",L"食べた"},std::pair{L"kaita",L"書いた"},std::pair{L"yonda",L"読んだ"},std::pair{L"shimbun",L"新聞"},std::pair{L"ogoru",L"奢る"},std::pair{L"ogoruna",L"奢るな"},std::pair{L"おごるな",L"驕るな"},std::pair{L"anofutari",L"あの二人"},std::pair{L"あのふたり",L"あの二人"}}) {
                auto rows = japanese.Query(pair.first); Check(std::any_of(rows.begin(),rows.end(),[&](const auto& row) { return row.language == 1 && row.text == pair.second; }),"full Japanese native kanji lookup");
            }
            std::vector<double> timings;
            for(int i=0;i<80;++i){auto begin=std::chrono::steady_clock::now();auto result=japanese.Query(i%2?L"anofutarihakita":L"watashihanihongowobenkyoushiteimasu");Check(!result.empty(),"Japanese phrase benchmark returns candidates");timings.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());}
            std::sort(timings.begin(),timings.end());std::cout<<"Full Japanese phrase query median="<<timings[40]<<" ms P95="<<timings[76]<<" ms\n";
            std::cout << "Full Japanese dictionary/native kanji lookups passed.\n"; return 0;
        }
        if (argc == 3 && std::wstring_view(argv[1]) == L"--snapshot") {
            penda::Engine imported; Check(imported.Load(argv[2]),"load real Rime import snapshot");
            for (const auto& pair : {std::pair{L"kan",L"看"},std::pair{L"kankan",L"看看"},std::pair{L"gongzuo",L"工作"}}) {
                auto rows = imported.Query(pair.first);
                Check(std::any_of(rows.begin(),rows.end(),[&](const auto& row) { return row.language == 0 && row.text == pair.second; }),"native query of imported Rime dictionary");
            }
            std::cout << "Real Rime dictionaries: native snapshot and candidate queries passed.\n"; return 0;
        }
        if (argc != 3) throw std::runtime_error("Usage: NativeTests FIXTURES DLL");
        auto learningTest=(std::filesystem::absolute(argv[1])/(L"learn-test-"+std::to_wstring(GetCurrentProcessId())+L".tsv")).wstring();
        SetEnvironmentVariableW(L"PENDA_LEARNING_PATH",learningTest.c_str());
        auto priorityTest=learningTest+L".priority"; SetEnvironmentVariableW(L"PENDA_PREFERENCE_PATH",priorityTest.c_str());
        for(int i=0;i<5;++i)penda::LearnChinese(L"ge",L"个");
        penda::LearnChinese(L"gengxin",L"更新");
        penda::LearnWord(1,L"renshuuyou",L"練習用");std::vector<penda::Candidate> rememberedJapanese;penda::RankUsage(rememberedJapanese,L"renshuuyou",2);Check(!rememberedJapanese.empty()&&rememberedJapanese.front().text==L"練習用","Japanese learned full phrase is recalled immediately even outside beam candidates");
        std::vector<penda::Candidate> longest={{L"个",L"ge",0,3000000},{L"更新",L"gengxin",0,1}};
        penda::RankUsage(longest,L"gengxin",1);Check(longest.front().text==L"更新","frequent short character cannot displace longest match");
        penda::LearnChinese(L"ceping",L"测评");penda::LearnChinese(L"ceping",L"测评");penda::LearnChinese(L"ceping",L"测评");penda::LearnChinese(L"ceping",L"侧评");
        std::vector<penda::Candidate> weighted;penda::RankUsage(weighted,L"ceping",1);Check(weighted.front().text==L"测评","usage count wins over the most recent rare word immediately");
        penda::ReloadChineseLearning();weighted.clear();penda::RankUsage(weighted,L"ceping",1);Check(weighted.front().text==L"测评","usage counts survive disk reload");
        penda::Engine longEngine; longEngine.languages=1; longEngine.words[0]={{L"kan",L"看",100000},{L"kankan",L"看看",200000},{L"wo",L"我",200000}};
        Check(longEngine.Query(L"wokankan").front().text==L"我","Chinese offers first word for staged selection");
        std::wstring longKey,longText; for(int i=0;i<12;++i) { longKey+=L"wokankan"; longText+=L"我看看"; }
        Check(longEngine.Query(longKey).front().text==L"我","longer-than-64 pinyin offers short leading word");
        penda::LearnChinese(longKey,longText); penda::ReloadChineseLearning(); Check(penda::RecallChinese(longKey)==longText,"long phrase learning survives disk round trip");
        penda::Engine many;many.languages=1;for(int i=0;i<300;i++)many.words[0].push_back({L"shi",std::wstring(1,static_cast<wchar_t>(0x4E00+i)),1000-i});
        auto manyRows=many.Query(L"shi");Check(manyRows.size()==300,"all exact homophones retained beyond 192 candidates");
        Check(penda::CandidatePanel(manyRows,29).Digit(5,true)==299,"last homophone selectable on thirtieth page");
        std::vector<penda::Candidate> layoutRows;
        for (int rank = 0; rank < 36; ++rank) for (int language = 0; language < 3; ++language) layoutRows.push_back({std::to_wstring(language)+L"-"+std::to_wstring(rank),L"key",language,100-rank});
        penda::CandidatePanel panel(layoutRows);
        Check(panel.Pages() == 4,"four ten-per-language pages");
        for (int language = 0; language < 3; ++language) for (int digit = 1; digit <= 5; ++digit) {
            Check(layoutRows[panel.Digit(digit,false,language)].text == std::to_wstring(language)+L"-"+std::to_wstring(digit-1),"digits select active-language first row");
            Check(layoutRows[panel.Digit(digit,true,language)].text == std::to_wstring(language)+L"-"+std::to_wstring(digit+4),"shift digits select active-language second row");
        }
        Check(penda::CandidatePanel(layoutRows,1).Digit(1,true) == 45,"shift-digit mapping on next page");
        Check(penda::CandidatePanel({}).Digit(1,true) == -1,"empty second-row slot is not selectable");
        unsigned parityInputs = 0;
        {penda::Engine legacy;Check(legacy.Load((std::filesystem::path(argv[1])/L"legacy-romaji.pdat").wstring()),"load legacy index without new romaji table");Check(legacy.Hiragana(L"ofa-")==L"おふぁー"&&legacy.Hiragana(L"dhizuni-")==L"でぃずにー","native built-in rules upgrade old dictionary indexes");}
        for (int index = 0; index < 4; ++index) {
            auto stem = std::filesystem::path(argv[1]) / (L"case"+std::to_wstring(index));
            penda::Engine engine; Check(engine.Load(stem.wstring()+L".pdat"),"load compiled snapshot");
            std::ifstream stream(stem.wstring()+L".golden",std::ios::binary);
            auto u32 = [&]() { unsigned value{}; stream.read(reinterpret_cast<char*>(&value),4); Check(bool(stream),"golden integer"); return value; };
            auto text = [&]() { auto count = u32(); std::wstring value(count,L'\0'); stream.read(reinterpret_cast<char*>(value.data()),count*2); Check(bool(stream),"golden string"); return value; };
            unsigned count = u32(); parityInputs += count;
            for (unsigned i = 0; i < count; ++i) {
                auto input = text(), hiragana = text(), width = text(), preferred = text();
                if (engine.Hiragana(input) != hiragana) std::cerr << "Romaji mismatch: case " << index << " input " << i << '\n';
                Check(engine.Hiragana(input) == hiragana,"native romaji parity"); Check(engine.Width(input) == width,"native width parity");Check(engine.PreferredHiragana(input)==preferred,"native dictionary-aware kana parity");
                auto rows = engine.Query(input); auto expected = u32(); Check(rows.size() == expected,"native candidate count parity");
                for (unsigned j = 0; j < expected; ++j) { auto value = text(); auto language = u32(); Check(rows[j].text == value && rows[j].language == static_cast<int>(language),"native candidate order parity"); }
            }
            auto before = engine.Query(L"ni"); Check(!engine.Load(L"not-existing.pdat"),"reject missing snapshot"); Check(engine.Query(L"ni").size() == before.size(),"preserve old snapshot on failure");
            auto truncated = std::filesystem::path(argv[1]) / L"truncated.pdat";
            { std::ofstream broken(truncated,std::ios::binary); broken << "PND1"; }
            Check(!engine.Load(truncated.wstring()),"reject truncated native snapshot"); Check(engine.Query(L"ni").size() == before.size(),"preserve loaded index after corruption");
        }
        std::cout << "Engine parity passed across 4 settings / " << parityInputs << " inputs.\n";
        Hr(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED),"COM init");
        HMODULE dll = LoadLibraryW(argv[2]); Check(dll != nullptr,"load TSF DLL");
        auto factoryExport = reinterpret_cast<HRESULT(__stdcall*)(REFCLSID,REFIID,void**)>(GetProcAddress(dll,"DllGetClassObject"));
        auto canUnload = reinterpret_cast<HRESULT(__stdcall*)()>(GetProcAddress(dll,"DllCanUnloadNow")); Check(factoryExport && canUnload,"COM exports");
        {
            using penda::ComPtr;
            ComPtr<IClassFactory> factory; Hr(factoryExport(penda::ServiceId,IID_PPV_ARGS(&factory)),"get class factory");
            Check(canUnload() == S_FALSE,"factory holds DLL");
            ComPtr<ITfTextInputProcessorEx> service; Hr(factory->CreateInstance(nullptr,IID_PPV_ARGS(&service)),"create native text service");
            ComPtr<ITfThreadMgr> manager; Hr(CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager)),"create real TSF manager");
            TfClientId client{}; Hr(manager->Activate(&client),"activate TSF manager");
            ComPtr<ITfDocumentMgr> document; Hr(manager->CreateDocumentMgr(&document),"create document manager");
            ComPtr<Store> store; store.Attach(new Store()); store->window = CreateWindowExW(0,L"STATIC",L"Penda TSF test host",WS_POPUP,0,0,800,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
            ComPtr<ITfContext> context; TfEditCookie cookie{}; Hr(document->CreateContext(client,0,store.Get(),&context,&cookie),"create real edit context");
            Hr(document->Push(context.Get()),"push edit context"); Hr(manager->SetFocus(document.Get()),"focus TSF document");
            ComPtr<TestManager> bridge; bridge.Attach(new TestManager(manager.Get()));
            Hr(service->ActivateEx(bridge.Get(),client,0),"activate native service with test key dispatcher");
            ComPtr<ITfKeyEventSink> keys; Hr(service.As(&keys),"key sink");
            auto key = [&](WPARAM value) {
                BOOL eaten = FALSE; auto before = store->text; Hr(keys->OnTestKeyDown(context.Get(),value,0,&eaten),"test key"); if(!eaten)std::cerr<<"Uneaten key="<<value<<" text-length="<<store->text.size()<<" checks="<<checks<<"\n"; Check(eaten,"TSF consumes input key"); Check(store->text == before,"test key does not modify target text");
                Hr(keys->OnKeyDown(context.Get(),value,0,&eaten),"key down"); Check(eaten,"TSF edit session accepted"); Pump();
                Hr(keys->OnKeyUp(context.Get(),value,0,&eaten),"key up"); Check(eaten,"matching key up");
            };
            auto type = [&](const wchar_t* value) { for (; *value; ++value) key(*value==L'-' ? VK_OEM_MINUS : *value==L'\'' ? VK_OEM_7 : static_cast<WPARAM>(towupper(*value))); };
            type(L"n");
            HWND popup = TestPopup(); Check(popup && IsWindowVisible(popup),"candidate popup visible");
            RECT initial{}; GetWindowRect(popup,&initial);
            store->noLayout = true; key(L'I');
            Check(IsWindowVisible(popup),"temporary missing layout does not hide popup");
            store->noLayout = false; key(L'H');
            RECT updated{}; GetWindowRect(popup,&updated);
            Check(EqualRect(&initial,&updated),"typing keeps popup geometry stable");
            Check(popup == TestPopup(),"typing reuses existing popup");
            key(VK_ESCAPE); Check(!IsWindowVisible(popup),"cancel hides popup and ends retry");
            type(L"nihao"); Check(store->text == L"nihao","composition lives inside target text store"); key(VK_SPACE); Check(store->text == L"你好","Chinese commit through TSF edit session");
            type(L"nihongo"); key(VK_TAB); key(VK_SPACE); Check(store->text == L"你好日本語","Japanese commit through active-language TSF edit session"); type(L"x"); key(VK_TAB); key(VK_TAB); key(VK_ESCAPE);
            type(L"hello"); key(VK_RETURN); Check(store->text == L"你好日本語hello","raw English commit");
            type(L"nihao"); key(VK_ESCAPE); Check(store->text == L"你好日本語hello","cancel removes only composition");
            type(L"hello"); key(VK_BACK); Check(store->text.ends_with(L"hell"),"backspace updates composition"); key(VK_RETURN);
            {std::ofstream preference{std::filesystem::path(priorityTest)};preference<<0;}
            type(L"nihao"); key(VK_OEM_PERIOD); Check(store->text.ends_with(L"你好。"),"Chinese punctuation commits candidate");
            key(VK_OEM_COMMA); Check(store->text.ends_with(L"你好。，"),"standalone Chinese punctuation after commit");
            type(L"nihao"); key(VK_NEXT); key(VK_PRIOR); key(L'1'); Check(store->text.ends_with(L"你好"),"paging and number selection");
            type(L"nihongo"); key(VK_TAB); key(L'1'); Check(store->text.ends_with(L"日本語"),"Tab then 1 selects Japanese group");
            type(L"hello"); key(VK_TAB);key(VK_TAB); key(L'1'); Check(store->text.ends_with(L"hello"),"Tab then 1 selects English group");
            type(L"x"); key(VK_TAB); key(VK_ESCAPE);
            penda::Engine current; if (!current.Load(penda::UserSnapshotPath())) Hr(current.Load((std::filesystem::path(argv[2]).parent_path()/L"engine.pdat").wstring()) ? S_OK : E_FAIL,"load selection expectation");
            if(current.words[1].size()>1000000){type(L"ogoru");key(VK_TAB);key(L'1');Check(store->text.ends_with(L"奢る"),"screenshot ogoru commits kanji through full TSF service");type(L"x");key(VK_TAB);key(VK_TAB);key(VK_ESCAPE);}
            {std::ofstream preference{std::filesystem::path(priorityTest)};preference<<0;}
            auto beforeCompletion=store->text;type(L"haox");key(L'1');Check(store->text==beforeCompletion+L"好像","incomplete pinyin commits completed word without leftover letters");type(L"haox");key(VK_SPACE);Check(store->text.ends_with(L"好像好像"),"abbreviated word selection is learned immediately");
            auto shiftRows = current.Query(L"s"); int shiftIndex = penda::CandidatePanel(shiftRows).Digit(1,true); Check(shiftIndex >= 0,"Chinese second-row fixture exists");
            auto beforeShift = store->text; type(L"s");
            BYTE keyboardState[256]{}; GetKeyboardState(keyboardState); BYTE shifted[256]; std::copy_n(keyboardState,256,shifted); shifted[VK_SHIFT] |= 0x80; SetKeyboardState(shifted); key(L'1'); SetKeyboardState(keyboardState);
            Check(store->text == beforeShift+shiftRows[shiftIndex].text,"Shift+1 commits second-row candidate, not punctuation");
            auto pageRows=current.Query(L"s");int pageIndex=penda::CandidatePanel(pageRows,1).Digit(1,false,0);Check(pageIndex>=0,"second page fixture exists");
            type(L"s");key(VK_NEXT);key(L'1');Check(store->text.ends_with(pageRows[pageIndex].text),"PageDown reveals and selects later candidates");
            ComPtr<ITfThreadFocusSink> focusSink; Hr(service.As(&focusSink),"thread focus sink");
            type(L"ni"); auto beforeFocusLoss = store->text; Hr(focusSink->OnKillThreadFocus(),"thread focus loss"); Pump(); Check(store->text == beforeFocusLoss,"focus loss preserves raw text"); focusSink.Reset();
            store->readOnly = true; BOOL eaten = TRUE; Hr(keys->OnTestKeyDown(context.Get(),L'A',0,&eaten),"readonly probe"); Check(!eaten,"read-only passthrough"); store->readOnly = false;
            ComPtr<ITfCompartmentMgr> compartments; Hr(context.As(&compartments),"context compartments"); ComPtr<ITfCompartment> disabled; Hr(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_DISABLED,&disabled),"disabled compartment");
            VARIANT value; VariantInit(&value); value.vt = VT_I4; value.lVal = 1; Hr(disabled->SetValue(client,&value),"disable context"); keys->OnTestKeyDown(context.Get(),L'A',0,&eaten); Check(!eaten,"disabled context passthrough"); value.lVal = 0; disabled->SetValue(client,&value);
            auto niRows=current.Query(L"ni"); auto alternateIndex=penda::CandidatePanel(niRows).Digit(2,false,0); Check(alternateIndex>=0,"alternate Chinese candidate fixture"); auto alternate=niRows[alternateIndex].text;
            auto splitRows=current.Query(L"nihaoxiexie"); auto prefix=std::find_if(splitRows.begin(),splitRows.end(),[](const auto& item) { return item.language==0 && item.key==L"nihao" && item.text==L"你好"; }); Check(prefix!=splitRows.end(),"leading-word correction candidate exists");
            int rank=0; for(auto it=splitRows.begin();it!=prefix;++it) if(it->language==0) ++rank;
            type(L"nihaoxiexie"); BYTE savedState[256]{}; GetKeyboardState(savedState); BYTE prefixState[256]; std::copy_n(savedState,256,prefixState); if(rank>=5) prefixState[VK_SHIFT]|=0x80; SetKeyboardState(prefixState); key(L'1'+rank%5); SetKeyboardState(savedState);
            Check(store->text.ends_with(L"你好xiexie"),"confirming a prefix preserves remaining pinyin"); key(VK_SPACE); Check(store->text.ends_with(L"你好谢谢"),"finishing remaining segment commits entire phrase");
            penda::ReloadChineseLearning(); Check(penda::RecallChinese(L"nihaoxiexie")==L"你好谢谢","multi-segment correction learns full pinyin-to-phrase mapping");
            type(L"nihaoxiexie");key(VK_SPACE);Check(store->text.ends_with(L"你好谢谢"),"two-pick phrase recalled as a new candidate without a base dictionary entry");
            type(L"ni"); key(L'2'); Check(store->text.ends_with(alternate),"user confirms alternate Chinese candidate");
            type(L"ni"); key(VK_SPACE); Check(store->text.ends_with(alternate+alternate),"same pinyin recalls confirmed candidate first");
            std::wstring stagedKey,stagedText; for(int i=0;i<12;++i){stagedKey+=L"nihaoxiexie";stagedText+=L"你好谢谢";}
            type(stagedKey.c_str()); Check(store->text.ends_with(stagedKey),"long pinyin remains inline past 64 characters"); Check(IsWindowVisible(popup),"long pinyin keeps candidate window visible");
            key(VK_SPACE); Check(store->text.ends_with(L"你好谢谢"+stagedKey.substr(11)),"first selection uses longest learned leading group");
            for(int i=1;i<24 && IsWindowVisible(popup);++i)key(VK_SPACE); Check(store->text.ends_with(stagedText),"successive short groups assemble complete long sentence");
            penda::ReloadChineseLearning(); Check(penda::RecallChinese(stagedKey)==stagedText,"staged sentence preserves full learning record");
            type(stagedKey.c_str()); key(VK_SPACE); Check(store->text.ends_with(L"你好谢谢"+stagedKey.substr(11)),"remembered long sentence uses longest short learned group"); key(VK_ESCAPE);
            Hr(service->Deactivate(),"deactivate before learning reload"); Hr(service->ActivateEx(bridge.Get(),client,0),"reactivate learning service"); type(L"ni"); key(VK_SPACE); Check(store->text.ends_with(alternate),"learning survives service reactivation");
            type(L"nihaoxiexie");key(VK_SPACE);Check(store->text.ends_with(L"你好谢谢"),"new multi-pick word survives service reload");
            {
                penda::ComPtr<ITfLangBarItemMgr> bar; Hr(bridge->QueryInterface(IID_PPV_ARGS(&bar)),"isolated test language bar manager");
                const GUID& menuId=GUID_LBI_INPUTMODE;
                penda::ComPtr<ITfLangBarItem> item; Hr(bar->GetItem(menuId,&item),"native language bar settings registered");
                penda::ComPtr<ITfLangBarItemButton> button; Hr(item.As(&button),"settings menu interface");
                TF_LANGBARITEMINFO info{}; Hr(item->GetInfo(&info),"menu description"); Check((info.dwStyle&TF_LBI_STYLE_SHOWNINTRAY)!=0,"settings button requests tray visibility");
                Check(info.guidItem==GUID_LBI_INPUTMODE,"modern Windows input mode item identifier");
                Check((info.dwStyle&TF_LBI_STYLE_BTN_BUTTON)!=0 && (info.dwStyle&TF_LBI_STYLE_BTN_MENU)!=0,"support both native click and system-hosted menu routes");
                MenuCapture captured; Hr(button->InitMenu(&captured),"system-hosted menu population");
                Check(captured.ids==std::vector<UINT>{1,10,11,12},"system-hosted menu exposes settings and all priority languages"); Check(captured.labels[0]==L"设置与词库…","system menu settings label complete");
                ComPtr<ITfFunctionProvider> provider; Hr(service.As(&provider),"standard function provider");
                ComPtr<IUnknown> config; Hr(provider->GetFunction(GUID_NULL,IID_ITfFnConfigure,&config),"standard Windows configure function");
                ComPtr<ITfFnConfigure> configure; Hr(config.As(&configure),"configuration interface"); BSTR name=nullptr; Hr(configure->GetDisplayName(&name),"configuration display name");Check(name && SysStringLen(name)>0,"configuration name exists");SysFreeString(name);
                menuVisible=false;menuTicks=0; auto menuTimer=SetTimer(nullptr,0,20,CheckMenu); Check(menuTimer!=0,"menu test timer");
                Hr(button->OnClick(TF_LBI_CLK_RIGHT,POINT{50,50},nullptr),"native right click popup"); KillTimer(nullptr,menuTimer);
                Check(menuVisible,"right click enters native popup menu loop with an owned window"); Check(!FindWindowW(L"STATIC",L"Penda.Tsf.MenuOwner"),"popup owner cleaned up");
                Hr(button->OnMenuSelect(10),"set Chinese before recent-language tests");type(L"nihongo");key(VK_TAB);key(VK_RETURN);
                Check(store->text.ends_with(L"にほんご"),"Japanese group Enter still commits kana");
                {std::ifstream preference{std::filesystem::path(priorityTest)};int selected=-1;preference>>selected;Check(selected==1,"last Japanese commit becomes persisted first group");}
                type(L"nihongo");key(L'1');Check(store->text.ends_with(L"日本語"),"next composition starts in last committed Japanese group");
                type(L"hello");key(VK_TAB);key(VK_TAB);key(L'1');Check(store->text.ends_with(L"hello"),"English commit after Tab succeeds");
                {std::ifstream preference{std::filesystem::path(priorityTest)};int selected=-1;preference>>selected;Check(selected==2,"English commit replaces first group");}
                type(L"x");key(VK_TAB);key(VK_ESCAPE);
                {std::ifstream preference{std::filesystem::path(priorityTest)};int selected=-1;preference>>selected;Check(selected==2,"Tab and cancel do not change recent language");}
                BYTE shortcutSaved[256]{};GetKeyboardState(shortcutSaved);BYTE shortcutState[256];std::copy_n(shortcutSaved,256,shortcutState);shortcutState[VK_CONTROL]|=0x80;shortcutState[VK_MENU]|=0x80;SetKeyboardState(shortcutState);BOOL shortcutEaten=TRUE;keys->OnTestKeyDown(context.Get(),L'L',0,&shortcutEaten);SetKeyboardState(shortcutSaved);Check(!shortcutEaten,"removed Ctrl Alt L shortcut passes through");
                Hr(button->OnMenuSelect(11),"persist Japanese priority"); type(L"nihongo"); key(L'1'); Check(store->text.ends_with(L"日本語"),"Japanese priority selects Japanese without Tab");
                type(L"nihongo");key(VK_RETURN);Check(store->text.ends_with(L"にほんご"),"Japanese priority Enter commits hiragana instead of raw romaji or first kanji");
                type(L"gakkou");key(VK_RETURN);Check(store->text.ends_with(L"がっこう"),"Japanese priority Enter converts voiced and doubled consonants");
                type(L"funniki");key(L'1');Check(store->text.ends_with(L"雰囲気"),"ambiguous nn spelling selects correct dictionary kanji");
                type(L"funniki");key(VK_RETURN);Check(store->text.ends_with(L"ふんいき"),"ambiguous nn Enter uses dictionary-supported kana");
                type(L"fun'iki");key(L'1');Check(store->text.ends_with(L"雰囲気"),"explicit apostrophe spelling remains supported");
                type(L"konnichiha");key(VK_RETURN);Check(store->text.ends_with(L"こんにちは"),"normal nni spelling is not broken by ambiguous nn matching");
                type(L"fa");key(VK_RETURN);Check(store->text.ends_with(L"ふぁ"),"native TSF fa commits small-vowel hiragana");
                type(L"ofa-");key(L'1');Check(store->text.ends_with(L"オファー"),"native TSF ofa dash selects loanword from unchanged user dictionary");
                type(L"pa-thi-");key(VK_RETURN);Check(store->text.ends_with(L"ぱーてぃー"),"native TSF extended thi and long vowels compose correctly");
                type(L"dhizuni-");key(L'1');Check(store->text.ends_with(L"ディズニー"),"native TSF dhi reaches katakana dictionary entry");
                type(L"anofutari");key(VK_SPACE);Check(store->text.ends_with(L"あの二人"),"Japanese whole-phrase lattice converts interior words");
                type(L"anofutari");key(VK_RETURN);Check(store->text.ends_with(L"あのふたり"),"Japanese phrase Enter still commits pure kana");
                type(L"ogoruna");key(VK_SPACE);Check(store->text.ends_with(L"奢るな"),"Japanese particle suffix retains dictionary kanji");
                key(VK_OEM_COMMA);key(VK_OEM_PERIOD);key(VK_OEM_4);key(VK_OEM_6);Check(store->text.ends_with(L"、。「」"),"standalone Japanese punctuation uses Japanese forms");
                SetKeyboardState(shifted);key(VK_OEM_2);key(L'1');SetKeyboardState(keyboardState);Check(store->text.ends_with(L"？！"),"standalone Japanese question and exclamation are fullwidth");
                type(L"ogoruna");key(VK_OEM_PERIOD);Check(store->text.ends_with(L"奢るな。"),"Japanese punctuation commits suffix candidate without losing particle");
                type(L"ogoruna");key(VK_RETURN);Check(store->text.ends_with(L"おごるな"),"Japanese Enter still chooses pure kana despite composed kanji candidates");
                type(L"hello");key(VK_TAB);key(VK_TAB);key(VK_RETURN);Check(store->text.ends_with(L"hello"),"English group Enter remains raw despite Japanese priority");
                type(L"x");key(VK_TAB);key(VK_ESCAPE);
                type(L"hello"); key(VK_TAB); key(VK_TAB); key(L'1'); Check(store->text.ends_with(L"hello"),"Tab follows Japanese Chinese English order");
                Hr(button->OnMenuSelect(12),"persist English priority"); type(L"hello"); key(L'1'); Check(store->text.ends_with(L"hello"),"English priority selects English without Tab");
                Hr(button->OnMenuSelect(10),"restore Chinese priority");
            }
            type(L"ceping");for(int i=0;i<4;++i)penda::LearnChinese(L"ceping",L"侧评");
            for(int i=0;i<25;++i){MsgWaitForMultipleObjects(0,nullptr,FALSE,20,QS_ALLINPUT);Pump();}
            key(VK_SPACE);Check(store->text.ends_with(L"侧评"),"open composition refreshes external frequency changes without another letter");
            {
                UiHost host;ComPtr<ITfSource> source;Hr(manager.As(&source),"UI-less event source");DWORD uiCookie=TF_INVALID_COOKIE;Hr(source->AdviseSink(IID_ITfUIElementSink,&host,&uiCookie),"UI-less host advises sink");
                auto before=store->text;type(L"nihao");Check(host.starts>0&&host.updates>0,"system candidate events delivered");Check(!IsWindowVisible(TestPopup()),"host-controlled candidates suppress desktop popup");
                ComPtr<ITfUIElementMgr> uiManager;Hr(manager.As(&uiManager),"real system UI manager");ComPtr<ITfUIElement> element;Hr(uiManager->GetUIElement(host.id,&element),"system retrieves candidate element");ComPtr<ITfCandidateListUIElementBehavior> behavior;Hr(element.As(&behavior),"host reads candidate list behavior");UINT count=0;Hr(behavior->GetCount(&count),"host candidate count");Check(count>0,"host receives candidates");BSTR word=nullptr;Hr(behavior->GetString(0,&word),"host reads first word");Check(std::wstring(word)==L"你好","host sees longest Chinese candidate");SysFreeString(word);
                Hr(behavior->SetSelection(0),"host selects word");Pump();Hr(behavior->Finalize(),"host commits candidate");Pump();Check(store->text==before+L"你好","UI-less commit reaches target text store");Check(host.ends>0,"system candidate lifetime ends after commit");
                behavior.Reset();element.Reset();type(L"nihao");Hr(uiManager->GetUIElement(host.id,&element),"system retrieves second candidate element");Hr(element.As(&behavior),"second behavior");Hr(behavior->Abort(),"host cancels composition");Pump();Check(store->text==before+L"你好","UI-less cancel preserves previous committed text");
                behavior.Reset();element.Reset();source->UnadviseSink(uiCookie);
            }
            type(L"ni"); Hr(service->Deactivate(),"deactivate with active composition"); Pump(); Check(store->text.ends_with(L"ni"),"deactivation preserves unfinished raw input");
            keys.Reset(); service.Reset(); manager->SetFocus(nullptr); document->Pop(TF_POPF_ALL); context.Reset(); document.Reset(); manager->Deactivate(); manager.Reset(); DestroyWindow(store->window); store.Reset(); factory.Reset(); Pump();
        }
        Check(canUnload() == S_OK,"COM objects and edit sessions released"); FreeLibrary(dll); CoUninitialize();
        std::cout << "Real TSF text-store integration passed. Checks: " << checks << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
