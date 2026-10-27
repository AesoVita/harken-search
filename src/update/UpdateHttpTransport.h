#pragma once

#include "update/UpdateDownloadManager.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aris::update {

// @stage7-contract
// Owned-value HTTP transfer contract. The pure policy owns protocol decisions;
// platform adapters own native requests only and deliver response/chunk events.
// No absolute filesystem root or platform handle crosses this boundary.

struct HttpRequestHeader {
    std::string name;
    std::string value;
};

struct HttpTransferIdentity {
    std::string flightKey;
    std::uint64_t generation = 0;
    std::string partPath;
};

struct HttpTransferRequestPlan {
    std::string url;
    std::vector<HttpRequestHeader> headers;
    HttpTransferIdentity identity;
    std::uint64_t expectedSize = 0;
    std::string expectedSha256;
    std::uint64_t resumeOffset = 0;
    std::string metaPath;
};

enum class HttpRequestPlanError {
    None,
    InvalidDownloadRequest,
    UnapprovedUrl,
    InvalidResume,
    InvalidFlight,
};

struct HttpTransferRequestPlanResult {
    HttpRequestPlanError error = HttpRequestPlanError::None;
    HttpTransferRequestPlan plan;
    bool ok() const { return error == HttpRequestPlanError::None; }
};

struct HttpResponseHead {
    std::uint32_t statusCode = 0;
    std::optional<std::uint64_t> contentLength;
    std::string contentRange;
    std::string etag;
    std::string lastModified;
    std::string location;
};

enum class HttpResponseDisposition {
    Reject,
    AcceptFresh,
    AcceptAppend,
    RestartFresh,
};

struct ResumeMetadataIntent {
    HttpTransferIdentity identity;
    std::string metaPath;
    ResumeMetadata metadata;
};

struct HttpResponseDecision {
    HttpResponseDisposition disposition = HttpResponseDisposition::Reject;
    DownloadErrorKind error = DownloadErrorKind::InvalidArgument;
    std::uint64_t bodyStart = 0;
    std::uint64_t expectedBodyBytes = 0;
    RemoteValidators validators;
    ResumeMetadataIntent resumeMetadata;
    std::vector<DownloadCleanupIntent> cleanupIntents;
};

struct HttpTransferSession {
    HttpTransferIdentity identity;
    std::uint64_t expectedSize = 0;
    std::uint64_t nextOffset = 0;
    std::string expectedSha256;
    ResumeMetadataIntent resumeMetadata;
    bool active = false;
};

struct TransferAppendIntent {
    HttpTransferIdentity identity;
    std::string partPath;
    std::uint64_t offset = 0;
    std::vector<std::uint8_t> bytes;
};

struct HttpChunkDecision {
    bool accepted = false;
    DownloadErrorKind error = DownloadErrorKind::InvalidArgument;
    HttpTransferSession session;
    TransferAppendIntent intent;
    ResumeMetadataIntent resumeMetadata;
};

struct VerifyPartIntent {
    HttpTransferIdentity identity;
    std::string partPath;
    std::uint64_t expectedSize = 0;
    std::string expectedSha256;
};

struct HttpEofDecision {
    bool success = false;
    DownloadErrorKind error = DownloadErrorKind::Network;
    VerifyPartIntent verify;
};

class UpdateHttpTransport {
public:
    static HttpTransferRequestPlanResult BuildRequest(
        const DownloadRequest& request,
        const ResumeDecision& resume,
        const DownloadFlightState& activeFlight,
        const DownloadFlightToken& token);
    static bool IsApprovedRedirectUrl(const std::string& url);
    static HttpResponseDecision EvaluateResponse(
        const HttpTransferRequestPlan& plan,
        const HttpResponseHead& response);
    static HttpTransferSession BeginSession(
        const HttpTransferRequestPlan& plan,
        const HttpResponseDecision& response);
    static HttpChunkDecision AcceptChunk(
        const HttpTransferSession& session,
        const DownloadFlightState& activeFlight,
        const HttpTransferIdentity& identity,
        const std::vector<std::uint8_t>& bytes);
    static HttpEofDecision Finish(
        const HttpTransferSession& session,
        const DownloadFlightState& activeFlight,
        const HttpTransferIdentity& identity);
};

class HttpTransportCancellation {
public:
    virtual ~HttpTransportCancellation() = default;
    virtual bool IsCancellationRequested() const = 0;
    class InterruptRegistration {
    public:
        virtual ~InterruptRegistration() = default;
    };
    // The returned registration owns a barrier: its destructor must not return
    // while a copied/in-flight handler can still execute.
    virtual std::unique_ptr<InterruptRegistration> RegisterInterrupt(
        std::function<void()> handler) const = 0;
};

class HttpTransportSink {
public:
    virtual ~HttpTransportSink() = default;
    virtual bool OnResponseHead(const HttpResponseHead& response) = 0;
    virtual bool OnChunk(const std::vector<std::uint8_t>& bytes) = 0;
    virtual bool OnEof() = 0;
};

enum class HttpTransportExecutionError {
    None,
    InvalidPlan,
    Cancelled,
    Network,
    SinkRejected,
};

struct HttpTransportExecutionResult {
    HttpTransportExecutionError error = HttpTransportExecutionError::None;
    std::uint32_t systemError = 0;
    std::uint64_t streamedBytes = 0;
    bool ok() const { return error == HttpTransportExecutionError::None; }
};

class HttpTransportPort {
public:
    virtual ~HttpTransportPort() = default;
    virtual HttpTransportExecutionResult Execute(
        const HttpTransferRequestPlan& plan,
        const HttpTransportCancellation& cancellation,
        HttpTransportSink& sink) = 0;
};

}  // namespace aris::update
