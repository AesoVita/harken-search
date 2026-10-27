#include "DatabaseSwitchBlockingOperation.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

class RecordingEnvironment final
    : public aura::database_switch::BlockingDatabaseSwitchEnvironment {
public:
    void JoinBatchFeatures() override { calls.push_back("join_batch"); }
    void StopSameFolderScan() override { calls.push_back("stop_same_folder"); }
    void StopSearch() override { calls.push_back("stop_search"); }
    void StopWaveform() override { calls.push_back("stop_waveform"); }
    void StopBackfill() override { calls.push_back("stop_backfill"); }
    void StopCovers() override { calls.push_back("stop_covers"); }
    void CloseDatabase() override { calls.push_back("close_database"); }

    bool OpenDatabase(const std::string& pathUtf8) override {
        calls.push_back("open_database");
        openedPath = pathUtf8;
        return openResult;
    }

    bool SetWaveformCacheMode(int mode) override {
        calls.push_back("set_policy:" + std::to_string(mode));
        return policyResult;
    }

    int EnqueueStartupReconciliation() override {
        calls.push_back("reconcile");
        return reconciliationCount;
    }

    std::vector<std::string> calls;
    std::string openedPath;
    bool openResult = true;
    bool policyResult = true;
    int reconciliationCount = 7;
};

const std::vector<std::string> kTeardownAndOpen = {
    "join_batch",
    "stop_same_folder",
    "stop_search",
    "stop_waveform",
    "stop_backfill",
    "stop_covers",
    "close_database",
    "open_database",
};

void ExpectCalls(const RecordingEnvironment& environment,
                 const std::vector<std::string>& expected,
                 const char* message) {
    Expect(environment.calls == expected, message);
}

} // namespace

int main() {
    const std::string oldPath = "old.aurodb";
    const std::string newPath = "new.aurodb";

    {
        RecordingEnvironment environment;
        const bool result =
            aura::database_switch::RunBlockingDatabaseSwitchOperation(
                environment,
                {oldPath, newPath, -1, false});
        Expect(result, "successful open without policy must succeed");
        ExpectCalls(environment, kTeardownAndOpen,
                    "successful open without policy must preserve exact order");
        Expect(environment.openedPath == newPath,
               "operation must forward the new database path");
    }

    {
        RecordingEnvironment environment;
        environment.openResult = false;
        const bool result =
            aura::database_switch::RunBlockingDatabaseSwitchOperation(
                environment,
                {oldPath, newPath, 3, true});
        Expect(!result, "failed open must publish failure");
        ExpectCalls(environment, kTeardownAndOpen,
                    "failed open must skip policy and reconciliation");
    }

    {
        RecordingEnvironment environment;
        std::vector<std::string> expected = kTeardownAndOpen;
        expected.push_back("set_policy:2");
        expected.push_back("reconcile");
        const bool result =
            aura::database_switch::RunBlockingDatabaseSwitchOperation(
                environment,
                {oldPath, newPath, 2, true});
        Expect(result, "successful policy and reconciliation must succeed");
        ExpectCalls(environment, expected,
                    "policy must precede reconciliation after successful open");
    }

    {
        RecordingEnvironment environment;
        environment.policyResult = false;
        std::vector<std::string> expected = kTeardownAndOpen;
        expected.push_back("set_policy:1");
        const bool result =
            aura::database_switch::RunBlockingDatabaseSwitchOperation(
                environment,
                {oldPath, newPath, 1, true});
        Expect(!result, "failed initial policy must publish failure");
        ExpectCalls(environment, expected,
                    "failed initial policy must skip reconciliation");
    }

    if (failures != 0) return 1;
    std::cout << "PASS: database switch blocking operation contracts\n";
    return 0;
}
