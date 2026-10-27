#include "platform/update_http_transport.h"
#include "update/UpdateReleaseConfig.h"

#if defined(AURA_BUILD_DIAGNOSTICS) && AURA_BUILD_DIAGNOSTICS
#include "DiagnosticsLogger.h"
#define ARIS_UPDATE_DIAG(detail) DIAG_SYSTEM("AutoUpdate", detail)
#else
#define ARIS_UPDATE_DIAG(detail) do {} while (0)
#endif

#ifdef ARIS_UPDATE_HTTP_TRANSPORT_TESTING
#include "platform/update_http_transport_test_hooks.h"
#endif

#ifdef _WIN32

#include "platform/text_encoding.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wininet.h>

#include <algorithm>
#include <charconv>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Platform::UpdateHttp {
namespace {

using aris::update::HttpResponseHead;
using aris::update::HttpTransferRequestPlan;
using aris::update::HttpTransportCancellation;
using aris::update::HttpTransportExecutionError;
using aris::update::HttpTransportExecutionResult;
using aris::update::HttpTransportPort;
using aris::update::HttpTransportSink;

class ScopedInternet {
public:
    ScopedInternet() = default;
    explicit ScopedInternet(HINTERNET handle) : handle_(handle) {}
    ~ScopedInternet() { if (handle_) InternetCloseHandle(handle_); }
    ScopedInternet(const ScopedInternet&) = delete;
    ScopedInternet& operator=(const ScopedInternet&) = delete;
    ScopedInternet(ScopedInternet&& other) noexcept : handle_(other.release()) {}
    ScopedInternet& operator=(ScopedInternet&& other) noexcept {
        if (this != &other) {
            if (handle_) InternetCloseHandle(handle_);
            handle_ = other.release();
        }
        return *this;
    }
    HINTERNET get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }
    HINTERNET release() {
        const auto value = handle_;
        handle_ = nullptr;
        return value;
    }
private:
    HINTERNET handle_ = nullptr;
};

HttpTransportExecutionResult Failure(HttpTransportExecutionError error,
                                     DWORD systemError = ERROR_SUCCESS,
                                     std::uint64_t streamed = 0) {
    HttpTransportExecutionResult result;
    result.error = error;
    result.systemError = systemError;
    result.streamedBytes = streamed;
    return result;
}

bool IsSafePlan(const HttpTransferRequestPlan& plan) {
    // Must match the release repository the build was pinned to or the
    // company-cloud distribution origin. Both are set at compile time (see
    // UpdateReleaseConfig.h), so releases published to a different owner/repo
    // or origin are rejected here even if the manifest slipped through.
    static const std::string githubPrefix =
        std::string(ARIS_UPDATE_GITHUB_RELEASE_DOWNLOAD_PREFIX);
    static const std::string companyPrefix =
        std::string(ARIS_UPDATE_COMPANY_RELEASE_DOWNLOAD_PREFIX);
    const bool approvedUrl = plan.url.rfind(githubPrefix, 0) == 0 ||
                             plan.url.rfind(companyPrefix, 0) == 0;
    if (!approvedUrl || plan.identity.flightKey.empty() ||
        plan.identity.generation == 0 || plan.identity.partPath.empty() ||
        plan.expectedSize == 0 || plan.expectedSha256.size() != 64) return false;
    for (const auto& header : plan.headers) {
        if ((header.name != "Range" && header.name != "If-Range") ||
            header.value.empty() || header.value.find('\r') != std::string::npos ||
            header.value.find('\n') != std::string::npos) return false;
    }
    return true;
}

std::string BuildHeaders(const HttpTransferRequestPlan& plan) {
    std::string headers;
    for (const auto& header : plan.headers) {
        headers += header.name + ": " + header.value + "\r\n";
    }
    return headers;
}

bool QueryHeader(HINTERNET request, DWORD query, std::string& value) {
    DWORD bytes = 0;
    if (HttpQueryInfoW(request, query, nullptr, &bytes, nullptr)) return false;
    if (GetLastError() == ERROR_HTTP_HEADER_NOT_FOUND) {
        value.clear();
        return true;
    }
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || bytes < sizeof(wchar_t)) {
        return false;
    }
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
    if (!HttpQueryInfoW(request, query, buffer.data(), &bytes, nullptr)) return false;
    value = Platform::TextEncoding::WideToUtf8Strict(buffer.data());
    return !value.empty();
}

bool ParseUint64(const std::string& text, std::uint64_t& value) {
    if (text.empty()) return false;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool QueryResponseHead(HINTERNET request, HttpResponseHead& head) {
    DWORD status = 0;
    DWORD bytes = sizeof(status);
    if (!HttpQueryInfoW(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                        &status, &bytes, nullptr)) return false;
    head.statusCode = status;
    std::string length;
    if (!QueryHeader(request, HTTP_QUERY_CONTENT_LENGTH, length)) return false;
    if (!length.empty()) {
        std::uint64_t parsed = 0;
        if (!ParseUint64(length, parsed)) return false;
        head.contentLength = parsed;
    }
    if (!QueryHeader(request, HTTP_QUERY_CONTENT_RANGE, head.contentRange) ||
        !QueryHeader(request, HTTP_QUERY_ETAG, head.etag) ||
        !QueryHeader(request, HTTP_QUERY_LAST_MODIFIED, head.lastModified) ||
        !QueryHeader(request, HTTP_QUERY_LOCATION, head.location)) {
        return false;
    }
    return true;
}

enum class StreamReadStatus { Chunk, Eof, Error };

struct StreamReadResult {
    StreamReadStatus status = StreamReadStatus::Error;
    std::vector<std::uint8_t> bytes;
    std::uint32_t systemError = 0;
};

class StreamApi {
public:
    virtual ~StreamApi() = default;
    virtual bool Open(const std::string& url,
                      const std::string& headers,
                      std::uint32_t& systemError) = 0;
    virtual bool Query(HttpResponseHead& head,
                       std::uint32_t& systemError) = 0;
    virtual StreamReadResult Read() = 0;
    virtual void Interrupt() = 0;
    virtual void Close() = 0;
};

class ScopedApiRequest {
public:
    explicit ScopedApiRequest(StreamApi& api) : api_(api) {}
    ~ScopedApiRequest() { api_.Close(); }
private:
    StreamApi& api_;
};

class ScopedInterruptHandler {
public:
    ScopedInterruptHandler(const HttpTransportCancellation& cancellation,
                           StreamApi& api)
        : cancellation_(cancellation) {
        registration_ = cancellation_.RegisterInterrupt(
            [&api] { api.Interrupt(); });
    }
private:
    const HttpTransportCancellation& cancellation_;
    std::unique_ptr<HttpTransportCancellation::InterruptRegistration>
        registration_;
};

class WinInetApi final : public StreamApi {
public:
    explicit WinInetApi(ScopedInternet session) : session_(std::move(session)) {}

    bool Open(const std::string& url,
              const std::string& headers,
              std::uint32_t& systemError) override {
        const std::wstring wideUrl = Platform::TextEncoding::Utf8ToWideStrict(url);
        const std::wstring wideHeaders =
            Platform::TextEncoding::Utf8ToWideStrict(headers);
        if (wideUrl.empty() || (!headers.empty() && wideHeaders.empty())) {
            systemError = ERROR_NO_UNICODE_TRANSLATION;
            return false;
        }
        DWORD flags = INTERNET_FLAG_RELOAD |
                                INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI |
                                INTERNET_FLAG_NO_AUTO_REDIRECT;
        if (url.rfind("https://", 0) == 0) flags |= INTERNET_FLAG_SECURE;
        ScopedInternet opened(InternetOpenUrlW(
            session_.get(), wideUrl.c_str(),
            wideHeaders.empty() ? nullptr : wideHeaders.c_str(),
            wideHeaders.empty() ? 0 : static_cast<DWORD>(wideHeaders.size()),
            flags, 0));
        if (!opened) {
            systemError = GetLastError();
            return false;
        }
        std::lock_guard<std::mutex> lock(requestMutex_);
        request_ = std::move(opened);
        return true;
    }

    bool Query(HttpResponseHead& head,
               std::uint32_t& systemError) override {
        HINTERNET request = nullptr;
        {
            std::lock_guard<std::mutex> lock(requestMutex_);
            request = request_.get();
        }
        if (request && QueryResponseHead(request, head)) return true;
        systemError = GetLastError();
        return false;
    }

    StreamReadResult Read() override {
        std::vector<std::uint8_t> buffer(64 * 1024);
        DWORD bytesRead = 0;
        HINTERNET request = nullptr;
        {
            std::lock_guard<std::mutex> lock(requestMutex_);
            request = request_.get();
        }
        if (!request || !InternetReadFile(request, buffer.data(),
                              static_cast<DWORD>(buffer.size()), &bytesRead)) {
            return {StreamReadStatus::Error, {}, GetLastError()};
        }
        if (bytesRead == 0) return {StreamReadStatus::Eof, {}, 0};
        buffer.resize(bytesRead);
        return {StreamReadStatus::Chunk, std::move(buffer), 0};
    }

    void Interrupt() override { Close(); }

    void Close() override {
        ScopedInternet closing;
        {
            std::lock_guard<std::mutex> lock(requestMutex_);
            closing = std::move(request_);
        }
    }

private:
    ScopedInternet session_;
    ScopedInternet request_;
    std::mutex requestMutex_;
};

class StreamingTransport final : public HttpTransportPort {
public:
    explicit StreamingTransport(std::unique_ptr<StreamApi> api)
        : api_(std::move(api)) {}

    HttpTransportExecutionResult Execute(
        const HttpTransferRequestPlan& plan,
        const HttpTransportCancellation& cancellation,
        HttpTransportSink& sink) override {
        if (!IsSafePlan(plan)) return Failure(HttpTransportExecutionError::InvalidPlan);
        ScopedInterruptHandler interrupt(cancellation, *api_);
        if (cancellation.IsCancellationRequested()) {
            return Failure(HttpTransportExecutionError::Cancelled,
                           ERROR_OPERATION_ABORTED);
        }
        std::uint32_t systemError = 0;
        HttpResponseHead head;
        std::string requestUrl =
            aris::update::ResolveUpdateDownloadUrl(plan.url);
        ARIS_UPDATE_DIAG("PackageSourceAttempt=COS");
        constexpr std::uint32_t kMaxRedirects = 5;
        std::uint32_t redirects = 0;
        for (;;) {
            if (!api_->Open(requestUrl, BuildHeaders(plan), systemError)) {
                if (cancellation.IsCancellationRequested()) {
                    return Failure(HttpTransportExecutionError::Cancelled,
                                   ERROR_OPERATION_ABORTED);
                }
                return Failure(HttpTransportExecutionError::Network, systemError);
            }
            if (cancellation.IsCancellationRequested()) {
                api_->Close();
                return Failure(HttpTransportExecutionError::Cancelled,
                               ERROR_OPERATION_ABORTED);
            }
            head = {};
            if (!api_->Query(head, systemError)) {
                api_->Close();
                if (cancellation.IsCancellationRequested()) {
                    return Failure(HttpTransportExecutionError::Cancelled,
                                   ERROR_OPERATION_ABORTED);
                }
                return Failure(HttpTransportExecutionError::Network, systemError);
            }
            if (head.statusCode < 300 || head.statusCode >= 400) break;
            api_->Close();
            if (++redirects > kMaxRedirects ||
                !aris::update::UpdateHttpTransport::IsApprovedRedirectUrl(
                    head.location)) {
                return Failure(HttpTransportExecutionError::InvalidPlan);
            }
            requestUrl = head.location;
        }
        if (head.statusCode < 200 || head.statusCode >= 300) {
            api_->Close();
            return Failure(HttpTransportExecutionError::Network, systemError);
        }

        ScopedApiRequest close(*api_);
        if (!sink.OnResponseHead(head)) {
            return Failure(HttpTransportExecutionError::SinkRejected);
        }

        std::uint64_t streamed = 0;
        for (;;) {
            if (cancellation.IsCancellationRequested()) {
                return Failure(HttpTransportExecutionError::Cancelled,
                               ERROR_OPERATION_ABORTED, streamed);
            }
            auto read = api_->Read();
            if (read.status == StreamReadStatus::Error) {
                if (cancellation.IsCancellationRequested()) {
                    return Failure(HttpTransportExecutionError::Cancelled,
                                   ERROR_OPERATION_ABORTED, streamed);
                }
                return Failure(HttpTransportExecutionError::Network,
                               read.systemError, streamed);
            }
            if (read.status == StreamReadStatus::Eof) break;
            if (read.bytes.empty() || !sink.OnChunk(read.bytes)) {
                return Failure(HttpTransportExecutionError::SinkRejected, 0,
                               streamed);
            }
            streamed += read.bytes.size();
        }
        if (!sink.OnEof()) {
            return Failure(HttpTransportExecutionError::SinkRejected, 0,
                           streamed);
        }
        ARIS_UPDATE_DIAG("PackageSource=COS");
        return Failure(HttpTransportExecutionError::None, 0, streamed);
    }

private:
    std::unique_ptr<StreamApi> api_;
};

#ifdef ARIS_UPDATE_HTTP_TRANSPORT_TESTING
class TestApiBridge final : public StreamApi {
public:
    explicit TestApiBridge(std::unique_ptr<Testing::NativeHttpApi> api)
        : api_(std::move(api)) {}
    bool Open(const std::string& url, const std::string& headers,
              std::uint32_t& error) override {
        return api_->Open(url, headers, error);
    }
    bool Query(HttpResponseHead& head, std::uint32_t& error) override {
        return api_->QueryResponseHead(head, error);
    }
    StreamReadResult Read() override {
        auto value = api_->Read();
        StreamReadStatus status = StreamReadStatus::Error;
        if (value.status == Testing::NativeReadStatus::Chunk) {
            status = StreamReadStatus::Chunk;
        } else if (value.status == Testing::NativeReadStatus::Eof) {
            status = StreamReadStatus::Eof;
        }
        return {status, std::move(value.bytes), value.systemError};
    }
    void Interrupt() override { api_->Interrupt(); }
    void Close() override { api_->Close(); }
private:
    std::unique_ptr<Testing::NativeHttpApi> api_;
};
#endif

}  // namespace

CreateResult CreateUpdateHttpTransportPort() {
    ScopedInternet session(InternetOpenW(
        L"Harken Search Update/1", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0));
    if (!session) {
        CreateResult result;
        result.status = CreateStatus::SessionOpenFailed;
        result.systemError = GetLastError();
        return result;
    }
    DWORD connectTimeoutMs = 10000;
    DWORD receiveTimeoutMs = 30000;
    DWORD sendTimeoutMs = 10000;
    if (!InternetSetOptionW(session.get(), INTERNET_OPTION_CONNECT_TIMEOUT,
                            &connectTimeoutMs, sizeof(connectTimeoutMs)) ||
        !InternetSetOptionW(session.get(), INTERNET_OPTION_RECEIVE_TIMEOUT,
                            &receiveTimeoutMs, sizeof(receiveTimeoutMs)) ||
        !InternetSetOptionW(session.get(), INTERNET_OPTION_SEND_TIMEOUT,
                            &sendTimeoutMs, sizeof(sendTimeoutMs))) {
        CreateResult result;
        result.status = CreateStatus::SessionOpenFailed;
        result.systemError = GetLastError();
        return result;
    }
    CreateResult result;
    result.status = CreateStatus::Success;
    result.port = std::make_unique<StreamingTransport>(
        std::make_unique<WinInetApi>(std::move(session)));
    return result;
}

#ifdef ARIS_UPDATE_HTTP_TRANSPORT_TESTING
namespace Testing {

std::unique_ptr<aris::update::HttpTransportPort> CreateWithNativeApi(
    std::unique_ptr<NativeHttpApi> api) {
    if (!api) return {};
    return std::make_unique<StreamingTransport>(
        std::make_unique<TestApiBridge>(std::move(api)));
}

}  // namespace Testing
#endif

}  // namespace Platform::UpdateHttp

#else

namespace Platform::UpdateHttp {

CreateResult CreateUpdateHttpTransportPort() {
    CreateResult result;
    result.status = CreateStatus::Unsupported;
    return result;
}

}  // namespace Platform::UpdateHttp

#endif

#undef ARIS_UPDATE_DIAG
