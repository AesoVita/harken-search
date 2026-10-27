#pragma once
// Aris - WAAPI Client (F-034 S2) - WAMP v2 message parser + encoder.
//
// Wwise's WAAPI speaks WAMP v2 over WebSocket, not plain JSON-RPC. Every
// message is a JSON array whose first element is a positive integer
// message-type code:
//   1  HELLO   [1, realm, {roles:...}]
//   2  WELCOME [2, sessionId, {roles:...}]
//   8  ERROR   [8, requestType, requestId, details, errorUri, args?, kwargs?]
//   48 CALL    [48, requestId, options, procedure, args, kwargs]
//   50 RESULT  [50, requestId, details, args, kwargs]
//
// Evidence this is the actual on-wire protocol: user's
// %TEMP%/aura_waapi_trace.log shows Wwise rejecting bare JSON-RPC objects
// with "message is expected to be an array", and rejecting a 1-element
// JSON-RPC array with "expecting a positive number for message type" —
// exactly what you'd get from a WAMP server parsing malformed messages.
//
// This header exposes only the pieces unit-testable without IXWebSocket:
// WAMP CALL encoding + response (RESULT/ERROR) decoding.

#include "WaapiClient.h"          // CallResult

#include <cstdint>
#include <string>

namespace aura {
namespace waapi {
namespace detail {

// @stage7-contract
// API contract: WAMP encode/parse helpers are synchronous, transport-free
// boundary functions. Encoders borrow JSON/procedure strings and return owned
// wire text; parsers fill caller-owned id/result outputs and return false for
// malformed, unsupported, or mismatched messages without retaining input.

// WAMP message type codes used on our code path. Subset of the WAMP spec.
constexpr int kWampHello   = 1;
constexpr int kWampWelcome = 2;
constexpr int kWampAbort   = 3;
constexpr int kWampError   = 8;
constexpr int kWampCall    = 48;
constexpr int kWampResult  = 50;

// Encode a WAMP CALL message.
//   requestId   — monotonic id produced by the client; echoed back in RESULT/ERROR
//   procedure   — WAAPI method name (e.g. "ak.wwise.ui.getSelectedObjects")
//   kwargsJson  — JSON object encoded as string; becomes the CALL's kwargs dict.
//                 Pass "" or "{}" for calls that take no arguments.
// Returns the serialised JSON text ready to hand to ws.sendText().
std::string encodeWampCall(std::int64_t requestId,
                           const std::string& procedure,
                           const std::string& kwargsJson,
                           const std::string& callOptionsJson = "{}");

// Parse a WAMP response message (RESULT or ERROR). Returns true for
// well-formed RESULT/ERROR responses whose id matches our outgoing CALL
// numbering scheme; populates outId + outResult accordingly.
//
// Returns false for:
//   - malformed JSON
//   - non-array root (Wwise sometimes emits plain-text protocol errors)
//   - empty array or non-numeric first element (not WAMP)
//   - WAMP message types we don't dispatch yet (HELLO/WELCOME/ABORT/etc.)
//   - responses missing the required id field for their type
//
// On WAMP ERROR the call is resolved with ok=false and errorMessage
// populated from the error URI + first positional arg when present.
bool parseWampResponse(const std::string& text,
                       std::int64_t& outId,
                       CallResult& outResult);

} // namespace detail
} // namespace waapi
}
