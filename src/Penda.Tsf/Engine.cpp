#include "Engine.h"
#include "ChineseDecoder.h"
#include "JapaneseDecoder.h"
#include "RomajiTable.generated.h"
#include <shlobj.h>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <set>
#include <stdexcept>

namespace penda {
std::wstring ModuleDirectory() { wchar_t path[32768]{}; GetModuleFileNameW(module, path, 32768); return std::filesystem::path(path).parent_path().wstring(); }
static std::wstring LegacySnapshotPath() {
    PWSTR root = nullptr; std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_NO_PACKAGE_REDIRECTION, nullptr, &root))) { result = std::wstring(root) + L"\\Penda\\engine.pdat"; CoTaskMemFree(root); }
    if(result.empty()){wchar_t environment[32768]{};if(GetEnvironmentVariableW(L"LOCALAPPDATA",environment,32768))result=std::wstring(environment)+L"\\Penda\\engine.pdat";}
    return result;
}
std::wstring UserDataDirectory() {
    PWSTR profile=nullptr;
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile,KF_FLAG_NO_PACKAGE_REDIRECTION,nullptr,&profile))){auto root=(std::filesystem::path(profile)/L".penda").wstring();CoTaskMemFree(profile);return root;}
    wchar_t environment[32768]{};if(GetEnvironmentVariableW(L"USERPROFILE",environment,32768))return (std::filesystem::path(environment)/L".penda").wstring();
    return std::filesystem::path(LegacySnapshotPath()).parent_path().wstring();
}
std::wstring UserSnapshotPath() {
    auto result=(std::filesystem::path(UserDataDirectory())/L"engine.pdat").wstring();
    if(!result.empty()) {
        // Every host prefers the same published cache, including hosts with a readable old index.
        auto compatible=(std::filesystem::path(result).parent_path()/L"Runtime"/L"engine.pdat").wstring();
        HANDLE readable=CreateFileW(compatible.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(readable!=INVALID_HANDLE_VALUE){CloseHandle(readable);return compatible;}
    }
    if(GetFileAttributesW(result.c_str())==INVALID_FILE_ATTRIBUTES)return LegacySnapshotPath();
    return result;
}

bool Engine::Load(const std::wstring& path) noexcept {
    try {
        std::ifstream stream(std::filesystem::path(path), std::ios::binary);
        if (!stream) return false;
        stream.seekg(0, std::ios::end); auto size = stream.tellg();
        if (size < 24 || size > 512LL * 1024 * 1024) return false;
        stream.seekg(0);
        auto u32 = [&]() { unsigned value = 0; if (!stream.read(reinterpret_cast<char*>(&value), 4)) throw std::runtime_error("truncated"); return value; };
        auto text = [&](unsigned limit) { auto count = u32(); if (count > limit) throw std::runtime_error("length"); std::wstring value(count, L'\0'); if (!stream.read(reinterpret_cast<char*>(value.data()), count * 2)) throw std::runtime_error("text"); return value; };
        if (u32() != 0x31444E50 || u32() != 1) return false;
        Engine next; next.languages = u32(); next.latin = u32(); next.numbers = u32(); next.kana = u32();
        if (next.languages > 7 || next.latin > 1 || next.numbers > 1 || next.kana > 1) return false;
        for (auto& rows : next.words) {
            auto count = u32(); if (count > 2'000'000) return false;
            rows.reserve(count);
            for (unsigned i = 0; i < count; ++i) { auto key = text(512), value = text(1024); int frequency = static_cast<int>(u32()); if (key.empty() || value.empty() || frequency < 0 || frequency > 1'000'000) return false; rows.push_back({std::move(key), std::move(value), frequency}); }
            if (!std::is_sorted(rows.begin(), rows.end(), [](const Word& a, const Word& b) { return a.key < b.key; })) return false;
        }
        auto count = u32(); if (count > 1024) return false;
        for (unsigned i = 0; i < count; ++i) { auto key = text(8), value = text(8); next.romaji.emplace(std::move(key), std::move(value)); }
        if (stream.peek() != std::char_traits<char>::eof()) return false;
        // Older indexes embed the old map. Fill missing rules without changing
        // dictionary entries or overwriting mappings supplied by the index.
        for(const auto& pair:DefaultRomajiTable)next.romaji.try_emplace(pair.key,pair.value);
        *this = std::move(next); return true;
    } catch (...) { return false; }
}
static std::wstring Lower(std::wstring input) {
    if (input.empty()) return input;
    std::wstring output(input.size(), L'\0');
    int count = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, input.data(), static_cast<int>(input.size()), output.data(), static_cast<int>(output.size()), nullptr, nullptr, 0);
    return count > 0 ? output : input;
}
static std::wstring Katakana(std::wstring input) { for (auto& c : input) if (c >= 0x3041 && c <= 0x3096) c += 0x60; return input; }
std::wstring Engine::Hiragana(std::wstring input) const {
    input = Lower(input); std::wstring output;
    for (size_t i = 0; i < input.size();) {
        wchar_t c = input[i];
        if (c == L'm' && i+1 < input.size() && (input[i+1] == L'b' || input[i+1] == L'p')) { output += L'ん'; ++i; continue; }
        if(c==L't' && i+2<input.size() && input[i+1]==L'c' && input[i+2]==L'h') {output+=L'っ';++i;continue;}
        if (i + 1 < input.size() && c == input[i + 1] && c != L'n' && std::wstring_view(L"bcdfghjklmpqrstvwxyz").find(c) != std::wstring_view::npos) { output += L'っ'; ++i; continue; }
        if (c == L'n' && (i + 1 == input.size() || (input[i + 1] != L'n' && std::wstring_view(L"aiueoy'").find(input[i + 1]) == std::wstring_view::npos))) { output += L'ん'; ++i; continue; }
        if (c == L'n' && i + 2 < input.size() && input[i + 1] == L'n' && std::wstring_view(L"aiueoy").find(input[i + 2]) != std::wstring_view::npos) { output += L'ん'; ++i; continue; }
        bool found = false;
        for (size_t length = std::min<size_t>(4, input.size() - i); length; --length) { auto entry = romaji.find(input.substr(i, length)); if (entry != romaji.end()) { output += entry->second; i += length; found = true; break; } }
        if (!found) { output += c; ++i; }
    }
    return output;
}
std::wstring Engine::Width(const std::wstring& input) const {
    std::wstring output, expanded;
    if (kana) {
        for (size_t i = 0; i < input.size();) {
            if (input[i] >= 0xFF61 && input[i] <= 0xFF9F) {
                size_t start = i; while (i < input.size() && input[i] >= 0xFF61 && input[i] <= 0xFF9F) ++i;
                std::wstring full((i-start)*2+2,L'\0');
                int count = LCMapStringEx(L"ja-JP",LCMAP_FULLWIDTH,input.data()+start,static_cast<int>(i-start),full.data(),static_cast<int>(full.size()),nullptr,nullptr,0);
                if (count > 0) expanded.append(full.data(),count); else expanded.append(input,start,i-start);
            } else expanded += input[i++];
        }
    }
    for (wchar_t c : kana ? expanded : input) {
        if ((c >= 0xFF10 && c <= 0xFF19) || (c >= 0xFF21 && c <= 0xFF3A) || (c >= 0xFF41 && c <= 0xFF5A)) c -= 0xFEE0;
        if (c >= L'0' && c <= L'9') output += numbers ? static_cast<wchar_t>(c + 0xFEE0) : c;
        else if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z')) output += latin ? static_cast<wchar_t>(c + 0xFEE0) : c;
        else if (!kana && (c == 0x3099 || c == 0x309A)) output += c == 0x3099 ? L'ﾞ' : L'ﾟ';
        else if (!kana && ((c >= 0x3041 && c <= 0x3096) || (c >= 0x30A1 && c <= 0x30FC) || (c >= 0x3001 && c <= 0x3002) || (c >= 0x300C && c <= 0x300D))) {
            if (c >= 0x3041 && c <= 0x3096) c += 0x60;
            wchar_t converted[8]{}; int count = LCMapStringEx(L"ja-JP", LCMAP_HALFWIDTH, &c, 1, converted, 8, nullptr, nullptr, 0);
            if (count > 0) output.append(converted, count); else output += c;
        } else output += c;
    }
    return output;
}
std::vector<Candidate> Engine::Query(const std::wstring& input, unsigned mask) const {
    if (input.empty()) return {};
    auto key = Lower(input); std::erase(key, L'\'');
    std::array<std::vector<Candidate>, 3> groups;
    for (int language = 0; language < 3; ++language) {
        if (!((mask == 0xFFFFFFFF ? languages : mask) & (1u << language))) continue;
        auto& group = groups[language]; const auto& rows = words[language];
        std::vector<std::wstring> keys{key}; if (language == 1) { auto kanaKey = Hiragana(input); if (kanaKey != key) keys.push_back(kanaKey); }
        for (const auto& lookupKey : keys) {
        auto start = std::lower_bound(rows.begin(), rows.end(), lookupKey, [](const Word& word, const std::wstring& value) { return word.key < value; });
        int scanned = 0;
        for (auto it = start; it != rows.end() && it->key.starts_with(lookupKey) && (it->key==lookupKey || scanned < 192); ++it, ++scanned) group.push_back({it->text, language==0 ? lookupKey : it->key, language, (it->key == lookupKey ? 2'000'000 : 0) + it->frequency});
        }
        if (language == 0 && key.size()>=1 && key.size()<=512) { auto decoded=DecodeChinese(rows,key); group.insert(group.end(),decoded.begin(),decoded.end()); }
        if (language == 1) {
            auto hiragana = Hiragana(input);
            const bool exact=std::any_of(group.begin(),group.end(),[](const Candidate& item){return item.score>=2'000'000;});
            if(!exact){auto decoded=DecodeJapanese(rows,hiragana,key);group.insert(group.end(),decoded.begin(),decoded.end());}
            group.push_back({hiragana, input, language, 1'500'000}); group.push_back({Katakana(hiragana), input, language, 1'400'000});
        }
        if (language == 2) group.push_back({input, input, language, 1'600'000});
        if(language==0) std::erase_if(group,[](const Candidate& candidate){return candidate.text.size()>8;});
        std::stable_sort(group.begin(), group.end(), [&](const Candidate& a, const Candidate& b) { if(language==0){auto al=key.starts_with(a.key)?a.key.size():0,bl=key.starts_with(b.key)?b.key.size():0;if(al!=bl)return al>bl;} return a.score > b.score; });
        std::set<std::wstring> seen; std::erase_if(group, [&](const Candidate& item) { return !seen.insert(item.text).second; });

    }
    std::vector<Candidate> result;
    size_t ranks=0; for(const auto& group:groups) ranks=std::max(ranks,group.size());
    for (size_t rank = 0; rank < ranks; ++rank) for (const auto& group : groups) if (rank < group.size()) { auto item = group[rank]; item.text = Width(item.text); result.push_back(std::move(item)); }
    return result;
}
}
