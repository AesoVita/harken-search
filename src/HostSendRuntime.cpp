#include "HostSendRuntime.h"
#include "platform/filesystem.h"

#include <filesystem>

namespace HarkenSearch::HostSend {

namespace {

std::filesystem::path Utf8Path(const std::string& value) {
    return std::filesystem::u8path(value);
}

std::string ToUtf8(const std::filesystem::path& path) {
    return path.u8string();
}

std::string LabelFromPath(const std::string& sourcePathUtf8) {
    const std::filesystem::path sourcePath = Utf8Path(sourcePathUtf8);
    std::string label = sourcePath.stem().u8string();
    if (label.empty()) {
        label = sourcePath.filename().u8string();
    }
    return label.empty() ? "audio" : label;
}

} // namespace

Request BuildRequestFromFiles(HostId host,
                              const std::vector<std::string>& sourcePathsUtf8,
                              DeliveryMode mode,
                              SourceSelectionKind selectionKind,
                              bool copyToProject,
                              bool keepHarkenSearchProcessingSuffix) {
    Request request;
    request.host = host;
    request.mode = mode;
    request.copyToProject = copyToProject;
    request.keepHarkenSearchProcessingSuffix = keepHarkenSearchProcessingSuffix;
    request.items.reserve(sourcePathsUtf8.size());
    for (const auto& sourcePathUtf8 : sourcePathsUtf8) {
        SourceItem item;
        item.sourcePathUtf8 = sourcePathUtf8;
        item.displayLabel = LabelFromPath(sourcePathUtf8);
        item.selectionKind = selectionKind;
        request.items.push_back(std::move(item));
    }
    return request;
}

bool MaterializeStagingPlan(const Job& job, std::string& outError) {
    if (job.items.size() != job.stagingPlan.files.size()) {
        outError = "HostSend staging plan does not match source item count";
        return false;
    }

    for (const auto& file : job.stagingPlan.files) {
        if (file.sourcePathUtf8.empty()) {
            outError = "HostSend staging source path is empty";
            return false;
        }
        if (file.stagedPathUtf8.empty()) {
            outError = "HostSend staging destination path is empty";
            return false;
        }

        const std::filesystem::path stagedPath = Utf8Path(file.stagedPathUtf8);
        std::string probeError;
        const auto sourceStatus =
            Platform::FileSystem::ProbePath(file.sourcePathUtf8, &probeError);
        if (sourceStatus == Platform::FileSystem::PathStatus::Error) {
            outError = "HostSend source file cannot be probed: " +
                file.sourcePathUtf8 + " (" + probeError + ")";
            return false;
        }
        if (sourceStatus == Platform::FileSystem::PathStatus::Missing) {
            outError = "HostSend source file does not exist: " + file.sourcePathUtf8;
            return false;
        }

        const auto stagedDir = stagedPath.parent_path();
        if (!stagedDir.empty() && !Platform::FileSystem::EnsureDirectoryExists(
                ToUtf8(stagedDir))) {
            outError = "HostSend failed to create staging directory: " + ToUtf8(stagedDir);
            return false;
        }

        if (Platform::FileSystem::Equivalent(
                file.sourcePathUtf8, file.stagedPathUtf8)) {
            continue;
        }
        if (!Platform::FileSystem::CopyFile(
                file.sourcePathUtf8, file.stagedPathUtf8, true)) {
            outError = "HostSend failed to copy staged file: " + file.stagedPathUtf8;
            return false;
        }
    }

    return true;
}

std::vector<std::string> CollectDispatchPaths(const Job& job) {
    std::vector<std::string> paths;
    paths.reserve(job.stagingPlan.files.size());
    for (const auto& file : job.stagingPlan.files) {
        paths.push_back(file.stagedPathUtf8.empty()
            ? file.sourcePathUtf8
            : file.stagedPathUtf8);
    }
    return paths;
}

} // namespace HarkenSearch::HostSend
