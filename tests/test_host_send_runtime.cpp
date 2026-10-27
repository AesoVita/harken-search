#include "HostSendRuntime.h"
#include "platform/filesystem.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int failures = 0;

void Check(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAIL: " << message << "\n";
    ++failures;
}

struct Sandbox {
    fs::path root = fs::temp_directory_path() /
        ("aris_host_send_runtime_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    Sandbox() { fs::create_directories(root); }
    ~Sandbox() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
};

void Write(const fs::path& path, const std::string& value) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary | std::ios::trunc) << value;
}

std::string Read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

HarkenSearch::HostSend::Job MakeJob(
    const std::vector<std::pair<std::string, std::string>>& paths) {
    HarkenSearch::HostSend::Job job;
    for (const auto& [source, target] : paths) {
        job.items.push_back({source, "item"});
        HarkenSearch::HostSend::StagingFilePlan file;
        file.sourcePathUtf8 = source;
        file.stagedPathUtf8 = target;
        job.stagingPlan.files.push_back(std::move(file));
    }
    return job;
}

void TestValidation() {
    std::string error;
    HarkenSearch::HostSend::Job mismatch;
    mismatch.items.push_back({"source.wav", "source"});
    Check(!HarkenSearch::HostSend::MaterializeStagingPlan(mismatch, error) &&
              error == "HostSend staging plan does not match source item count",
          "count mismatch keeps exact error");

    auto emptySource = MakeJob({{"", "target.wav"}});
    Check(!HarkenSearch::HostSend::MaterializeStagingPlan(emptySource, error) &&
              error == "HostSend staging source path is empty",
          "empty source keeps exact error");
    auto emptyTarget = MakeJob({{"source.wav", ""}});
    Check(!HarkenSearch::HostSend::MaterializeStagingPlan(emptyTarget, error) &&
              error == "HostSend staging destination path is empty",
          "empty destination keeps exact error");
}

void TestMissingAndMidFailureNoRollback() {
    Sandbox box;
    const fs::path source = box.root / "first.wav";
    const fs::path firstTarget = box.root / "stage" / "first.wav";
    const fs::path missing = box.root / "missing.wav";
    Write(source, "first");
    auto job = MakeJob({
        {source.u8string(), firstTarget.u8string()},
        {missing.u8string(), (box.root / "stage" / "missing.wav").u8string()},
    });
    std::string error;
    Check(!HarkenSearch::HostSend::MaterializeStagingPlan(job, error),
          "mid-plan missing source fails fast");
    Check(error == "HostSend source file does not exist: " + missing.u8string(),
          "missing source keeps exact error");
    Check(Read(firstTarget) == "first", "mid-plan failure does not roll back earlier copy");
}

void TestNestedCopyOverwriteAndErrorRetention() {
    Sandbox box;
    const fs::path source = box.root / "source.wav";
    const fs::path target = box.root / "nested" / "deeper" / "target.wav";
    Write(source, "new");
    Write(target, "old");
    auto job = MakeJob({{source.u8string(), target.u8string()}});
    std::string error = "retain-on-success";
    Check(HarkenSearch::HostSend::MaterializeStagingPlan(job, error),
          "nested staging copy succeeds");
    Check(Read(target) == "new", "staging copy overwrites existing destination");
    Check(error == "retain-on-success", "success leaves outError unchanged");
}

void TestEquivalentPaths() {
    Sandbox box;
    const fs::path source = box.root / "same.wav";
    Write(source, "same");
    Check(Platform::FileSystem::Equivalent(source.u8string(), source.u8string()),
          "Equivalent recognizes the same path");
    Check(!Platform::FileSystem::Equivalent(
              source.u8string(), (box.root / "missing.wav").u8string()),
          "Equivalent returns false for missing paths");
    auto job = MakeJob({{source.u8string(), source.u8string()}});
    std::string error;
    Check(HarkenSearch::HostSend::MaterializeStagingPlan(job, error) && Read(source) == "same",
          "equivalent staging path skips copy");
}

void TestLabelsAndDispatchOrder() {
    const auto request = HarkenSearch::HostSend::BuildRequestFromFiles(
        HarkenSearch::HostSend::HostId::Nuendo,
        {"C:/audio/Kick One.wav", "C:/audio/.hidden"},
        HarkenSearch::HostSend::DeliveryMode::BringIntoHost);
    Check(request.items.size() == 2 && request.items[0].displayLabel == "Kick One" &&
              request.items[1].displayLabel == ".hidden",
          "request labels remain path-derived and ordered");

    auto job = MakeJob({{"source-a.wav", "stage-a.wav"}, {"source-b.wav", ""}});
    const auto paths = HarkenSearch::HostSend::CollectDispatchPaths(job);
    Check(paths == std::vector<std::string>({"stage-a.wav", "source-b.wav"}),
          "dispatch paths preserve order and source fallback");
}

} // namespace

int main() {
    TestValidation();
    TestMissingAndMidFailureNoRollback();
    TestNestedCopyOverwriteAndErrorRetention();
    TestEquivalentPaths();
    TestLabelsAndDispatchOrder();
    if (failures != 0) std::cerr << "Total failures: " << failures << "\n";
    return failures == 0 ? 0 : 1;
}
