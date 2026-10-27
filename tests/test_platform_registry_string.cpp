#include "platform/system.h"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << "\n";
        ++failures;
    }
}

} // namespace

int main() {
    using Platform::ReadRegistryString;
    using Platform::RegistryHive;
    using Platform::RegistryStringStatus;

    Check(ReadRegistryString(RegistryHive::CurrentUser, "", "value").status ==
              RegistryStringStatus::InvalidArgument,
          "empty subkey is invalid");
    Check(ReadRegistryString(RegistryHive::CurrentUser,
                             std::string("bad\0key", 7), "value").status ==
              RegistryStringStatus::InvalidArgument,
          "embedded subkey NUL is invalid");
    Check(ReadRegistryString(RegistryHive::CurrentUser, "Software",
                             std::string("bad\0value", 9)).status ==
              RegistryStringStatus::InvalidArgument,
          "embedded value-name NUL is invalid");
    Check(ReadRegistryString(static_cast<RegistryHive>(99), "Software", "value").status ==
              RegistryStringStatus::InvalidArgument,
          "unknown hive is invalid");
    Check(ReadRegistryString(RegistryHive::CurrentUser,
                             std::string(1, static_cast<char>(0xFF)), "value").status ==
              RegistryStringStatus::EncodingFailed,
          "malformed UTF-8 subkey is rejected");
    Check(ReadRegistryString(RegistryHive::CurrentUser, "Software",
                             std::string(1, static_cast<char>(0xFF))).status ==
              RegistryStringStatus::EncodingFailed,
          "malformed UTF-8 value name is rejected");

    const auto defaultValue = ReadRegistryString(
        RegistryHive::CurrentUser, "Software\\HarkenSearchStage6MissingRegistryProbe", "");
    Check(defaultValue.status == RegistryStringStatus::NotFound,
          "empty value name queries the default value without mutation");

    const auto productName = ReadRegistryString(
        RegistryHive::LocalMachine,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName");
    Check(productName.status == RegistryStringStatus::Success &&
              !productName.valueUtf8.empty(),
          "known machine REG_SZ returns strict UTF-8");

    if (failures == 0) std::cout << "All registry-string PAL tests passed\n";
    return failures == 0 ? 0 : 1;
}
