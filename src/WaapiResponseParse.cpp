// Aris - WAAPI Client (F-034 S2) - WAMP v2 message parser + encoder.

#include "WaapiResponseParse.h"

#include "nlohmann_json.hpp"

namespace aura {
namespace waapi {
namespace detail {

std::string encodeWampCall(std::int64_t requestId,
                           const std::string& procedure,
                           const std::string& kwargsJson,
                           const std::string& callOptionsJson) {
    // kwargs defaults to an empty object — WAMP servers accept absent
    // kwargs, but we emit `{}` explicitly so the message shape stays
    // canonical across all our procedures.
    nlohmann::json kwargs = nlohmann::json::object();
    if (!kwargsJson.empty() && kwargsJson != "null") {
        try {
            kwargs = nlohmann::json::parse(kwargsJson);
            if (!kwargs.is_object()) kwargs = nlohmann::json::object();
        } catch (const std::exception&) {
            kwargs = nlohmann::json::object();
        }
    }
    nlohmann::json callOptions = nlohmann::json::object();
    if (!callOptionsJson.empty() && callOptionsJson != "null") {
        try {
            callOptions = nlohmann::json::parse(callOptionsJson);
            if (!callOptions.is_object()) callOptions = nlohmann::json::object();
        } catch (const std::exception&) {
            callOptions = nlohmann::json::object();
        }
    }

    // [48, requestId, options, procedure, args, kwargs]
    //   48       = CALL message type
    //   options  = call-time options dict (empty — we don't use progressive
    //              results or receive_progress)
    //   args     = positional arguments list (empty — WAAPI methods are
    //              keyword-only as far as we use them)
    //   kwargs   = keyword arguments dict (the user's JSON payload)
    const nlohmann::json msg = nlohmann::json::array({
        kWampCall,
        requestId,
        std::move(callOptions),        // options
        procedure,
        nlohmann::json::array(),       // args (positional)
        std::move(kwargs),             // kwargs
    });
    return msg.dump();
}

bool parseWampResponse(const std::string& text,
                       std::int64_t& outId,
                       CallResult& outResult) {
    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(text);
    } catch (const std::exception&) {
        return false; // malformed or plain-text protocol error from Wwise
    }
    if (!parsed.is_array() || parsed.empty()) return false;
    if (!parsed[0].is_number()) return false;

    const int type = parsed[0].get<int>();

    // [50, requestId, details, args, kwargs]
    if (type == kWampResult) {
        if (parsed.size() < 3 || !parsed[1].is_number()) return false;
        outId = parsed[1].get<std::int64_t>();
        CallResult r;
        r.ok = true;
        // Prefer kwargs (the shape WAAPI actually uses for object-returning
        // procedures). Fall back to the first positional arg when kwargs is
        // absent or empty — some WAAPI procedures emit data positionally.
        // Default to "{}" so downstream parsers never see an empty string.
        if (parsed.size() >= 5 && parsed[4].is_object() && !parsed[4].empty()) {
            r.resultJson = parsed[4].dump();
        } else if (parsed.size() >= 4 && parsed[3].is_array() && !parsed[3].empty()) {
            r.resultJson = parsed[3][0].dump();
        } else {
            r.resultJson = "{}";
        }
        outResult = std::move(r);
        return true;
    }

    // [8, requestType, requestId, details, errorUri, args?, kwargs?]
    if (type == kWampError) {
        if (parsed.size() < 5) return false;
        if (!parsed[2].is_number()) return false;
        outId = parsed[2].get<std::int64_t>();
        CallResult r;
        r.ok = false;
        r.errorCode = -32001; // generic — WAMP errors don't carry numeric codes
        if (parsed[4].is_string()) {
            r.errorMessage = parsed[4].get<std::string>(); // errorUri
        } else {
            r.errorMessage = "wamp.error.unknown";
        }
        // Append the first positional arg if it's a string — WAAPI uses
        // that slot for the human-readable message complementing the URI.
        if (parsed.size() >= 6 && parsed[5].is_array() && !parsed[5].empty()
            && parsed[5][0].is_string()) {
            r.errorMessage += ": " + parsed[5][0].get<std::string>();
        }
        outResult = std::move(r);
        return true;
    }

    // HELLO / WELCOME / ABORT / SUBSCRIBE events and any other WAMP control
    // frame — not a response to one of our CALLs; let the message handler
    // deal with them separately (WELCOME triggers session-ready).
    return false;
}

} // namespace detail
} // namespace waapi
}
