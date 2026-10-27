#include "EditorSendSurfaceModel.h"

#include <algorithm>

namespace HarkenSearch::UI {

std::vector<EditorSendRouteDescriptor> BuildEditorSendRoutes(
    const EditorSendSurfaceContext& ctx) {
    std::vector<EditorSendRouteDescriptor> routes;

    EditorSendRouteDescriptor dawRoute;
    dawRoute.id = EditorSendRouteId::DawTargets;
    dawRoute.key = "send_to_daw";
    dawRoute.enabled = ctx.hasDawTargets;
    if (!dawRoute.enabled) {
        dawRoute.disabledReason = "No DAW target configured";
    }
    routes.push_back(std::move(dawRoute));

    if (ctx.exposeWwiseTransfer) {
        EditorSendRouteDescriptor wwiseRoute;
        wwiseRoute.id = EditorSendRouteId::WwiseTransfer;
        wwiseRoute.key = "send_to_wwise";
        wwiseRoute.enabled = ctx.wwiseEnabled;
        if (!wwiseRoute.enabled) {
            wwiseRoute.disabledReason = "Wwise transfer is not enabled";
        }
        routes.push_back(std::move(wwiseRoute));
    }

    return routes;
}

const EditorSendRouteDescriptor* FindEditorSendRoute(
    const std::vector<EditorSendRouteDescriptor>& routes,
    EditorSendRouteId id) {
    auto it = std::find_if(
        routes.begin(),
        routes.end(),
        [id](const EditorSendRouteDescriptor& route) {
            return route.id == id;
        });
    return it == routes.end() ? nullptr : &(*it);
}

} // namespace HarkenSearch::UI
