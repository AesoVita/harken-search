#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace HarkenSearch::HostSend {

// @stage7-contract
// API contract: host-send models are value/state descriptions and do not own
// bridge processes or target sessions. Callers preserve HostId/AdapterPath/
// DeliveryMode domains and handle validation/results before execution.

enum class HostId {
    Nuendo,
    ProTools,
};

enum class AdapterPath {
    SteinbergBridge,
    ProToolsImportAutomation,
    ProToolsPtslBridge,
};

enum class DeliveryMode {
    BringIntoHost,
    SpotToHost,
    ImportToClipsList,
};

enum class SourceSelectionKind {
    WholeFile,
    RegionRender,
};

enum class StagingPolicy {
    CopySource,
};

struct SourceItem {
    std::string sourcePathUtf8;
    std::string displayLabel;
    SourceSelectionKind selectionKind = SourceSelectionKind::WholeFile;
    // S8: per-file audio metadata for Pro Tools spot positioning (zero = unknown)
    std::uint32_t sourceSampleRate = 0;
    std::uint16_t channelCount = 0;
    std::uint64_t durationSamples = 0;
    std::uint64_t bwfTimeReferenceSamples = 0;
};

struct Request {
    HostId host = HostId::Nuendo;
    DeliveryMode mode = DeliveryMode::BringIntoHost;
    StagingPolicy stagingPolicy = StagingPolicy::CopySource;
    bool copyToProject = false;
    bool keepHarkenSearchProcessingSuffix = true;
    std::vector<SourceItem> items;
};

struct StagingFilePlan {
    std::string sourcePathUtf8;
    std::string stagedPathUtf8;
    std::string stagedFileName;
    std::string extensionLower;
};

struct StagingPlan {
    std::string rootDirectoryUtf8;
    std::vector<StagingFilePlan> files;
};

struct DiagnosticsContract {
    std::string category = "HostSend";
    std::string requestEvent = "HostSend.Requested";
    std::string stageReadyEvent = "HostSend.StageReady";
    std::string dispatchEvent = "HostSend.Dispatch";
    std::string resultEvent = "HostSend.Result";
    std::string failEvent = "HostSend.Fail";
};

struct Job {
    HostId host = HostId::Nuendo;
    AdapterPath adapterPath = AdapterPath::SteinbergBridge;
    DeliveryMode mode = DeliveryMode::BringIntoHost;
    bool copyToProject = false;
    StagingPolicy stagingPolicy = StagingPolicy::CopySource;
    std::vector<SourceItem> items;
    StagingPlan stagingPlan;
    DiagnosticsContract diagnostics;
    std::string payloadJson;
};

const char* HostKey(HostId host);
const char* AdapterPathKey(AdapterPath path);
const char* DeliveryModeKey(DeliveryMode mode);
const char* SourceSelectionKey(SourceSelectionKind kind);

AdapterPath DefaultAdapterPathForHost(HostId host);
DeliveryMode DefaultDeliveryModeForHost(HostId host);

std::string DefaultStagingRootUtf8();
std::string BuildPayloadJson(const Job& job);
StagingPlan BuildStagingPlan(const Request& request, const std::string& rootDirectoryUtf8);
Job BuildJob(const Request& request, const std::string& rootDirectoryUtf8 = {});

} // namespace HarkenSearch::HostSend
