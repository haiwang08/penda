#pragma once
#include <windows.h>
#include <array>
#include <string>
#include <vector>
#include <map>

namespace penda {
struct Word { std::wstring key, text; int frequency; };
struct Candidate { std::wstring text, key; int language, score; };
struct Engine {
    unsigned languages = 7, latin = 0, numbers = 0, kana = 1;
    std::array<std::vector<Word>, 3> words;
    std::map<std::wstring, std::wstring> romaji;
    bool Load(const std::wstring& path) noexcept;
    std::wstring Hiragana(std::wstring input) const;
    std::wstring Width(const std::wstring& input) const;
    std::vector<Candidate> Query(const std::wstring& input, unsigned mask = 0xFFFFFFFF) const;
};
std::wstring ModuleDirectory();
std::wstring UserSnapshotPath();
std::wstring UserDataDirectory();
extern HMODULE module;
}
