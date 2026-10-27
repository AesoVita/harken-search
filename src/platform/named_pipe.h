#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace Platform::NamedPipe {

// @stage7-contract
// API contract: named-pipe handles are owned by the returned/session object and
// must be closed through the declared lifecycle. Buffer operations report their
// existing status values and never retain caller buffers after return.

enum class MessageExchangeStatus {
    Unsupported,
    Ok,
    ConnectFailed,
    MessageModeFailed,
    WriteFailed,
    ResponseWaitFailed,
    ReadFailed,
    TimedOut,
};

struct MessageExchangeResult {
    MessageExchangeStatus status = MessageExchangeStatus::Unsupported;
    unsigned long systemError = 0;
    std::string response;
    bool retryExhausted = false;
};

MessageExchangeResult ExchangeMessage(std::wstring_view pipeName,
                                      std::string_view request,
                                      uint32_t timeoutMilliseconds);

struct MessageExchangeOptions {
    uint32_t connectAttempts = 0;
    uint32_t busyWaitMs = 0;
    uint32_t notFoundSleepMs = 0;
    uint32_t responsePollMs = 5;
    uint32_t responseCapacity = 0;
    bool ignoreMessageModeFailure = false;
    bool continueAfterPeekOrReadFailure = false;
    bool trimTrailingCrLf = false;
};

MessageExchangeResult ExchangeMessageWithOptions(
    std::wstring_view pipeName, std::string_view request,
    uint32_t timeoutMilliseconds, const MessageExchangeOptions& options);

enum class ServerSessionStatus {
    Unsupported,
    Ok,
    InvalidArgument,
    CreateFailed,
    AcceptFailed,
    ReadFailed,
    EmptyMessage,
    WriteFailed,
    FlushFailed,
    DisconnectFailed,
    CloseFailed,
};

struct ServerOperationResult {
    ServerSessionStatus status = ServerSessionStatus::Unsupported;
    unsigned long systemError = 0;
    unsigned long secondarySystemError = 0;
};

struct ServerReadResult : ServerOperationResult {
    std::string message;
};

class ServerSession {
public:
    ServerSession();
    ~ServerSession();
    ServerSession(ServerSession&&) noexcept;
    ServerSession& operator=(ServerSession&&) noexcept;
    ServerSession(const ServerSession&) = delete;
    ServerSession& operator=(const ServerSession&) = delete;

    ServerOperationResult Open(std::wstring_view pipeName,
                               uint32_t inboundBytes,
                               uint32_t outboundBytes,
                               uint32_t maxInstances = 1);
    ServerOperationResult Accept();
    ServerReadResult Read(uint32_t maxMessageBytes);
    ServerOperationResult WriteAndFlush(std::string_view response);
    ServerOperationResult DisconnectAndClose();
    bool IsOpen() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Platform::NamedPipe
