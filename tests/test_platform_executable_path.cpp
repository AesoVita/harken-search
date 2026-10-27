#include "platform/executable_path.h"
#include "platform/text_encoding.h"

#include <filesystem>
#include <iostream>

int main() {
    const Platform::ExecutablePathResult result = Platform::GetExecutablePath();
    if (result.status != Platform::ExecutablePathStatus::Success ||
        result.pathUtf8.empty() || result.systemError != 0) {
        std::cerr << "FAIL: current executable path query did not succeed\n";
        return 1;
    }

    const std::wstring wide =
        Platform::TextEncoding::Utf8ToWideStrict(result.pathUtf8);
    if (wide.empty()) {
        std::cerr << "FAIL: executable path is not strict UTF-8\n";
        return 1;
    }
    const std::filesystem::path path(wide);
    if (!path.is_absolute() || path.filename().empty()) {
        std::cerr << "FAIL: executable path is not an absolute file path\n";
        return 1;
    }
    if (Platform::TextEncoding::WideToUtf8Strict(wide) != result.pathUtf8) {
        std::cerr << "FAIL: executable path did not round trip\n";
        return 1;
    }

    std::cout << "PASS: platform executable path contract\n";
    return 0;
}
