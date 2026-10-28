#include "CoverWallPageResultBuilder.h"

#include <iostream>
#include <filesystem>

std::filesystem::path Utf8ToPath(const std::string& value) {
    return std::filesystem::u8path(value);
}
std::string PathToUtf8(const std::filesystem::path& value) {
    return value.u8string();
}

namespace {
Database::CoverWallPage nextPage;
}

Database::Database() = default;
Database::~Database() = default;
bool Database::openReadOnly(const std::string&) { return true; }
Database::CoverWallPage Database::getGlobalCoverWallEntriesPage(const std::string&, int, bool) {
    return nextPage;
}

namespace CoverModule {
bool GetFolderCoverImagePathBackground(const std::string& folder,
                                       std::string& out,
                                       bool,
                                       bool) {
    if (folder.find("repeat") != std::string::npos) {
        out = "C:/covers/shared.jpg";
    } else if (folder.find("other") != std::string::npos) {
        out = "C:/covers/other.jpg";
    } else {
        out.clear();
    }
    return !out.empty();
}
}

namespace aura::cover_wall {
void SanitizeCoverWallItemsForCache(std::vector<CoverModule::CoverItem>&) {}
void BuildCoverWallItemsFromCachedKeys(const std::vector<AudioRecord>&,
                                       std::vector<CoverModule::CoverItem>&) {}
}

int main() {
    nextPage.entries = {
        {"?|C:/repeat/a", "C:/repeat/a", "", false},
        {"?|C:/repeat/b", "C:/repeat/b", "", false},
        {"?|C:/other", "C:/other", "", false},
    };
    nextPage.hasMore = true;
    nextPage.nextCursor = "fl2:123";
    const auto first = aura::cover_wall::LoadGlobalCoverWallPage("isolated.db", {}, 512);
    const bool onePerSource = first.items.size() == 2 && first.hasMore &&
                              first.nextCursor == "fl2:123" &&
                              first.items[0].key == "?|C:/repeat/a" &&
                              first.items[0].folderCoverSrcPathUtf8 == "C:/covers/shared.jpg";

    nextPage.entries = {{"?|C:/repeat/c", "C:/repeat/c", "", false}};
    const auto second = aura::cover_wall::LoadGlobalCoverWallPage("isolated.db", "fl2:123", 512);
    const bool sameAcrossPages = second.items.size() == 1 &&
        aura::cover_wall::BuildCoverWallVisualDedupeToken(first.items[0]) ==
        aura::cover_wall::BuildCoverWallVisualDedupeToken(second.items[0]);

    const auto distinct = aura::cover_wall::BuildCoverWallPageLoadResult({{
        {"?|C:/similar/a", "C:/similar/a", "C:/covers/similar-a.jpg", false},
        {"?|C:/similar/b", "C:/similar/b", "C:/covers/similar-b.jpg", false},
    }});
    const bool distinctFilesRemain = distinct.items.size() == 2;

    std::cout << "same source once=" << onePerSource
              << " across pages=" << sameAcrossPages
              << " distinct files=" << distinctFilesRemain << '\n';
    return onePerSource && sameAcrossPages && distinctFilesRemain ? 0 : 1;
}
