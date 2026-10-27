#include "SearchKeywordCompiler.h"

#include "SearchIntent.h"

namespace aura::search {

std::string CompileSearchBufferKeywordForFilter(const std::string& searchText) {
    auto compiled = HarkenSearch::SearchIntent::CompileSlashSearchInputToKeyword(searchText);
    return compiled.ok ? compiled.keyword : searchText;
}

}  // namespace aura::search
