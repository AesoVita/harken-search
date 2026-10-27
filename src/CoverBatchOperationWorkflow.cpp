#include "CoverBatchOperationWorkflow.h"

#include <cstddef>

#define XXH_INLINE_ALL
#include "../thirdparty/xxhash.h"

namespace aura::cover_batch {
namespace {

std::string Hex64(std::uint64_t value) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string result(16, '0');
    for (int index = 15; index >= 0; --index) {
        result[static_cast<std::size_t>(index)] =
            kHex[static_cast<std::size_t>(value & 0xF)];
        value >>= 4;
    }
    return result;
}

std::string BuildEmbeddedCoverKey(const std::vector<std::uint8_t>& data,
                                  const std::string& mime) {
    const auto hash = static_cast<std::uint64_t>(
        XXH3_64bits(data.data(), data.size()));
    return "E|" + Hex64(hash) + "|" + std::to_string(data.size()) + "|" + mime;
}

}  // namespace

std::string ResolveCoverImageMime(const std::vector<std::uint8_t>& data,
                                  const std::string& hintMime) {
    if (!hintMime.empty()) return hintMime;
    if (data.size() >= 3 && data[0] == 0xFF && data[1] == 0xD8 &&
        data[2] == 0xFF) {
        return "image/jpeg";
    }
    if (data.size() >= 8 && data[0] == 0x89 && data[1] == 'P' &&
        data[2] == 'N' && data[3] == 'G') {
        return "image/png";
    }
    return "image/jpeg";
}

bool PumpCoverBatchOperation(const CoverBatchOperationInput& input,
                             CoverBatchPersistencePort& persistence,
                             CoverBatchViewPort& view,
                             CoverBatchStatePort& state) {
    if (!input.active) return false;

    if (input.progress == 0 && input.total > 0) {
        const std::int64_t now = persistence.currentUnixTime();
        std::string key;
        if (input.paste) {
            const std::string mime =
                ResolveCoverImageMime(input.imageData, input.mimeHint);
            key = BuildEmbeddedCoverKey(input.imageData, mime);
            if (!persistence.hasCoverThumb(key) &&
                input.imageData.size() <= 512U * 1024U) {
                persistence.saveCoverThumb(key, input.imageData, mime);
            }
            persistence.assignEmbeddedCoverByPaths(input.targets, key, now);
        } else {
            persistence.clearCoverByPaths(input.targets);
        }

        view.applyVisibleRows(input.targets, input.paste, key);
        view.invalidateFileCoverCaches(input.targets);
        if (!input.paste) view.clearFolderCoverNegativeCache();
        view.requestCoverWallRefresh();
        view.syncToTab();
    }

    state.finishBatch(input.total);
    return true;
}

}  // namespace aura::cover_batch
