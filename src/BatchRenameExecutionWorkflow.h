#pragma once

#include "BatchRenamePolicy.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace aura::rename {

struct BatchRenameExecutionLabels {
    std::string ready;
    std::string done;
    std::string fileError;
    std::string unknownError;
};

struct CompletedBatchRename {
    std::string oldPath;
    std::string newPath;
};

using BatchRenameOperation = std::function<bool(
    int64_t recordId,
    const std::string& oldPath,
    const std::string& newName)>;

// @stage7-contract
// The operation is borrowed synchronously and is never retained. Preview
// statuses are updated in input order; returned paths are owned by the caller.
std::vector<CompletedBatchRename> ExecuteEligibleRenames(
    std::vector<RenamePreview>& previews,
    const BatchRenameExecutionLabels& labels,
    const BatchRenameOperation& renameOperation);

} // namespace aura::rename
