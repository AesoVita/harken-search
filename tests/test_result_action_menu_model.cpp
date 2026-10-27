#include "src/ResultActionMenu.h"
#include "src/BetaFeatureFlags.h"

#include <iostream>
#include <string>
#include <vector>

static int failed = 0;

static void check(bool cond, const char* msg) {
    if (cond) {
        std::cout << "PASS: " << msg << "\n";
    } else {
        std::cout << "FAIL: " << msg << "\n";
        ++failed;
    }
}

static void testDeliveryOrder() {
    HarkenSearch::UI::ResultActionContext ctx;
    ctx.isMulti = false;
    ctx.selectedCount = 1;
    ctx.channels = 6;
    ctx.hasDawTargets = true;
    ctx.wwiseEnabled = false;

    auto actions = HarkenSearch::UI::BuildResultDeliveryActions(ctx);
    check(HarkenSearch::BetaFeatureFlags::kExposeWwiseTransfer,
          "development slice exposes Wwise transfer feature surface");
    check(actions.size() == 4, "delivery group includes exposed Wwise descriptor");
    check(actions[0].id == HarkenSearch::UI::ResultActionId::ExportFiles,
          "export is first delivery action");
    check(actions[1].id == HarkenSearch::UI::ResultActionId::ExtractChannel,
          "extract channel follows export");
    check(actions[2].id == HarkenSearch::UI::ResultActionId::SendToDaw,
          "send to DAW follows extract channel");
    check(actions[3].id == HarkenSearch::UI::ResultActionId::SendToWwise,
          "send to Wwise follows DAW handoff");
    check(actions[1].visible && actions[1].enabled,
          "5.1 single selection enables extract channel");
    const auto* wwise = HarkenSearch::UI::FindResultAction(
        actions,
        HarkenSearch::UI::ResultActionId::SendToWwise);
    check(wwise != nullptr, "Send to Wwise descriptor exists while feature surface is exposed");
    check(wwise && !wwise->enabled,
          "Send to Wwise stays disabled when ProductEdition/context denies wwise_transfer");
    check(wwise && wwise->disabledReason == "Wwise transfer is not enabled",
          "disabled Send to Wwise carries honest reason");
}

static void testWwiseEnabledState() {
    HarkenSearch::UI::ResultActionContext ctx;
    ctx.isMulti = true;
    ctx.selectedCount = 3;
    ctx.hasDawTargets = true;
    ctx.wwiseEnabled = true;

    const auto actions = HarkenSearch::UI::BuildResultDeliveryActions(ctx);
    const auto* wwise = HarkenSearch::UI::FindResultAction(
        actions,
        HarkenSearch::UI::ResultActionId::SendToWwise);
    check(wwise != nullptr, "enabled Wwise context exposes Send to Wwise descriptor");
    check(wwise && wwise->enabled,
          "Send to Wwise is enabled when ProductEdition/context allows wwise_transfer");
    check(wwise && wwise->disabledReason.empty(),
          "enabled Send to Wwise has no disabled reason");
}

static void testExtractChannelStates() {
    for (uint32_t channels : {0u, 1u, 2u}) {
        HarkenSearch::UI::ResultActionContext ctx;
        ctx.channels = channels;
        auto actions = HarkenSearch::UI::BuildResultDeliveryActions(ctx);
        const auto* extract = HarkenSearch::UI::FindResultAction(
            actions, HarkenSearch::UI::ResultActionId::ExtractChannel);
        check(extract != nullptr, "extract descriptor exists for single selection");
        check(extract && extract->visible && !extract->enabled,
              "mono/stereo/unknown channel files keep extract disabled");
        check(extract && !extract->disabledReason.empty(),
              "disabled extract descriptor carries reason copy");
        check(HarkenSearch::UI::BuildChannelExportActions(channels).empty(),
              "mono/stereo/unknown channel files expose no channel items");
    }

    HarkenSearch::UI::ResultActionContext multi;
    multi.isMulti = true;
    multi.selectedCount = 3;
    multi.channels = 6;
    auto actions = HarkenSearch::UI::BuildResultDeliveryActions(multi);
    const auto* extract = HarkenSearch::UI::FindResultAction(
        actions, HarkenSearch::UI::ResultActionId::ExtractChannel);
    check(extract && !extract->visible,
          "multi-select hides extract channel descriptor");
}

static void testChannelItems() {
    auto items = HarkenSearch::UI::BuildChannelExportActions(6);
    check(items.size() == 6, "5.1 files expose six channel export items");
    check(items[0].channelIndex == 0 && items[0].label == "L -> Mono WAV",
          "first channel item labels left channel");
    check(items[2].channelIndex == 2 && items[2].label == "C -> Mono WAV",
          "center channel item uses C label");
    check(items[3].channelIndex == 3 && items[3].label == "LFE -> Mono WAV",
          "LFE channel item uses LFE label");
}

int main() {
    testDeliveryOrder();
    testWwiseEnabledState();
    testExtractChannelStates();
    testChannelItems();

    if (failed != 0) {
        std::cerr << "Total failures: " << failed << "\n";
        return 1;
    }
    std::cout << "All result action menu model tests passed\n";
    return 0;
}
