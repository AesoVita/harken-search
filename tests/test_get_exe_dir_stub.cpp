#include <filesystem>
#include <string>

#include "platform/system.h"

namespace Platform {

std::string GetExeDir()
{
    std::error_code ec;
    const auto path = std::filesystem::current_path(ec);
    return ec ? std::string() : path.u8string();
}

} // namespace Platform
