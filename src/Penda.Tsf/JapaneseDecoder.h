#pragma once
#include "Engine.h"
#include <algorithm>
namespace penda {
// Bounded lattice search over every reading position; unknown spans remain intact.
inline std::vector<Candidate> DecodeJapanese(const std::vector<Word>& words,const std::wstring& reading,const std::wstring& key) {
    struct Path { std::wstring text; int cost; bool changed; };
    constexpr size_t beam=12,variants=8,maxWord=64;
    std::vector<std::vector<Path>> paths(reading.size()+1);paths[0].push_back({L"",0,false});
    auto add=[&](size_t position,Path value) {
        if(value.text.size()>1024)return;
        auto& list=paths[position];
        auto same=std::find_if(list.begin(),list.end(),[&](const Path& p){return p.text==value.text;});
        if(same!=list.end()){if(same->cost<=value.cost)return;list.erase(same);}
        auto at=std::lower_bound(list.begin(),list.end(),value,[](const Path& a,const Path& b){return a.cost!=b.cost?a.cost<b.cost:a.text<b.text;});
        list.insert(at,std::move(value));if(list.size()>beam)list.pop_back();
    };
    for(size_t i=0;i<reading.size();++i) {
        for(const auto& path:paths[i])add(i+1,{path.text+reading.substr(i,1),path.cost+1200,path.changed});
        for(size_t length=1;length<=std::min(maxWord,reading.size()-i);++length){
            auto part=reading.substr(i,length);
            auto first=std::lower_bound(words.begin(),words.end(),part,[](const Word& w,const std::wstring& value){return w.key<value;});
            if(first==words.end()||!first->key.starts_with(part))break;
            size_t count=0;
            for(auto it=first;it!=words.end()&&it->key==part&&count<variants;++it,++count){
                bool kanji=std::any_of(it->text.begin(),it->text.end(),[](wchar_t c){return c>=0x3400&&c<=0x9FFF;});
                int cost=90+(1'000'000-it->frequency)/1000-(kanji&&length>=2?20:0);
                for(const auto& path:paths[i])add(i+length,{path.text+it->text,path.cost+cost,path.changed||it->text!=part});
            }
        }
    }
    std::vector<Candidate> result;
    for(const auto& path:paths.back())if(path.changed)result.push_back({path.text,key,1,3'100'000-path.cost});
    return result;
}
}
