#include "CoverKeyFilterWorkflow.h"

namespace aura::search {

std::vector<std::size_t> HandleCoverKeyFilter(
    const CoverKeyFilterInput& input,
    CoverKeyFilterEffects& effects) {
    std::vector<std::size_t> kept;
    if (!input.active || input.databaseView || input.browserView) {
        kept.resize(input.records.size());
        for (std::size_t i = 0; i < kept.size(); ++i) kept[i] = i;
        return kept;
    }
    for (std::size_t i = 0; i < input.records.size(); ++i) {
        const auto& record = input.records[i];
        if (record.cachedMatchAvailable) {
            if (record.cachedMatch) kept.push_back(i);
            continue;
        }
        bool keep = record.embeddedCoverKey == input.key || record.folderCoverKey == input.key;
        if (!record.embeddedCoverKey.empty() || !record.folderCoverKey.empty()) {
            if (keep) kept.push_back(i);
            continue;
        }
        if (effects.fileHasCover(record.filePath, input.key)) kept.push_back(i);
    }
    return kept;
}

}  // namespace aura::search
