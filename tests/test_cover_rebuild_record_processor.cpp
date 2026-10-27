#include "src/CoverRebuildRecordProcessor.h"

#include <iostream>
#include <string>

using namespace aura::cover_rebuild;

namespace {
int failures = 0;
void Check(bool value, const char* message) {
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct FakeLookup final : CoverRebuildFolderLookupPort {
    int calls = 0;
    FolderCoverResolution resolution;
    FolderCoverResolution resolve(const std::string&) override {
        ++calls;
        return resolution;
    }
};
}  // namespace

int main() {
    {
        FakeLookup lookup;
        const auto result = ProcessCoverRebuildRecord(
            {"embedded.wav", "folder", "embedded-key", false}, lookup);
        Check(lookup.calls == 0, "embedded cover skips folder lookup");
        Check(result.foundDelta == 1 && result.setHasCover,
              "embedded cover sets missing has-cover state");
        Check(!result.clearHasCover && !result.folderUpdate.has_value(),
              "embedded cover emits no folder update");
    }
    {
        FakeLookup lookup;
        lookup.resolution = {"folder-key", "cover.png", 42};
        const auto result = ProcessCoverRebuildRecord(
            {"folder.wav", "folder", "", false}, lookup);
        Check(lookup.calls == 1, "folder miss resolves exactly once");
        Check(result.foundDelta == 1 && result.setHasCover,
              "folder cover sets missing has-cover state");
        Check(result.folderUpdate.has_value() &&
                  result.folderUpdate->folderCoverKey == "folder-key" &&
                  result.folderUpdate->folderCoverSrcMtime == 42,
              "folder resolution emits cache update");
    }
    {
        FakeLookup lookup;
        const auto result = ProcessCoverRebuildRecord(
            {"gone.wav", "folder", "", true}, lookup);
        Check(result.foundDelta == 0 && result.clearHasCover,
              "missing cover clears stale has-cover state");
        Check(!result.folderUpdate.has_value(),
              "missing cover emits no empty folder update");
    }
    if (failures) return 1;
    std::cout << "PASS: cover rebuild record processor\n";
    return 0;
}
