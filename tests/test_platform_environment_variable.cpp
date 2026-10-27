#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "platform/system.h"

#include <iostream>
#include <iterator>
#include <string>
#include <utility>

namespace {

class EnvironmentRestore {
public:
    explicit EnvironmentRestore(std::wstring name) : name_(std::move(name)) {
        wchar_t buffer[32768] = {};
        SetLastError(ERROR_SUCCESS);
        const DWORD length = GetEnvironmentVariableW(
            name_.c_str(), buffer, static_cast<DWORD>(std::size(buffer)));
        existed_ = length > 0 || GetLastError() != ERROR_ENVVAR_NOT_FOUND;
        if (length > 0 && length < std::size(buffer)) {
            value_.assign(buffer, length);
        }
    }

    ~EnvironmentRestore() {
        SetEnvironmentVariableW(name_.c_str(), existed_ ? value_.c_str() : nullptr);
    }

private:
    std::wstring name_;
    bool existed_ = false;
    std::wstring value_;
};

int failures = 0;

void Check(bool condition, const char* message) {
    if (condition) {
        std::cout << "PASS: " << message << '\n';
        return;
    }
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

} // namespace

int main() {
    using Platform::EnvironmentStringStatus;
    using Platform::ReadEnvironmentVariable;

    const std::wstring nameWide = L"ARIS_STAGE6_ENVIRONMENT_PAL_TEST_" +
        std::to_wstring(GetCurrentProcessId());
    const std::string nameUtf8 = "ARIS_STAGE6_ENVIRONMENT_PAL_TEST_" +
        std::to_string(GetCurrentProcessId());
    EnvironmentRestore restore(nameWide);

    Check(SetEnvironmentVariableW(nameWide.c_str(), nullptr) != 0,
          "test variable can be removed process-locally");
    Check(ReadEnvironmentVariable(nameUtf8).status ==
              EnvironmentStringStatus::NotFound,
          "a missing variable reports NotFound");

    Check(SetEnvironmentVariableW(nameWide.c_str(), L"") != 0,
          "an empty test variable can be set process-locally");
    const auto empty = ReadEnvironmentVariable(nameUtf8);
    Check(empty.status == EnvironmentStringStatus::Success &&
              empty.valueUtf8.empty() && empty.systemError == ERROR_SUCCESS,
          "an existing empty variable reports Success with an empty value");

    Check(SetEnvironmentVariableW(nameWide.c_str(), L"enabled") != 0,
          "a nonempty test variable can be set process-locally");
    const auto enabled = ReadEnvironmentVariable(nameUtf8);
    Check(enabled.status == EnvironmentStringStatus::Success &&
              enabled.valueUtf8 == "enabled",
          "a nonempty variable returns strict UTF-8");

    return failures == 0 ? 0 : 1;
}
