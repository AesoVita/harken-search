#pragma once

#include <string>
#include <vector>

namespace HarkenSearch::UI {

enum class EditorSendRouteId {
    DawTargets,
    WwiseTransfer,
};

struct EditorSendSurfaceContext {
    bool hasDawTargets = false;
    bool exposeWwiseTransfer = false;
    bool wwiseEnabled = false;
};

struct EditorSendRouteDescriptor {
    EditorSendRouteId id = EditorSendRouteId::DawTargets;
    std::string key;
    bool visible = true;
    bool enabled = true;
    std::string disabledReason;
};

std::vector<EditorSendRouteDescriptor> BuildEditorSendRoutes(
    const EditorSendSurfaceContext& ctx);

const EditorSendRouteDescriptor* FindEditorSendRoute(
    const std::vector<EditorSendRouteDescriptor>& routes,
    EditorSendRouteId id);

} // namespace HarkenSearch::UI
