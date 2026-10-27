#include "CueviFeedbackClient.h"

#include "nlohmann_json.hpp"
#include "platform/system.h"
#include "platform/text_encoding.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

namespace CueviFeedback {

namespace {

const char* categoryName(Category category) {
    return category == Category::Bug ? "bug" : "requirement";
}

const char* sourceName(Source source) {
    return source == Source::CrashRecovery ? "crash_recovery" : "airs_feedback";
}

std::string errorMessage(const nlohmann::json& body) {
    if (!body.is_object() || !body.contains("error") || !body["error"].is_object()) return {};
    const auto& error = body["error"];
    const auto message = error.find("message");
    return message != error.end() && message->is_string() ? message->get<std::string>() : std::string{};
}

bool isValidWorkItemId(const std::string& value) {
    size_t prefixLength = 0;
    if (value.rfind("BUG-", 0) == 0) prefixLength = 4;
    else if (value.rfind("RQ-", 0) == 0) prefixLength = 3;
    else return false;
    if (value.size() < prefixLength + 8 + 1 + 2) return false;
    const size_t dateEnd = prefixLength + 8;
    if (!std::all_of(value.begin() + static_cast<std::ptrdiff_t>(prefixLength),
                     value.begin() + static_cast<std::ptrdiff_t>(dateEnd),
                     [](unsigned char c) { return c >= '0' && c <= '9'; })) return false;
    if (value[dateEnd] != '-') return false;
    return std::all_of(value.begin() + static_cast<std::ptrdiff_t>(dateEnd + 1), value.end(),
                       [](unsigned char c) { return c >= '0' && c <= '9'; });
}

bool hasOptionalType(const nlohmann::json& body, const char* name, nlohmann::json::value_t type) {
    const auto value = body.find(name);
    return value == body.end() || value->type() == type;
}

bool isTimestamp(const nlohmann::json& value) {
    if (!value.is_string()) return false;
    const auto text = value.get<std::string>();
    return text.size() >= 20 && text.back() == 'Z' && text.find('T') != std::string::npos;
}

bool isAcceptedStage(const std::string& value) {
    return value == "backlog" || value == "in_progress" || value == "awaiting_qa" || value == "qa_failed" || value == "blocked";
}

uint32_t remainingTimeoutMs(const std::chrono::steady_clock::time_point& deadline) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) return 0;
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
    return static_cast<uint32_t>(std::max<int64_t>(1, remaining));
}

} // namespace

std::string BuildSubmissionJson(const Submission& input) {
    nlohmann::json body = {
        {"projectId", input.projectId},
        {"boardId", input.boardId},
        {"milestoneId", input.milestoneId},
        {"category", categoryName(input.category)},
        {"source", sourceName(input.source)},
        {"title", input.title},
        {"submittedAt", input.submittedAt},
        {"productVersion", input.productVersion},
        {"buildId", input.buildId},
        {"releaseChannel", input.releaseChannel},
        {"environment", input.environment},
        {"anonymous", input.anonymous},
        {"submissionId", input.submissionId},
        {"idempotencyKey", input.idempotencyKey}
    };
    if (!input.description.empty()) body["description"] = input.description;
    if (!input.anonymous && !input.reporter.empty()) body["reporter"] = input.reporter;
    if (!input.diagnosticLog.empty()) body["diagnosticLog"] = input.diagnosticLog;
    return body.dump();
}

Error ValidateSubmission(const Submission& input) {
    if (input.title.empty() || input.title.size() > 256) return Error::Validation;
    if (input.description.size() > 8192) return Error::Validation;
    if (input.diagnosticLog.size() > 65536) return Error::Validation;
    try {
        if (BuildSubmissionJson(input).size() > 128 * 1024) return Error::Validation;
    } catch (const std::exception&) {
        return Error::Validation;
    }
    return Error::None;
}

Result ParseResponse(uint32_t statusCode, const std::string& body, Category expectedCategory) {
    Result result;
    nlohmann::json parsed = nlohmann::json::parse(body, nullptr, false);
    if (statusCode == 401 || statusCode == 403) result.error = Error::Unauthorized;
    else if (statusCode == 408) result.error = Error::Timeout;
    else if (statusCode == 413 || statusCode == 422) result.error = Error::Validation;
    else if (statusCode == 429) result.error = Error::RateLimited;
    else if (statusCode >= 500) result.error = Error::Server;
    else if (statusCode < 200 || statusCode >= 300) result.error = Error::Network;
    if (result.error != Error::None) {
        if (!parsed.is_discarded()) result.detail = errorMessage(parsed);
        return result;
    }
    if (parsed.is_discarded() || !parsed.is_object()) {
        result.error = Error::MalformedResponse;
        return result;
    }
    const auto workItemId = parsed.find("workItemId");
    const auto category = parsed.find("category");
    const auto stage = parsed.find("stage");
    const auto replayed = parsed.find("replayed");
    const auto receivedAt = parsed.find("receivedAt");
    const bool idMatchesCategory = workItemId != parsed.end() && workItemId->is_string() &&
        ((expectedCategory == Category::Bug && workItemId->get<std::string>().rfind("BUG-", 0) == 0) ||
         (expectedCategory == Category::Requirement && workItemId->get<std::string>().rfind("RQ-", 0) == 0));
    if (workItemId == parsed.end() || !workItemId->is_string() || !isValidWorkItemId(workItemId->get<std::string>()) || !idMatchesCategory ||
        category == parsed.end() || !category->is_string() || category->get<std::string>() != categoryName(expectedCategory) ||
        stage == parsed.end() || !stage->is_string() || !isAcceptedStage(stage->get<std::string>()) ||
        replayed == parsed.end() || !replayed->is_boolean() ||
        receivedAt == parsed.end() || !isTimestamp(*receivedAt) ||
        !hasOptionalType(parsed, "webUrl", nlohmann::json::value_t::string)) {
        result.error = Error::MalformedResponse;
        return result;
    }
    result.ok = true;
    result.error = Error::None;
    result.workItemId = workItemId->get<std::string>();
    result.replayed = replayed->get<bool>();
    result.receivedAt = receivedAt->get<std::string>();
    result.webUrl = parsed.value("webUrl", std::string{});
    return result;
}

bool FitDiagnosticLogToSubmissionLimits(Submission& input) {
    constexpr size_t kDiagnosticLimit = 60 * 1024;
    if (input.diagnosticLog.size() <= kDiagnosticLimit) return true;
    size_t start = input.diagnosticLog.size() - kDiagnosticLimit;
    while (start < input.diagnosticLog.size() &&
           (static_cast<unsigned char>(input.diagnosticLog[start]) & 0xC0u) == 0x80u) {
        ++start;
    }
    input.diagnosticLog.erase(0, start);
    return input.diagnosticLog.size() <= kDiagnosticLimit;
}

std::optional<Endpoint> ParseEndpoint(const std::string& url) {
    const bool secure = url.rfind("https://", 0) == 0;
    const bool plainHttp = url.rfind("http://", 0) == 0;
    if (!secure && !plainHttp) return std::nullopt;
    if (url.find('#') != std::string::npos) return std::nullopt;
    const size_t authorityStart = secure ? 8 : 7;
    const size_t pathStart = url.find('/', authorityStart);
    if (pathStart == std::string::npos) return std::nullopt;
    const std::string authority = url.substr(authorityStart, pathStart - authorityStart);
    if (authority.empty() || authority.find('@') != std::string::npos) return std::nullopt;
    const size_t colon = authority.rfind(':');
    if (colon != std::string::npos && authority.find(':') != colon) return std::nullopt;
    const std::string host = colon == std::string::npos ? authority : authority.substr(0, colon);
    if (host.empty()) return std::nullopt;
    if (plainHttp && host != "localhost" && host != "127.0.0.1") return std::nullopt;
    uint32_t port = secure ? 443u : 80u;
    if (colon != std::string::npos) {
        const std::string portText = authority.substr(colon + 1);
        if (portText.empty() || !std::all_of(portText.begin(), portText.end(), [](unsigned char c) { return c >= '0' && c <= '9'; })) return std::nullopt;
        try {
            const unsigned long parsedPort = std::stoul(portText);
            if (parsedPort == 0 || parsedPort > 65535) return std::nullopt;
            port = static_cast<uint32_t>(parsedPort);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }
    const std::wstring wideHost = Platform::TextEncoding::Utf8ToWideStrict(host);
    const std::wstring widePath = Platform::TextEncoding::Utf8ToWideStrict(url.substr(pathStart));
    if (wideHost.empty() || widePath.empty()) return std::nullopt;
    return Endpoint{secure, wideHost, widePath, static_cast<uint16_t>(port)};
}

std::string GenerateSubmissionId() {
    std::array<unsigned char, 16> bytes{};
    std::random_device random;
    for (auto& byte : bytes) byte = static_cast<unsigned char>(random());
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);
    const char* hex = "0123456789abcdef";
    std::string value;
    value.reserve(36);
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) value.push_back('-');
        value.push_back(hex[(bytes[i] >> 4) & 0x0F]);
        value.push_back(hex[bytes[i] & 0x0F]);
    }
    return value;
}

bool IsUuid(const std::string& value) {
    if (value.size() != 36) return false;
    for (size_t i = 0; i < value.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (value[i] != '-') return false;
            continue;
        }
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return value[14] == '4' && (value[19] == '8' || value[19] == '9' || value[19] == 'a' || value[19] == 'A' || value[19] == 'b' || value[19] == 'B');
}

bool IsUiEntryEnabled(const Config& config) {
    if (config.endpointUrl.empty() || config.restrictedKey.empty()) return false;
    if (config.projectId.empty() || config.boardId.empty() || config.milestoneId.empty()) return false;
    return static_cast<bool>(ParseEndpoint(config.endpointUrl));
}

Result Submit(const Config& config, const Submission& input) {
    if (config.endpointUrl.empty() || config.restrictedKey.empty()) return Result{false, false, {}, {}, {}, "feedback service is not configured", Error::NotConfigured};
    const auto endpoint = ParseEndpoint(config.endpointUrl);
    if (!endpoint) return Result{false, false, {}, {}, {}, "feedback endpoint is invalid", Error::InvalidEndpoint};
    Submission bounded = input;
    FitDiagnosticLogToSubmissionLimits(bounded);
    if (ValidateSubmission(bounded) != Error::None) return Result{false, false, {}, {}, {}, "feedback request is invalid or too large", Error::Validation};
    const std::string body = BuildSubmissionJson(bounded);
#ifdef _WIN32
    const uint32_t timeout = config.timeoutMs == 0 ? 15000 : config.timeoutMs;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    void* session = Platform::OpenPreconfiguredInternetSession(L"HarkenSearchFeedback/2.0");
    if (!session) return Result{false, false, {}, {}, {}, "InternetOpen failed", Error::Network};
    const uint32_t remaining = remainingTimeoutMs(deadline);
    if (remaining == 0) {
        Platform::CloseInternetHandle(session);
        return Result{false, false, {}, {}, {}, "feedback request timed out", Error::Timeout};
    }
    if (!Platform::SetInternetTimeouts(session, remaining)) {
        Platform::CloseInternetHandle(session);
        return Result{false, false, {}, {}, {}, "feedback request timed out", Error::Timeout};
    }
    void* connection = Platform::ConnectInternetHttp(session, endpoint->host, endpoint->port);
    if (!connection) {
        Platform::CloseInternetHandle(session);
        return Result{false, false, {}, {}, {}, "InternetConnect failed", Error::Network};
    }
    void* request = Platform::OpenJsonPostRequest(connection, endpoint->path, endpoint->secure);
    if (!request) {
        Platform::CloseInternetHandle(connection);
        Platform::CloseInternetHandle(session);
        return Result{false, false, {}, {}, {}, "HttpOpenRequest failed", Error::Network};
    }
    {
        const uint32_t remaining = remainingTimeoutMs(deadline);
        if (remaining == 0) {
            Platform::CloseInternetHandle(request);
            Platform::CloseInternetHandle(connection);
            Platform::CloseInternetHandle(session);
            return Result{false, false, {}, {}, {}, "feedback request timed out", Error::Timeout};
        }
        if (!Platform::SetInternetTimeouts(request, remaining)) {
            Platform::CloseInternetHandle(request);
            Platform::CloseInternetHandle(connection);
            Platform::CloseInternetHandle(session);
            return Result{false, false, {}, {}, {}, "feedback request timed out", Error::Timeout};
        }
    }
    const std::wstring key = Platform::TextEncoding::Utf8ToWideStrict(config.restrictedKey);
    const std::wstring headers = L"Content-Type: application/json; charset=utf-8\r\n"
        L"X-Cuevi-Feedback-Key: " + key + L"\r\n"
        L"Content-Length: " + std::to_wstring(body.size()) + L"\r\n";
    const bool sent = Platform::SendHttpRequest(request, headers, body.data(), static_cast<uint32_t>(body.size()));
    uint32_t transportError = sent ? 0 : Platform::LastSystemError();
    std::string responseBody;
    std::array<char, 4096> buffer{};
    uint32_t bytesRead = 0;
    bool readSucceeded = sent;
    while (sent) {
        const uint32_t remaining = remainingTimeoutMs(deadline);
        if (remaining == 0) {
            readSucceeded = false;
            transportError = 12002;
            break;
        }
        if (!Platform::SetInternetTimeouts(request, remaining)) {
            readSucceeded = false;
            transportError = 12002;
            break;
        }
        if (!Platform::ReadInternetFile(request, buffer.data(), static_cast<uint32_t>(buffer.size()), bytesRead)) {
            readSucceeded = false;
            transportError = Platform::LastSystemError();
            break;
        }
        if (bytesRead == 0) break;
        if (responseBody.size() + bytesRead > 128 * 1024) {
            Platform::CloseInternetHandle(request);
            Platform::CloseInternetHandle(connection);
            Platform::CloseInternetHandle(session);
            return Result{false, false, {}, {}, {}, "feedback response is too large", Error::MalformedResponse};
        }
        responseBody.append(buffer.data(), bytesRead);
    }
    uint32_t statusCode = 0;
    const bool hasStatus = sent && Platform::QueryHttpStatusCode(request, statusCode);
    Platform::CloseInternetHandle(request);
    Platform::CloseInternetHandle(connection);
    Platform::CloseInternetHandle(session);
    if (!sent || !readSucceeded || !hasStatus) {
        constexpr uint32_t kInternetTimeoutError = 12002;
        const Error error = transportError == kInternetTimeoutError ? Error::Timeout : Error::Network;
        return Result{false, false, {}, {}, {}, "feedback transport failed", error};
    }
    return ParseResponse(statusCode, responseBody, bounded.category);
#else
    (void)body;
    return Result{false, false, {}, {}, {}, "feedback transport is unsupported", Error::Network};
#endif
}

struct AsyncSubmission::State {
    mutable std::mutex mutex;
    bool ready = false;
    Result result;
};

AsyncSubmission AsyncSubmission::Start(std::function<Result()> operation) {
    auto state = std::make_shared<State>();
    std::thread([state, operation = std::move(operation)]() mutable {
        Result result;
        try {
            result = operation();
        } catch (...) {
            result.error = Error::MalformedResponse;
            result.detail = "feedback operation failed";
        }
        std::lock_guard lock(state->mutex);
        state->result = std::move(result);
        state->ready = true;
    }).detach();
    return AsyncSubmission(std::move(state));
}

std::optional<Result> AsyncSubmission::Poll() const {
    if (!state_) return std::nullopt;
    std::lock_guard lock(state_->mutex);
    if (!state_->ready) return std::nullopt;
    return state_->result;
}

} // namespace CueviFeedback
