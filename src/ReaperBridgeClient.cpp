#include "ReaperBridgeClient.h"
#include "platform/named_pipe.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>

namespace HarkenSearch::ReaperBridge {

PipeExchangeResult ExchangeMessage(
    const std::string& message,
    std::wstring_view pipeName,
    int timeoutMilliseconds)
{
    PipeExchangeResult result;
    if (message.empty()) {
        result.detail = "empty pipe message";
        return result;
    }
    if (message.size() > static_cast<std::size_t>(std::numeric_limits<uint32_t>::max())) {
        result.detail = "pipe message is too large";
        return result;
    }
    timeoutMilliseconds = (std::max)(1, timeoutMilliseconds);
    const auto exchange = Platform::NamedPipe::ExchangeMessage(
        pipeName, message, static_cast<uint32_t>(timeoutMilliseconds));
    result.systemError = exchange.systemError;
    result.response = exchange.response;
    switch (exchange.status) {
    case Platform::NamedPipe::MessageExchangeStatus::Ok:
        result.ok = true;
        break;
    case Platform::NamedPipe::MessageExchangeStatus::ConnectFailed:
        result.detail = "cannot connect to REAPER extension pipe (error " +
                        std::to_string(result.systemError) + ")";
        break;
    case Platform::NamedPipe::MessageExchangeStatus::MessageModeFailed:
        result.detail = "cannot set REAPER pipe message mode";
        break;
    case Platform::NamedPipe::MessageExchangeStatus::WriteFailed:
        result.detail = "failed to write complete REAPER bridge payload";
        break;
    case Platform::NamedPipe::MessageExchangeStatus::ResponseWaitFailed:
        result.detail = "failed while waiting for REAPER bridge response";
        break;
    case Platform::NamedPipe::MessageExchangeStatus::ReadFailed:
        result.detail = "failed to read REAPER bridge response";
        break;
    case Platform::NamedPipe::MessageExchangeStatus::TimedOut:
        result.detail = "REAPER extension did not respond within " +
                        std::to_string(timeoutMilliseconds) + "ms";
        break;
    case Platform::NamedPipe::MessageExchangeStatus::Unsupported:
        result.detail = "REAPER extension pipe is unsupported on this platform";
        break;
    }
    return result;
}

HandshakeResult ProbeHandshake(std::wstring_view pipeName, int timeoutMilliseconds) {
    const PipeExchangeResult exchange =
        ExchangeMessage(BuildHelloRequest(), pipeName, timeoutMilliseconds);
    if (!exchange.ok) {
        HandshakeResult result;
        result.code = HandshakeCode::TransportUnavailable;
        result.detail = exchange.detail;
        return result;
    }
    return ParseHelloResponse(exchange.response);
}

BatchSendResult SendFiles(
    const std::vector<std::string>& filePaths,
    SendProgress progress,
    std::wstring_view pipeName,
    int timeoutMilliseconds)
{
    BatchSendResult result;
    result.total = static_cast<int>(filePaths.size());
    std::string request;
    if (!BuildInsertBatchRequest(filePaths, request, result.detail)) return result;
    const PipeExchangeResult exchange =
        ExchangeMessage(request, pipeName, timeoutMilliseconds);
    if (!exchange.ok) {
        result.detail = exchange.detail;
        return result;
    }
    if (!ParseInsertBatchSuccessResponse(exchange.response, result.total, result.detail)) {
        return result;
    }
    result.ok = true;
    result.sent = result.total;
    if (progress) progress(result.sent, result.total);
    result.detail = result.sent == 1
        ? "已发送到 REAPER 轨道"
        : "已按顺序发送 " + std::to_string(result.sent) + " 个文件到 REAPER 轨道";
    return result;
}

} // namespace HarkenSearch::ReaperBridge
