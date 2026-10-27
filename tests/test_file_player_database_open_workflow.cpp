#include "FilePlayerDatabaseOpenWorkflow.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

bool Check(bool condition, const char* message) {
    if (condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

} // namespace

int main() {
    using aura::file_player::DatabaseOpenAttemptResult;
    using aura::file_player::OpenFilePlayerDatabaseWithFallback;

    std::vector<std::string> calls;
    auto primarySuccess = [&](const std::string& path) {
        calls.push_back(path);
        return DatabaseOpenAttemptResult{true, {}};
    };
    auto result = OpenFilePlayerDatabaseWithFallback(
        "primary.db", "fallback.db", primarySuccess);
    if (!Check(result.opened && !result.fallbackAttempted,
               "primary success should not attempt fallback")) return 1;
    if (!Check(result.activePathUtf8 == "primary.db" && result.error.empty(),
               "primary success should retain primary path without error")) return 1;
    if (!Check(calls == std::vector<std::string>{"primary.db"},
               "primary success should call once")) return 1;

    calls.clear();
    auto fallbackSuccess = [&](const std::string& path) {
        calls.push_back(path);
        if (path == "primary.db") {
            return DatabaseOpenAttemptResult{false, "primary failure"};
        }
        return DatabaseOpenAttemptResult{true, {}};
    };
    result = OpenFilePlayerDatabaseWithFallback(
        "primary.db", "fallback.db", fallbackSuccess);
    if (!Check(result.opened && result.fallbackAttempted,
               "fallback success should report both states")) return 1;
    if (!Check(result.activePathUtf8 == "fallback.db" && result.error.empty(),
               "fallback success should select fallback and clear error")) return 1;
    if (!Check(calls == std::vector<std::string>{"primary.db", "fallback.db"},
               "fallback success should preserve attempt order")) return 1;

    calls.clear();
    auto bothFail = [&](const std::string& path) {
        calls.push_back(path);
        return DatabaseOpenAttemptResult{
            false, path == "primary.db" ? "primary failure" : "fallback failure"};
    };
    result = OpenFilePlayerDatabaseWithFallback(
        "primary.db", "fallback.db", bothFail);
    if (!Check(!result.opened && result.fallbackAttempted,
               "both failures should report fallback attempted")) return 1;
    if (!Check(
            result.activePathUtf8 == "primary.db" &&
                result.error == "fallback failure",
            "both failures should keep primary path and latest error")) return 1;
    if (!Check(calls == std::vector<std::string>{"primary.db", "fallback.db"},
               "both failures should attempt each path once")) return 1;

    calls.clear();
    auto samePathFailure = [&](const std::string& path) {
        calls.push_back(path);
        return DatabaseOpenAttemptResult{false, "same path failure"};
    };
    result = OpenFilePlayerDatabaseWithFallback(
        "same.db", "same.db", samePathFailure);
    if (!Check(!result.opened && !result.fallbackAttempted,
               "equal paths should suppress duplicate fallback")) return 1;
    if (!Check(
            result.activePathUtf8 == "same.db" &&
                result.error == "same path failure",
            "equal-path failure should preserve primary result")) return 1;
    if (!Check(calls == std::vector<std::string>{"same.db"},
               "equal paths should be attempted once")) return 1;

    std::cout << "PASS: file player database open workflow contracts\n";
    return 0;
}
