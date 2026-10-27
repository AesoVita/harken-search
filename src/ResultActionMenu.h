#pragma once

#include <cstdint>
#include <string>
#include <vector>

// @stage7-contract
// API contract: result-action builders synchronously inspect caller-owned
// context/scalars and return owned descriptor vectors. FindResultAction
// borrows the action vector and returns a non-owning pointer valid only while
// that vector is unchanged; no UI state or callbacks are retained.

namespace HarkenSearch::UI {

enum class ResultActionId {
    ExportFiles,
    ExtractChannel,
    SendToDaw,
    SendToWwise
};

struct ResultActionContext {
    bool isMulti = false;
    int selectedCount = 0;
    uint32_t channels = 0;
    bool hasDawTargets = false;
    bool wwiseEnabled = false;
};

struct ResultActionDescriptor {
    ResultActionId id = ResultActionId::ExportFiles;
    std::string key;
    std::string label;
    bool visible = true;
    bool enabled = true;
    std::string disabledReason;
};

struct ChannelExportActionDescriptor {
    int channelIndex = -1;
    std::string label;
    bool enabled = true;
};

std::vector<ResultActionDescriptor> BuildResultDeliveryActions(
    const ResultActionContext& ctx);

std::vector<ChannelExportActionDescriptor> BuildChannelExportActions(
    uint32_t totalChannels);

const ResultActionDescriptor* FindResultAction(
    const std::vector<ResultActionDescriptor>& actions,
    ResultActionId id);

} // namespace HarkenSearch::UI
