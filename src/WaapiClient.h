#pragma once
// Aris - WAAPI Client (F-034)
// WaapiClient.h - Wwise Authoring API client over WebSocket + JSON-RPC 2.0
//
// 目标：与运行中的 Wwise Authoring 建立 WebSocket 连接，发送 WAAPI 调用并同步收结果。
// 协议：WAMP v2 JSON over WebSocket（默认 ws://127.0.0.1:8765/waapi）。
//
// S2 实现：基于 IXWebSocket 的真实 WAMP v2 / JSON-RPC 2.0 传输层已完成。

#include <string>
#include <functional>
#include <memory>
#include <chrono>

namespace aura {
namespace waapi {

// @stage7-contract
// API contract: WAAPI calls preserve JSON-RPC result/error domains and own all
// returned JSON strings. Connection and transport lifetime remain with the
// client; callers must serialize lifecycle changes and handle CallResult::ok
// before consuming resultJson.

// JSON-RPC 2.0 调用结果
struct CallResult {
    bool ok = false;              // 是否成功收到响应且无错误
    std::string resultJson;       // 成功时的 result 对象 JSON（可能为 "{}"）
    std::string errorMessage;     // 失败时的错误描述
    int errorCode = 0;            // JSON-RPC error.code 或传输层错误码
};

// 连接状态
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Failed,
};

// 连接配置
struct ClientConfig {
    std::string host = "127.0.0.1";
    int port = 8765;
    std::string path = "/waapi";         // Wwise WebSocket endpoint
    int connectTimeoutMs = 3000;
    int callTimeoutMs = 10000;
    int heartbeatIntervalMs = 30000;     // WebSocket ping/pong
    bool autoReconnect = true;
};

namespace detail {

// A plaintext WAAPI URL may never leave the local machine.  Keep this policy
// independent from the WebSocket implementation so it can be checked before
// network initialization, tracing, or any transport configuration.
struct PlaintextLoopbackEndpoint {
    bool allowed = false;
    std::string normalizedHost;
};

inline std::string asciiLower(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return value;
}

inline bool hasUnsafeEndpointCharacters(const std::string& value,
                                        bool rejectSlash) {
    for (const unsigned char byte : value) {
        if (byte < 0x21 || byte > 0x7e || byte == '%' || byte == '\\' ||
            byte == '?' || byte == '#' || byte == '@' ||
            (rejectSlash && byte == '/')) {
            return true;
        }
    }
    return false;
}

inline PlaintextLoopbackEndpoint validatePlaintextLoopbackEndpoint(
    const ClientConfig& cfg) {
    PlaintextLoopbackEndpoint result;
    if (cfg.port < 1 || cfg.port > 65535 || cfg.path != "/waapi" ||
        cfg.host.empty() || hasUnsafeEndpointCharacters(cfg.host, true) ||
        hasUnsafeEndpointCharacters(cfg.path, false)) {
        return result;
    }

    const std::string host = asciiLower(cfg.host);
    if (host == "127.0.0.1") {
        result.normalizedHost = host;
    } else if (host == "localhost" || host == "localhost.") {
        // Do not trust hosts/DNS resolution for a plaintext transport.  A
        // numeric loopback literal guarantees the connection cannot leave
        // this machine even if the system's localhost mapping is corrupted.
        result.normalizedHost = "127.0.0.1";
    } else if (host == "::1" || host == "[::1]") {
        // WebSocket URLs require brackets around an IPv6 literal.
        result.normalizedHost = "[::1]";
    } else {
        return result;
    }

    result.allowed = true;
    return result;
}

} // namespace detail

// 断线回调
using DisconnectCallback = std::function<void(const std::string& reason)>;

// WaapiClient - WAAPI JSON-RPC client

class WaapiClient {
public:
    WaapiClient();
    ~WaapiClient();

    WaapiClient(const WaapiClient&) = delete;
    WaapiClient& operator=(const WaapiClient&) = delete;

    // 同步连接到 Wwise，返回是否握手成功。
    // 失败原因通过 lastError() 获取。
    bool connect(const ClientConfig& cfg);

    // 主动断开。
    void disconnect();

    // 当前连接状态。
    ConnectionState state() const;

    // 是否连接正常（便捷方法）。
    bool isConnected() const { return state() == ConnectionState::Connected; }

    // 同步发送一次 WAAPI 调用。
    // @param method   WAAPI 方法名，例如 "ak.wwise.core.getInfo"
    // @param argsJson 参数 JSON 字符串，例如 R"({"options":{"return":["name","id"]}})"
    //                 为空时传 "{}" 即可。
    // @return         CallResult 结构，ok=true 时 resultJson 有效。
    CallResult call(const std::string& method, const std::string& argsJson);

    // 连通性测试：调用 ak.wwise.core.getInfo，校验 Wwise 版本。
    // 成功时把 Wwise 工程路径写回 projectPath（可能为空字符串如果工程未加载）。
    // S1 骨架：始终返回 false。
    bool ping(std::string& wwiseVersionOut, std::string& projectPathOut);

    // 设置断线回调（可选）。
    void setOnDisconnect(DisconnectCallback cb);

    // 最近一次操作的错误信息（连接失败、调用超时等）。
    std::string lastError() const;

    // 诊断跟踪日志路径。记录每次 send/recv/drop/timeout 的原始 payload；
    // 用户遇到 WAAPI 问题时可以把文件内容发给我们定位 Wwise 实际返回了什么。
    // 懒打开（首次 trace 时创建），未打开时返回空串。
    std::string traceLogPath() const;

    // 版本要求：WAAPI 2022.1+（Property Template 继承依赖 pasteProperties）。
    static constexpr const char* kMinWwiseVersion = "2022.1.0";

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace waapi
}
