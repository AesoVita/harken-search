#pragma once

#include <string>

namespace aura::database {

std::string NormalizeFolderPathForDb(const std::string& pathUtf8);

}  // namespace aura::database
