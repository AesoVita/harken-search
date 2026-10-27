#ifdef __APPLE__

#include "named_pipe.h"

namespace Platform::NamedPipe {

MessageExchangeResult ExchangeMessage(std::wstring_view,
                                      std::string_view,
                                      uint32_t) {
    MessageExchangeResult result;
    result.status = MessageExchangeStatus::Unsupported;
    return result;
}

MessageExchangeResult ExchangeMessageWithOptions(
    std::wstring_view, std::string_view, uint32_t,
    const MessageExchangeOptions&) {
    MessageExchangeResult result;
    result.status = MessageExchangeStatus::Unsupported;
    return result;
}

struct ServerSession::Impl {};

ServerSession::ServerSession() : impl_(std::make_unique<Impl>()) {}
ServerSession::~ServerSession() = default;
ServerSession::ServerSession(ServerSession&&) noexcept = default;
ServerSession& ServerSession::operator=(ServerSession&&) noexcept = default;

ServerOperationResult ServerSession::Open(std::wstring_view,
                                          uint32_t,
                                          uint32_t,
                                          uint32_t) {
    return {ServerSessionStatus::Unsupported};
}

ServerOperationResult ServerSession::Accept() {
    return {ServerSessionStatus::Unsupported};
}

ServerReadResult ServerSession::Read(uint32_t) {
    ServerReadResult result;
    result.status = ServerSessionStatus::Unsupported;
    return result;
}

ServerOperationResult ServerSession::WriteAndFlush(std::string_view) {
    return {ServerSessionStatus::Unsupported};
}

ServerOperationResult ServerSession::DisconnectAndClose() {
    return {ServerSessionStatus::Unsupported};
}

bool ServerSession::IsOpen() const noexcept {
    return false;
}

} // namespace Platform::NamedPipe

#endif // __APPLE__
