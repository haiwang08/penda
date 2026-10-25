#pragma once
#include "Engine.h"
#include <fstream>
#include <filesystem>
#include <mutex>
#include <algorithm>
#include <tuple>
namespace penda {
struct Usage { unsigned count=0, recent=0; };
struct LearningState { std::map<std::wstring,std::map<std::wstring,Usage>> words; FILETIME stamp{}; DWORD size=0; std::wstring path; unsigned sequence=0; };
inline std::mutex learningMutex;
inline std::array<LearningState,3> learning;
inline std::wstring LearningPath(int language=0) {
    wchar_t test[32768]{};
    if(GetEnvironmentVariableW(L"PENDA_LEARNING_PATH",test,32768))return std::wstring(test)+(language==0?L"":language==1?L".japanese":L".english");
    return (std::filesystem::path(UserDataDirectory())/(language==0?L"learned-chinese.tsv":language==1?L"learned-japanese.tsv":L"learned-english.tsv")).wstring();
}
inline bool ReloadChineseLearning() {
    std::lock_guard lock(learningMutex);bool changed=false;
    for(int language=0;language<3;++language){
        auto path=LearningPath(language);WIN32_FILE_ATTRIBUTE_DATA info{};auto& state=learning[language];
        if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&info))continue;
        if(path==state.path&&info.nFileSizeLow==state.size&&CompareFileTime(&state.stamp,&info.ftLastWriteTime)==0)continue;
        if(info.nFileSizeHigh||info.nFileSizeLow>8*1024*1024)continue;
        std::ifstream stream(std::filesystem::path(path),std::ios::binary);if(!stream)continue;
        LearningState next;next.path=path;next.stamp=info.ftLastWriteTime;next.size=info.nFileSizeLow;std::string line;
        while(std::getline(stream,line)){
            if(stream.eof())break; // Ignore an incomplete append from another process.
            int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,line.data(),static_cast<int>(line.size()),nullptr,0);if(count<=0)continue;
            std::wstring value(count,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,line.data(),static_cast<int>(line.size()),value.data(),count);
            auto split=value.find(L'\t');if(split==std::wstring::npos||split==0||split>512||value.size()-split>1025)continue;
            auto& usage=next.words[value.substr(0,split)][value.substr(split+1)];++usage.count;usage.recent=++next.sequence;
        }
        state=std::move(next);changed=true;
    }return changed;
}
inline std::wstring RecallChinese(const std::wstring& key){
    std::lock_guard lock(learningMutex);auto found=learning[0].words.find(key);if(found==learning[0].words.end())return L"";
    auto best=std::max_element(found->second.begin(),found->second.end(),[](const auto& a,const auto& b){return std::tie(a.second.count,a.second.recent)<std::tie(b.second.count,b.second.recent);});return best->first;
}
inline void LearnWord(int language,std::wstring key,const std::wstring& text) noexcept {
    try {
        if(language<0||language>2)return;std::erase(key,L'\'');for(auto& c:key)if(c>=L'A'&&c<=L'Z')c+=32;
        if(key.empty()||key.size()>512||text.empty()||text.size()>1024||text.find_first_of(L"\t\r\n")!=std::wstring::npos)return;
        if(language==0&&std::any_of(text.begin(),text.end(),[](wchar_t c){return c>=L'a'&&c<=L'z'||c>=L'A'&&c<=L'Z';}))return;
        {std::lock_guard lock(learningMutex);auto& state=learning[language];auto& usage=state.words[key][text];++usage.count;usage.recent=++state.sequence;}
        auto record=key+L"\t"+text+L"\n";int count=WideCharToMultiByte(CP_UTF8,0,record.data(),static_cast<int>(record.size()),nullptr,0,nullptr,nullptr);std::string bytes(count,'\0');WideCharToMultiByte(CP_UTF8,0,record.data(),static_cast<int>(record.size()),bytes.data(),count,nullptr,nullptr);
        auto path=LearningPath(language);std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        HANDLE file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE){LARGE_INTEGER size{};DWORD written{};if(GetFileSizeEx(file,&size)&&size.QuadPart<8*1024*1024)WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr);CloseHandle(file);}
    }catch(...){}
}
inline void LearnChinese(std::wstring key,const std::wstring& text){LearnWord(0,std::move(key),text);}
inline void RankUsage(std::vector<Candidate>& candidates,std::wstring key,unsigned languages){
    for(auto& c:key)if(c>=L'A'&&c<=L'Z')c+=32;std::erase(key,L'\'');
    std::lock_guard lock(learningMutex);
    if(languages&1u)for(size_t length=1;length<=key.size();++length){auto prefix=key.substr(0,length);auto found=learning[0].words.find(prefix);if(found==learning[0].words.end())continue;
        for(const auto& [text,usage]:found->second)if(text.size()<=8&&std::none_of(candidates.begin(),candidates.end(),[&](const Candidate& item){return item.language==0&&item.key==prefix&&item.text==text;}))candidates.push_back({text,prefix,0,1'700'000});
    }
    if(languages&2u){auto found=learning[1].words.find(key);if(found!=learning[1].words.end())for(const auto& [text,usage]:found->second)
        if(std::none_of(candidates.begin(),candidates.end(),[&](const Candidate& item){return item.language==1&&item.key==key&&item.text==text;}))candidates.push_back({text,key,1,3'200'000});}
    auto weight=[&](const Candidate& item){auto normalized=item.key;for(auto& c:normalized)if(c>=L'A'&&c<=L'Z')c+=32;auto found=learning[item.language].words.find(normalized);if(found==learning[item.language].words.end())return Usage{};auto word=found->second.find(item.text);return word==found->second.end()?Usage{}:word->second;};
    std::array<std::vector<Candidate>,3> groups;for(auto& item:candidates)groups[item.language].push_back(std::move(item));
    for(auto& group:groups)std::stable_sort(group.begin(),group.end(),[&](const Candidate& a,const Candidate& b){
        if(a.language==0){auto al=key.starts_with(a.key)?a.key.size():0,bl=key.starts_with(b.key)?b.key.size():0;if(al!=bl)return al>bl;}
        auto aw=weight(a),bw=weight(b);if(aw.count!=bw.count)return aw.count>bw.count;if(aw.recent!=bw.recent)return aw.recent>bw.recent;return a.score>b.score;
    });
    candidates.clear();size_t count=0;for(const auto& group:groups)count=std::max(count,group.size());for(size_t i=0;i<count;++i)for(auto& group:groups)if(i<group.size())candidates.push_back(std::move(group[i]));
}
}
