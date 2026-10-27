#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace aura::search {

struct CoverKeyFilterRecord {
    std::string filePath;
    std::string embeddedCoverKey;
    std::string folderCoverKey;
    bool cachedMatchAvailable = false;
    bool cachedMatch = false;
};

struct CoverKeyFilterInput {
    bool active = false;
    bool databaseView = false;
    bool browserView = false;
    std::string key;
    std::vector<CoverKeyFilterRecord> records;
};

class CoverKeyFilterEffects {
public:
    virtual ~CoverKeyFilterEffects() = default;
    virtual bool fileHasCover(const std::string& filePath, const std::string& key) = 0;
};

std::vector<std::size_t> HandleCoverKeyFilter(
    const CoverKeyFilterInput& input,
    CoverKeyFilterEffects& effects);

}  // namespace aura::search
