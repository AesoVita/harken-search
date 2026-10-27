#include "CoverRebuildRecordProcessor.h"

namespace aura::cover_rebuild {

CoverRebuildRecordResult ProcessCoverRebuildRecord(
    const CoverRebuildRecord& record, CoverRebuildFolderLookupPort& lookup) {
    FolderCoverResolution folder;
    const bool hasEmbeddedCover = !record.embeddedCoverKey.empty();
    if (!hasEmbeddedCover) {
        folder = lookup.resolve(record.folderPath);
    }

    if (folder.budgetExhausted) {
        CoverRebuildRecordResult result;
        result.budgetExhausted = true;
        return result;
    }

    const bool hasAnyCover = hasEmbeddedCover || !folder.key.empty();
    CoverRebuildRecordResult result;
    result.foundDelta = hasAnyCover ? 1 : 0;
    result.setHasCover = hasAnyCover && !record.hasCover;
    result.clearHasCover = !hasAnyCover && record.hasCover;
    if (!folder.key.empty()) {
        result.folderUpdate = CoverKeyCacheUpdate{
            record.filePath, folder.key, folder.sourcePath, folder.sourceMtime};
    }
    return result;
}

}  // namespace aura::cover_rebuild
