#include "src/EditorSendSurfaceModel.h"

#include <iostream>

static int failed = 0;

static void check(bool cond, const char* msg) {
    if (cond) {
        std::cout << "PASS: " << msg << "\n";
    } else {
        std::cout << "FAIL: " << msg << "\n";
        ++failed;
    }
}

static void testDawOnlySurface() {
    HarkenSearch::UI::EditorSendSurfaceContext ctx;
    ctx.hasDawTargets = true;

    const auto routes = HarkenSearch::UI::BuildEditorSendRoutes(ctx);
    check(routes.size() == 1, "DAW-only context exposes exactly one route");
    check(routes[0].id == HarkenSearch::UI::EditorSendRouteId::DawTargets,
          "DAW route is first");
    check(routes[0].enabled, "configured DAW route is enabled");
}

static void testMissingDawStillTruthful() {
    HarkenSearch::UI::EditorSendSurfaceContext ctx;

    const auto routes = HarkenSearch::UI::BuildEditorSendRoutes(ctx);
    const auto* daw = HarkenSearch::UI::FindEditorSendRoute(
        routes, HarkenSearch::UI::EditorSendRouteId::DawTargets);
    check(daw != nullptr, "DAW route remains visible even without targets");
    check(daw && !daw->enabled, "missing DAW target disables route");
    check(daw && daw->disabledReason == "No DAW target configured",
          "missing DAW target carries honest reason");
}

static void testWwiseEditionGate() {
    HarkenSearch::UI::EditorSendSurfaceContext ctx;
    ctx.hasDawTargets = true;
    ctx.exposeWwiseTransfer = true;
    ctx.wwiseEnabled = false;

    const auto routes = HarkenSearch::UI::BuildEditorSendRoutes(ctx);
    check(routes.size() == 2, "Wwise feature exposure adds second route");
    check(routes[1].id == HarkenSearch::UI::EditorSendRouteId::WwiseTransfer,
          "Wwise route follows DAW route");
    check(!routes[1].enabled, "edition-gated Wwise route is disabled");
    check(routes[1].disabledReason == "Wwise transfer is not enabled",
          "edition-gated Wwise route keeps explicit reason");
}

static void testWwiseHiddenWhenFeatureOff() {
    HarkenSearch::UI::EditorSendSurfaceContext ctx;
    ctx.hasDawTargets = true;
    ctx.exposeWwiseTransfer = false;
    ctx.wwiseEnabled = true;

    const auto routes = HarkenSearch::UI::BuildEditorSendRoutes(ctx);
    check(routes.size() == 1, "feature-off Wwise route stays hidden");
    check(HarkenSearch::UI::FindEditorSendRoute(routes, HarkenSearch::UI::EditorSendRouteId::WwiseTransfer) == nullptr,
          "no Wwise route exists when feature is hidden");
}

int main() {
    testDawOnlySurface();
    testMissingDawStillTruthful();
    testWwiseEditionGate();
    testWwiseHiddenWhenFeatureOff();

    if (failed != 0) {
        std::cerr << "Total failures: " << failed << "\n";
        return 1;
    }
    std::cout << "All editor send surface model tests passed\n";
    return 0;
}
