// Aris - WAAPI Client (F-034) - S2 implementation
// WaapiClient.cpp - real WebSocket + JSON-RPC 2.0 transport on top of IXWebSocket.

#include "WaapiClient.h"
#include "WaapiResponseParse.h"
#include "platform/filesystem.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessage.h>

// nlohmann/json single-header (thirdparty/nlohmann_json.hpp). We do not enable
// implicit conversions to avoid compile-time blow-up in the application TU. Keep all
// json handling confined to this cpp.
#include "nlohmann_json.hpp"

namespace aura {
namespace waapi {

using nlohmann::json;

namespace {
void ensureNetSystemInit() {
    static std::once_flag initFlag;
    std::call_once(initFlag, []() { ix::initNetSystem(); });
}
} // namespace

struct WaapiClient::Impl {
    ClientConfig cfg;
    ix::WebSocket ws;
    std::atomic<ConnectionState> state{ConnectionState::Disconnected};
    DisconnectCallback onDisconnect;
    mutable std::mutex generalMu;
    std::string lastError;

    // JSON-RPC 2.0 pending-call bookkeeping.
    std::atomic<std::int64_t> nextId{1};
    struct Pending {
        std::promise<CallResult> promise;
    };
    std::mutex callsMu;
    std::unordered_map<std::int64_t, std::shared_ptr<Pending>> pending;

    // Signals connect() that the Open/Error handshake has resolved.
    std::mutex connectMu;
    std::condition_variable connectCv;

    // Diagnostic trace — every outgoing request, every raw inbound frame,
    // every dropped/unmatched event goes here so a user hitting a WAAPI
    // timeout can share the log and we can see what Wwise actually said
    // (or didn't). Lives at `%TEMP%/aura_waapi_trace.log` on Windows, or
    // `/tmp/aura_waapi_trace.log` elsewhere. Opened lazily on first trace
    // call so we don't spam a file for non-WAAPI sessions.
    std::mutex traceMu;
    Platform::FileSystem::StreamWriter traceFile;
    std::string tracePath;

    void trace(const char* tag, const std::string& payload) {
        std::lock_guard<std::mutex> lk(traceMu);
        if (!traceFile.IsOpen()) {
            std::string base;
#if defined(_WIN32)
            if (const char* tmp = std::getenv("TEMP"); tmp && *tmp) base = tmp;
#endif
            if (base.empty()) base = Platform::FileSystem::TemporaryDirectory();
            if (!base.empty()) {
                tracePath = (std::filesystem::u8path(base) / "aura_waapi_trace.log").u8string();
            }
            if (!tracePath.empty() && traceFile.Open(tracePath, true, false)) {
                traceFile.Write("\n==== WaapiClient session start (" +
                                std::to_string(std::time(nullptr)) + ") ====\n");
                traceFile.Flush();
            }
        }
        if (!traceFile.IsOpen()) return;
        const auto t = std::time(nullptr);
        std::string line = std::to_string(t) + " [" + tag + "] ";
        // Truncate to keep log files manageable even if WAAPI spits MBs.
        if (payload.size() > 4096) {
            line += payload.substr(0, 4096) + " ...(truncated " +
                    std::to_string(payload.size() - 4096) + " more bytes)";
        } else {
            line += payload;
        }
        traceFile.Write(line + "\n");
        traceFile.Flush(); // flush on every write so crash loses nothing
    }

    void resolveCall(std::int64_t id, CallResult&& res) {
        std::shared_ptr<Pending> p;
        {
            std::lock_guard<std::mutex> lk(callsMu);
            auto it = pending.find(id);
            if (it == pending.end()) return;
            p = it->second;
            pending.erase(it);
        }
        try {
            p->promise.set_value(std::move(res));
        } catch (const std::future_error&) {
            // Promise already satisfied or abandoned — ignore.
        }
    }

    void failAllPending(const std::string& reason) {
        std::unordered_map<std::int64_t, std::shared_ptr<Pending>> drained;
        {
            std::lock_guard<std::mutex> lk(callsMu);
            drained.swap(pending);
        }
        for (auto& kv : drained) {
            CallResult r;
            r.ok = false;
            r.errorCode = -32099; // transport-level failure
            r.errorMessage = reason;
            try {
                kv.second->promise.set_value(std::move(r));
            } catch (const std::future_error&) {
                // already resolved
            }
        }
    }
};

WaapiClient::WaapiClient() : impl_(std::make_unique<Impl>()) {}
WaapiClient::~WaapiClient() { disconnect(); }

bool WaapiClient::connect(const ClientConfig& cfg) {
    const auto endpoint = detail::validatePlaintextLoopbackEndpoint(cfg);
    if (!endpoint.allowed) {
        std::lock_guard<std::mutex> lk(impl_->generalMu);
        impl_->lastError = "WAAPI endpoint policy rejected";
        return false;
    }

    ensureNetSystemInit();

    {
        std::lock_guard<std::mutex> lk(impl_->generalMu);
        impl_->cfg = cfg;
        impl_->lastError.clear();
    }

    const std::string url =
        "ws://" + endpoint.normalizedHost + ":" + std::to_string(cfg.port) + cfg.path;
    impl_->trace("CONNECT", url);
    impl_->ws.setUrl(url);
    // WAMP v2 JSON encoding — negotiate via Sec-WebSocket-Protocol header
    // so the server routes our frames through its WAMP parser instead of
    // whatever default pipeline it would use for unlabelled connections.
    impl_->ws.addSubProtocol("wamp.2.json");
    impl_->ws.disableAutomaticReconnection();
    if (cfg.heartbeatIntervalMs > 0) {
        impl_->ws.setPingInterval(cfg.heartbeatIntervalMs / 1000);
    }

    impl_->ws.setOnMessageCallback(
        [this](const ix::WebSocketMessagePtr& msg) {
            using ix::WebSocketMessageType;
            switch (msg->type) {
            case WebSocketMessageType::Open: {
                impl_->trace("OPEN", "handshake ok");
                impl_->state.store(ConnectionState::Connected);
                std::lock_guard<std::mutex> lk(impl_->connectMu);
                impl_->connectCv.notify_all();
                break;
            }
            case WebSocketMessageType::Error: {
                impl_->trace("ERROR", msg->errorInfo.reason);
                {
                    std::lock_guard<std::mutex> lk(impl_->generalMu);
                    impl_->lastError = msg->errorInfo.reason;
                }
                impl_->state.store(ConnectionState::Failed);
                impl_->failAllPending("transport error: " +
                                      msg->errorInfo.reason);
                std::lock_guard<std::mutex> lk(impl_->connectMu);
                impl_->connectCv.notify_all();
                break;
            }
            case WebSocketMessageType::Close: {
                impl_->state.store(ConnectionState::Disconnected);
                impl_->failAllPending("connection closed: " +
                                      msg->closeInfo.reason);
                DisconnectCallback cb;
                {
                    std::lock_guard<std::mutex> lk(impl_->generalMu);
                    cb = impl_->onDisconnect;
                }
                if (cb) cb(msg->closeInfo.reason);
                std::lock_guard<std::mutex> lk(impl_->connectMu);
                impl_->connectCv.notify_all();
                break;
            }
            case WebSocketMessageType::Message: {
                // Always trace raw inbound so a user hitting a timeout can
                // send us the log and we can see what Wwise actually said
                // (or whether Wwise sent anything at all).
                impl_->trace("RECV", msg->str);

                std::int64_t id = 0;
                CallResult r;
                if (!detail::parseWampResponse(msg->str, id, r)) {
                    // Not a WAMP RESULT/ERROR we can route. Could be a
                    // WELCOME we don't need to ack (Wwise doesn't gate
                    // CALLs on session state in practice), a plain-text
                    // protocol error from Wwise, or an unsolicited
                    // subscription event. Leave it in the trace for later
                    // forensics; don't fail pending calls.
                    impl_->trace("DROP", "non-RESULT/ERROR frame");
                    return;
                }
                impl_->resolveCall(id, std::move(r));
                break;
            }
            case WebSocketMessageType::Ping:
            case WebSocketMessageType::Pong:
            case WebSocketMessageType::Fragment:
            default:
                break;
            }
        });

    impl_->state.store(ConnectionState::Connecting);
    impl_->ws.start();

    std::unique_lock<std::mutex> lk(impl_->connectMu);
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(cfg.connectTimeoutMs);
    impl_->connectCv.wait_until(lk, deadline, [this] {
        const auto s = impl_->state.load();
        return s == ConnectionState::Connected || s == ConnectionState::Failed;
    });

    const auto finalState = impl_->state.load();
    if (finalState == ConnectionState::Connected) return true;

    if (finalState == ConnectionState::Connecting) {
        std::lock_guard<std::mutex> glk(impl_->generalMu);
        impl_->lastError =
            "connect timeout after " + std::to_string(cfg.connectTimeoutMs) +
            "ms (Wwise Authoring API running on " + url + "?)";
        impl_->state.store(ConnectionState::Failed);
    }
    impl_->ws.stop();
    return false;
}

void WaapiClient::disconnect() {
    impl_->ws.stop();
    impl_->state.store(ConnectionState::Disconnected);
    impl_->failAllPending("client disconnect");
}

ConnectionState WaapiClient::state() const { return impl_->state.load(); }

CallResult WaapiClient::call(const std::string& method,
                             const std::string& argsJson) {
    CallResult failEarly;
    if (impl_->state.load() != ConnectionState::Connected) {
        failEarly.ok = false;
        failEarly.errorCode = -32099;
        failEarly.errorMessage = "not connected";
        return failEarly;
    }

    const auto id = impl_->nextId.fetch_add(1);
    auto pending = std::make_shared<Impl::Pending>();
    {
        std::lock_guard<std::mutex> lk(impl_->callsMu);
        impl_->pending.emplace(id, pending);
    }

    json params;
    json callOptions = json::object();
    if (argsJson.empty()) {
        params = json::object();
    } else {
        try {
            params = json::parse(argsJson);
        } catch (const std::exception& e) {
            // Clean up pending and return error synchronously.
            std::lock_guard<std::mutex> lk(impl_->callsMu);
            impl_->pending.erase(id);
            failEarly.ok = false;
            failEarly.errorCode = -32602; // invalid params
            failEarly.errorMessage =
                std::string("argsJson parse error: ") + e.what();
            return failEarly;
        }
    }
    // WAAPI exposes a second "options" channel separate from the procedure
    // arguments (for example object.get return fields, language, platform).
    // Legacy call sites embed that object inside argsJson. Lift it into
    // the WAMP CALL options slot so existing callers keep working against
    // current Wwise schemas without rewriting every site.
    if (params.is_object()) {
        auto it = params.find("options");
        if (it != params.end() && it->is_object()) {
            callOptions = *it;
            params.erase(it);
        }
    }

    // Wwise's WAAPI speaks WAMP v2 over WebSocket. Evidence from the
    // user's trace log: the server rejected "Error: message is expected to
    // be an array" for bare JSON-RPC, then "expecting a positive number for
    // message type" for array-wrapped JSON-RPC. Encode a proper WAMP CALL
    // message: [48, id, {}, procedure, [], kwargs] — see
    // detail::encodeWampCall for details.
    const std::string serialized =
        detail::encodeWampCall(id, method, params.dump(), callOptions.dump());
    impl_->trace("SEND", serialized);
    const auto sendResult = impl_->ws.sendText(serialized);
    if (!sendResult.success) {
        std::lock_guard<std::mutex> lk(impl_->callsMu);
        impl_->pending.erase(id);
        failEarly.ok = false;
        failEarly.errorCode = -32099;
        failEarly.errorMessage = "sendText failed";
        return failEarly;
    }

    auto fut = pending->promise.get_future();
    const auto status =
        fut.wait_for(std::chrono::milliseconds(impl_->cfg.callTimeoutMs));
    if (status == std::future_status::timeout) {
        {
            std::lock_guard<std::mutex> lk(impl_->callsMu);
            impl_->pending.erase(id);
        }
        CallResult r;
        r.ok = false;
        r.errorCode = -32001;
        r.errorMessage = "call timeout after " +
                         std::to_string(impl_->cfg.callTimeoutMs) + "ms";
        impl_->trace("TIMEOUT", method + " (id=" + std::to_string(id) + ")");
        return r;
    }
    return fut.get();
}

bool WaapiClient::ping(std::string& wwiseVersionOut,
                       std::string& projectPathOut) {
    wwiseVersionOut.clear();
    projectPathOut.clear();

    // ak.wwise.core.getInfo returns { version: {displayName, ...},
    //                                 copyright, ... }
    // ak.wwise.core.getProjectInfo returns { name, path, ... } (Wwise 2022.1+).
    const auto infoRes = call("ak.wwise.core.getInfo", "{}");
    if (!infoRes.ok) {
        std::lock_guard<std::mutex> lk(impl_->generalMu);
        impl_->lastError = "getInfo failed: " + infoRes.errorMessage;
        return false;
    }
    try {
        const auto parsed = json::parse(infoRes.resultJson);
        if (parsed.contains("version") && parsed["version"].is_object()) {
            wwiseVersionOut =
                parsed["version"].value("displayName", std::string());
        }
    } catch (const std::exception&) {
        // Non-fatal; version is informational.
    }

    // Project info is optional — may fail if no project loaded yet.
    const auto projRes = call("ak.wwise.core.getProjectInfo", "{}");
    if (projRes.ok) {
        try {
            const auto parsed = json::parse(projRes.resultJson);
            projectPathOut = parsed.value("path", std::string());
        } catch (const std::exception&) {
            // leave projectPathOut empty
        }
    }
    return true;
}

void WaapiClient::setOnDisconnect(DisconnectCallback cb) {
    std::lock_guard<std::mutex> lk(impl_->generalMu);
    impl_->onDisconnect = std::move(cb);
}

std::string WaapiClient::traceLogPath() const {
    std::lock_guard<std::mutex> lk(impl_->traceMu);
    return impl_->tracePath;
}

std::string WaapiClient::lastError() const {
    std::lock_guard<std::mutex> lk(impl_->generalMu);
    return impl_->lastError;
}

} // namespace waapi
}
