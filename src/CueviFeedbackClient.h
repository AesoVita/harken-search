#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace CueviFeedback {

// @stage7-contract
// API contract: Feedback DTOs own all strings and maps. Validation and serialization are
// synchronous value operations. Submit performs one bounded transport call
// using Config::timeoutMs. AsyncSubmission owns shared completion state;
// Start copies its operation into a worker and Poll is a non-blocking snapshot
// that may be called repeatedly until a Result is available.
enum class Category { Bug, Requirement };
enum class Source { Manual, CrashRecovery };
enum class Error { None, NotConfigured, InvalidEndpoint, Network, Timeout, Unauthorized, RateLimited, Validation, Server, MalformedResponse };

struct Submission {
    std::string projectId;
    std::string boardId;
    std::string milestoneId;
    Category category = Category::Bug;
    Source source = Source::Manual;
    std::string title;
    std::string description;
    std::string submittedAt;
    std::string productVersion;
    std::string buildId;
    std::string releaseChannel;
    std::map<std::string, std::string> environment;
    bool anonymous = true;
    std::string reporter;
    std::string diagnosticLog;
    std::string submissionId;
    std::string idempotencyKey;
};

struct Endpoint {
    bool secure = true;
    std::wstring host;
    std::wstring path;
    uint16_t port = 443;
};

struct Config {
    std::string endpointUrl;
    std::string restrictedKey;
    uint32_t timeoutMs = 15000;
    std::string projectId;
    std::string boardId;
    std::string milestoneId;
};

struct Result {
    bool ok = false;
    bool replayed = false;
    std::string workItemId;
    std::string receivedAt;
    std::string webUrl;
    std::string detail;
    Error error = Error::None;
};

std::string BuildSubmissionJson(const Submission& input);
Error ValidateSubmission(const Submission& input);
Result ParseResponse(uint32_t statusCode, const std::string& body, Category expectedCategory);
std::optional<Endpoint> ParseEndpoint(const std::string& url);
std::string GenerateSubmissionId();
bool IsUuid(const std::string& value);
bool FitDiagnosticLogToSubmissionLimits(Submission& input);
bool IsUiEntryEnabled(const Config& config);
Result Submit(const Config& config, const Submission& input);

class AsyncSubmission {
public:
    AsyncSubmission() = default;
    AsyncSubmission(AsyncSubmission&&) noexcept = default;
    AsyncSubmission& operator=(AsyncSubmission&&) noexcept = default;
    AsyncSubmission(const AsyncSubmission&) = default;
    AsyncSubmission& operator=(const AsyncSubmission&) = default;

    static AsyncSubmission Start(std::function<Result()> operation);
    std::optional<Result> Poll() const;
    bool valid() const { return static_cast<bool>(state_); }

private:
    struct State;
    explicit AsyncSubmission(std::shared_ptr<State> state) : state_(std::move(state)) {}
    std::shared_ptr<State> state_;
};

} // namespace CueviFeedback
