#include "BrowserSystemFolderSearchPolicy.h"

#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace {

bool Check(bool condition, const char* message) {
    if (condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

} // namespace

int main() {
    using aura::browser::BuildSystemFolderPathChain;
    using aura::browser::BuildSystemFolderSearchTree;
    using aura::browser::IsSystemFolderAncestorOf;
    using aura::browser::MatchesSystemFolderSearchQuery;
    using aura::browser::SystemFolderSearchHit;

    if (!Check(
            MatchesSystemFolderSearchQuery("Anything", "C:\\Library", {}),
            "empty terms should match every folder")) return 1;
    if (!Check(
            MatchesSystemFolderSearchQuery(
                "BBC Impacts", "C:\\Library\\Metal", {"bbc", "metal"}),
            "all lowercased terms may match across display name and path")) return 1;
    if (!Check(
            MatchesSystemFolderSearchQuery(
                "BBC IMPACTS", "C:\\LIBRARY", {"bbc", "library"}),
            "display name and path matching should be case insensitive")) return 1;
    if (!Check(
            !MatchesSystemFolderSearchQuery(
                "BBC Impacts", "C:\\Library\\Metal", {"bbc", "wood"}),
            "every term should be required")) return 1;

    if (!Check(
            IsSystemFolderAncestorOf("C:\\Library", "C:\\Library\\SFX") &&
                IsSystemFolderAncestorOf("C:/Library", "C:/Library/SFX"),
            "slash and backslash boundaries should identify ancestors")) return 1;
    if (!Check(
            !IsSystemFolderAncestorOf("C:\\Lib", "C:\\Library") &&
                !IsSystemFolderAncestorOf("C:\\Library", "C:\\Library") &&
                !IsSystemFolderAncestorOf("", "C:\\Library"),
            "prefixes, identical paths, and empty paths should not be ancestors")) return 1;

    const auto driveChain = BuildSystemFolderPathChain("c:/Library/SFX/");
#ifdef _WIN32
    if (!Check(
            driveChain == std::vector<std::pair<std::string, std::string>>{
                {"C:\\", "C:\\"},
                {"C:\\Library", "Library"},
                {"C:\\Library\\SFX", "SFX"}},
            "drive chains should normalize separators, uppercase drives, and trim trailing separators")) return 1;
#else
    if (!Check(
            driveChain == std::vector<std::pair<std::string, std::string>>{
                {"c:", "c:"},
                {"c:\\Library", "Library"},
                {"c:\\Library\\SFX", "SFX"}},
            "non-Windows builds should preserve the fallback chain behavior")) return 1;
#endif
    if (!Check(
            BuildSystemFolderPathChain("alpha/beta/") ==
                std::vector<std::pair<std::string, std::string>>{
                    {"alpha", "alpha"}, {"alpha\\beta", "beta"}},
            "relative paths should use the component fallback chain")) return 1;
    if (!Check(
            BuildSystemFolderPathChain("").empty(),
            "empty paths should produce an empty chain")) return 1;

    const std::vector<SystemFolderSearchHit> hits{
        {"C:\\Root\\Beta", "Beta first", "ignored-parent", true},
        {"C:\\Root\\Alpha", "Alpha hit", "ignored-parent", false},
        {"C:\\Root\\Beta", "Beta last", "ignored-parent", false},
    };
    const auto tree = BuildSystemFolderSearchTree(hits);
    if (!Check(tree.size() == 1, "shared ancestors should merge by full path")) return 1;
    const auto& drive = tree.begin()->second;
    const auto rootIt = drive.children.find("C:\\Root");
    if (!Check(rootIt != drive.children.end(), "tree should contain the shared root")) return 1;
    const auto& root = rootIt->second;
    if (!Check(
            !drive.directHit && !root.directHit,
            "only terminal hit nodes should be direct hits")) return 1;
    if (!Check(root.children.size() == 2, "terminal siblings should share their ancestors")) return 1;
    const auto firstChild = root.children.begin();
    if (!Check(
            firstChild->first == "C:\\Root\\Alpha" &&
                std::next(firstChild)->first == "C:\\Root\\Beta",
            "tree children should retain deterministic map ordering")) return 1;
    const auto& alpha = firstChild->second;
    const auto& beta = std::next(firstChild)->second;
    if (!Check(
            alpha.directHit && beta.directHit &&
                alpha.displayName == "Alpha hit" && beta.displayName == "Beta last",
            "terminal hits should be marked and the last non-empty display name should win")) return 1;
    if (!Check(
            !alpha.isFile && !beta.isFile &&
                alpha.parentFolder == alpha.path && beta.parentFolder == beta.path,
            "tree projection should preserve current folder-only node semantics")) return 1;

    std::cout << "PASS: browser system folder search policy contracts\n";
    return 0;
}
