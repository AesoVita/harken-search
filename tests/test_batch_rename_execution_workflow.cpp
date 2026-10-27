#include "BatchRenameExecutionWorkflow.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

bool Check(bool condition, const char* message) {
    if (condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

RenamePreview Preview(
    int64_t id,
    std::string filePath,
    std::string newName,
    bool willChange = true,
    std::string status = "READY") {
    RenamePreview preview;
    preview.oldName = fs::u8path(filePath).filename().u8string();
    preview.newName = std::move(newName);
    preview.filePath = std::move(filePath);
    preview.recordId = id;
    preview.willChange = willChange;
    preview.status = std::move(status);
    preview.conflict = "preserve-conflict";
    return preview;
}

} // namespace

int main() {
    const fs::path root = fs::u8path("batch-workflow-root");
    std::vector<RenamePreview> previews{
        Preview(1, (root / "skip-flag.wav").u8string(), "ignored.wav", false),
        Preview(2, (root / "skip-status.wav").u8string(), "ignored.wav", true, "OTHER"),
        Preview(10, (root / "first.wav").u8string(), "first-done.wav"),
        Preview(20, (root / "false.wav").u8string(), "false-done.wav"),
        Preview(30, (root / "standard.wav").u8string(), "standard-done.wav"),
        Preview(40, (root / "unknown.wav").u8string(), "unknown-done.wav"),
        Preview(50, (root / "last.wav").u8string(), "last-done.wav"),
    };
    const auto before = previews;
    std::vector<int64_t> callOrder;

    const aura::rename::BatchRenameExecutionLabels labels{
        "READY", "DONE", "FILE_ERROR", "UNKNOWN_ERROR"};
    const auto completed = aura::rename::ExecuteEligibleRenames(
        previews,
        labels,
        [&](int64_t id, const std::string& oldPath, const std::string& newName) {
            callOrder.push_back(id);
            if (id == 10) {
                return oldPath == before[2].filePath && newName == before[2].newName;
            }
            if (id == 20) return false;
            if (id == 30) throw std::runtime_error("expected standard failure");
            if (id == 40) throw 40;
            if (id == 50) return true;
            throw std::runtime_error("skipped preview was executed");
        });

    if (!Check(
            callOrder == std::vector<int64_t>({10, 20, 30, 40, 50}),
            "eligible callbacks should preserve preview order")) return 1;
    if (!Check(
            previews[0].status == "READY" && previews[1].status == "OTHER",
            "ineligible previews should be skipped without status changes")) return 1;
    if (!Check(
            previews[2].status == "DONE" && previews[6].status == "DONE",
            "successful callbacks should receive the done status")) return 1;
    if (!Check(
            previews[3].status == "FILE_ERROR" &&
                previews[4].status == "FILE_ERROR",
            "false and standard exceptions should receive file-error status")) return 1;
    if (!Check(
            previews[5].status == "UNKNOWN_ERROR",
            "non-standard exceptions should receive unknown-error status")) return 1;
    if (!Check(
            completed.size() == 2,
            "only successful callbacks should produce completed entries")) return 1;
    if (!Check(
            completed[0].oldPath == before[2].filePath &&
                completed[0].newPath ==
                    (fs::u8path(before[2].filePath).parent_path() /
                     fs::u8path(before[2].newName)).u8string() &&
                completed[1].oldPath == before[6].filePath &&
                completed[1].newPath ==
                    (fs::u8path(before[6].filePath).parent_path() /
                     fs::u8path(before[6].newName)).u8string(),
            "completed entries should preserve old paths and sibling targets")) return 1;

    for (size_t i = 0; i < previews.size(); ++i) {
        if (!Check(
                previews[i].oldName == before[i].oldName &&
                    previews[i].newName == before[i].newName &&
                    previews[i].filePath == before[i].filePath &&
                    previews[i].recordId == before[i].recordId &&
                    previews[i].willChange == before[i].willChange &&
                    previews[i].conflict == before[i].conflict,
                "execution should mutate status only")) return 1;
    }

    std::cout << "PASS: batch rename execution workflow contracts\n";
    return 0;
}
