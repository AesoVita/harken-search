#include "BatchRenameExecutionWorkflow.h"

#include "Utils.h"

#include <exception>
#include <utility>

namespace aura::rename {

std::vector<CompletedBatchRename> ExecuteEligibleRenames(
    std::vector<RenamePreview>& previews,
    const BatchRenameExecutionLabels& labels,
    const BatchRenameOperation& renameOperation) {
    std::vector<CompletedBatchRename> completed;

    for (auto& preview : previews) {
        if (!preview.willChange || preview.status != labels.ready) continue;

        try {
            if (renameOperation(
                    preview.recordId,
                    preview.filePath,
                    preview.newName)) {
                preview.status = labels.done;
                const fs::path newPath =
                    Utf8ToPath(preview.filePath).parent_path() /
                    Utf8ToPath(preview.newName);
                completed.push_back(
                    {preview.filePath, PathToUtf8(newPath)});
            } else {
                preview.status = labels.fileError;
            }
        } catch (const std::exception&) {
            preview.status = labels.fileError;
        } catch (...) {
            preview.status = labels.unknownError;
        }
    }

    return completed;
}

} // namespace aura::rename
