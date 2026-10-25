#pragma once
#include "Engine.h"
#include <algorithm>

namespace penda {
// Two rows per language, five columns each. Digits select within the active language.
struct CandidatePanel {
    std::array<std::vector<int>,3> groups;
    int page = 0;
    explicit CandidatePanel(const std::vector<Candidate>& rows, int currentPage = 0) : page(currentPage) {
        for (int i = 0; i < static_cast<int>(rows.size()); ++i) groups[rows[i].language].push_back(i);
    }
    int Pages() const { size_t count = 0; for (const auto& group : groups) count = std::max(count,group.size()); return std::max(1,static_cast<int>((count+9)/10)); }
    int At(int language, int row, int column) const {
        if (language < 0 || language > 2 || row < 0 || row > 1 || column < 0 || column > 4) return -1;
        int rank = page*10 + row*5 + column;
        return rank < static_cast<int>(groups[language].size()) ? groups[language][rank] : -1;
    }
    int Digit(int number, bool shift, int language = 0) const { return number >= 1 && number <= 5 ? At(language,shift ? 1 : 0,number-1) : -1; }
    std::vector<int> Visible() const {
        std::vector<int> result;
        for (int language = 0; language < 3; ++language) for (int row = 0; row < 2; ++row) for (int column = 0; column < 5; ++column) { int index = At(language,row,column); if (index >= 0) result.push_back(index); }
        return result;
    }
    int PageOf(int index) const {
        for (const auto& group : groups) { auto found = std::find(group.begin(),group.end(),index); if (found != group.end()) return static_cast<int>(found-group.begin())/10; }
        return 0;
    }
};
}
