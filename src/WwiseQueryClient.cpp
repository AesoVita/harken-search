// Aris - Send to Wwise (F-034 S5-2) - WAAPI orchestrator for Preview Diff.
//
// Split from WwiseQuery.cpp so the pure parser + filesystem scanner can be
// unit-tested without dragging ixwebsocket's Debug-ABI-forced static lib
// into the test target (which triggers LNK2038 RuntimeLibrary /
// _ITERATOR_DEBUG_LEVEL mismatches). The test binary links WwiseQuery.cpp
// only; the main executable links both this file and WwiseQuery.cpp.

#include "WwiseQuery.h"

#include "nlohmann_json.hpp"

namespace aura {
namespace wwise {

bool fetchExistingObjects(waapi::WaapiClient& client,
                          const std::string& importDestination,
                          ExistingObjectMap& outMap,
                          std::string* errorOut) {
    outMap.clear();
    auto fail = [&](const std::string& msg) {
        if (errorOut) *errorOut = msg;
        return false;
    };

    if (!client.isConnected()) {
        return fail("WaapiClient not connected — cannot fetch existing objects");
    }

    // `from.path` anchors the query; transform.select:"descendants" pulls
    // every reachable object under it; options.return narrows the payload
    // to just (type, path) since those are the only fields the parser
    // consumes.
    nlohmann::json args;
    args["from"]["path"] = nlohmann::json::array({importDestination});
    args["transform"] = nlohmann::json::array({
        nlohmann::json{{"select", nlohmann::json::array({"descendants"})}}
    });
    args["options"]["return"] = nlohmann::json::array({"type", "path"});

    waapi::CallResult r = client.call("ak.wwise.core.object.get", args.dump());
    if (!r.ok) {
        return fail("WAAPI call failed: " + r.errorMessage);
    }

    std::string parseErr;
    if (!parseExistingObjectsResponse(r.resultJson, outMap, &parseErr)) {
        return fail("WAAPI response parse failed: " + parseErr);
    }
    return true;
}

} // namespace wwise
}
