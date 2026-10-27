#include "ResultActionMenu.h"

#include "BetaFeatureFlags.h"
#include "TransferEngine.h"

#include <algorithm>

namespace HarkenSearch::UI {

std::vector<ResultActionDescriptor> BuildResultDeliveryActions(
    const ResultActionContext& ctx) {
    const bool canExtractChannel =
        !ctx.isMulti && TransferEngine::canExtractSingleChannel((int)ctx.channels);

    std::vector<ResultActionDescriptor> actions = {
        {
            ResultActionId::ExportFiles,
            "export_files",
            ctx.isMulti ? "Export Selected" : "Export File",
            true,
            true,
            ""
        },
        {
            ResultActionId::ExtractChannel,
            "extract_channel",
            "Extract Channel...",
            !ctx.isMulti,
            canExtractChannel,
            canExtractChannel ? "" : "Surround files only (>2 ch)"
        },
        {
            ResultActionId::SendToDaw,
            "send_to_daw",
            "Send to DAW",
            true,
            ctx.hasDawTargets,
            ctx.hasDawTargets ? "" : "No DAW target configured"
        }
    };

    if (HarkenSearch::BetaFeatureFlags::kExposeWwiseTransfer) {
        actions.push_back({
            ResultActionId::SendToWwise,
            "send_to_wwise",
            "Send to Wwise",
            true,
            ctx.wwiseEnabled,
            ctx.wwiseEnabled ? "" : "Wwise transfer is not enabled"
        });
    }

    return actions;
}

std::vector<ChannelExportActionDescriptor> BuildChannelExportActions(
    uint32_t totalChannels) {
    if (!TransferEngine::canExtractSingleChannel((int)totalChannels)) {
        return {};
    }

    const int menuChannels = (std::min)((int)totalChannels, 32);
    std::vector<ChannelExportActionDescriptor> actions;
    actions.reserve((size_t)menuChannels);
    for (int ch = 0; ch < menuChannels; ++ch) {
        std::string chLabel = TransferEngine::getSingleChannelExportLabel(
            ch, (int)totalChannels);
        actions.push_back({ch, chLabel + " -> Mono WAV", true});
    }
    return actions;
}

const ResultActionDescriptor* FindResultAction(
    const std::vector<ResultActionDescriptor>& actions,
    ResultActionId id) {
    auto it = std::find_if(actions.begin(), actions.end(),
        [id](const ResultActionDescriptor& action) {
            return action.id == id;
        });
    return it == actions.end() ? nullptr : &(*it);
}

} // namespace HarkenSearch::UI
