#pragma once
#include "Engine.h"
#include <algorithm>
namespace penda {
// Offer complete leading dictionary words. The remaining pinyin stays in the composition.
inline std::vector<Candidate> DecodeChinese(const std::vector<Word>& words,const std::wstring& key) {
    std::vector<Candidate> result;
    for(size_t length=std::min<size_t>(32,key.size());length>0;--length) {
        auto prefix=key.substr(0,length);
        auto start=std::lower_bound(words.begin(),words.end(),prefix,[](const Word& word,const std::wstring& value){return word.key<value;});
        
        for(auto it=start;it!=words.end()&&it->key==prefix;++it) {
            if(it->text.size()>8)continue;
            result.push_back({it->text,prefix,0,1'700'000+static_cast<int>(length)*1000+std::min(it->frequency,999)});
        }
    }
    return result;
}
}
