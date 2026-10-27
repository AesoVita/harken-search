#pragma once

#include "ReaperBridgeProtocol.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace HarkenSearch::ReaperBridge {

// @stage7-contract
// API contract: bridge calls own returned response/detail strings and preserve
// pipe/transport error domains. Client lifecycle and process ownership remain
// with the bridge implementation; callers handle PipeExchangeResult::ok.

struct PipeExchangeResult {
    bool ok = false;
    unsigned long systemError = 0;
    std::string response;
    std::string detail;
};

struct BatchSendResult {
    bool ok = false;
    int sent = 0;
    int total = 0;
    std::string detail;
};

using SendProgress = std::function<void(int sent, int total)>;

PipeExchangeResult ExchangeMessage(
    const std::string& message,
    std::wstring_view pipeName = kDefaultPipeName,
    int timeoutMilliseconds = 3000);

HandshakeResult ProbeHandshake(
    std::wstring_view pipeName = kDefaultPipeName,
    int timeoutMilliseconds = 3000);

BatchSendResult SendFiles(
    const std::vector<std::string>& filePaths,
    SendProgress progress = {},
    std::wstring_view pipeName = kDefaultPipeName,
    int timeoutMilliseconds = 10000);

} // namespace HarkenSearch::ReaperBridge
