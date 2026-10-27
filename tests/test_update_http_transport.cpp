#include "update/UpdateHttpTransport.h"
#include "update/UpdateReleaseConfig.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace aris::update;

int failures = 0;
void check(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

DownloadRequest request() {
    DownloadRequest value;
    value.version = "1.2.0";
    value.relativeTarget = "Aris-1.2.0-win-x64-update.zip";
    value.url = "https://github.com/xindong/harken-search/releases/download/v1.2.0/Aris-1.2.0-win-x64-update.zip";
    value.expectedSize = 1000;
    value.expectedSha256 = std::string(64, 'a');
    return value;
}

DownloadAdmission admission(const DownloadRequest& value) {
    return UpdateDownloadManager::Admit(DownloadFlightState{}, value);
}

bool hasHeader(const HttpTransferRequestPlan& plan,
               const std::string& name,
               const std::string& value) {
    for (const auto& header : plan.headers) {
        if (header.name == name && header.value == value) return true;
    }
    return false;
}

void test_request_headers_and_owned_relative_identity() {
    const auto value = request();
    const auto admitted = admission(value);
    ResumeDecision fresh;
    const auto freshPlan = UpdateHttpTransport::BuildRequest(
        value, fresh, admitted.state, *admitted.token);
    check(freshPlan.ok() && freshPlan.plan.resumeOffset == 0 &&
              freshPlan.plan.identity.partPath ==
                  "1.2.0/Aris-1.2.0-win-x64-update.zip.part" &&
              !hasHeader(freshPlan.plan, "Range", "bytes=0-"),
          "fresh HTTPS request has no Range and owns only relative staging identity");

    ResumeDecision resume;
    resume.mode = ResumeMode::Resume;
    resume.offset = 400;
    resume.ifRange = "etag-120";
    const auto resumePlan = UpdateHttpTransport::BuildRequest(
        value, resume, admitted.state, *admitted.token);
    check(resumePlan.ok() &&
              hasHeader(resumePlan.plan, "Range", "bytes=400-") &&
              hasHeader(resumePlan.plan, "If-Range", "etag-120"),
          "resume emits Range and ETag-derived If-Range");

    auto invalid = value;
    invalid.url = "http://github.com/xindong/harken-search/releases/download/v1.2.0/Aris.zip";
    check(!UpdateHttpTransport::BuildRequest(
              invalid, fresh, admitted.state, *admitted.token).ok(),
          "transport refuses non-HTTPS and non-approved asset URLs");

    auto other = value;
    other.version = "1.3.0";
    other.url = "https://github.com/xindong/harken-search/releases/download/v1.3.0/Aris.zip";
    check(!UpdateHttpTransport::BuildRequest(
              value, fresh, admission(other).state, *admission(other).token).ok(),
          "flight token for a different staging identity cannot start a request");
    check(UpdateHttpTransport::IsApprovedRedirectUrl(
              "https://release-assets.githubusercontent.com/github-production-release-asset/aris") &&
              !UpdateHttpTransport::IsApprovedRedirectUrl(
                  "https://example.invalid/redirected.zip"),
          "redirect targets are HTTPS and host-allowlisted explicitly");
    check(UpdateHttpTransport::IsApprovedRedirectUrl(
              "https://github.com/xindong/harken-search/releases/download/v1.0.24/Aris-1.0.24-win-x64-update.zip"),
          "canonical same-repository asset redirect is accepted");
    check(ResolveUpdateDownloadUrl(value.url) ==
              "https://harkensearch.xd.com/aris/releases/download/v1.2.0/Aris-1.2.0-win-x64-update.zip" &&
              ResolveUpdateDownloadUrl(
                  "https://github.com/xindong/harken-search/releases/latest/download/Aris.zip") ==
                  "https://github.com/xindong/harken-search/releases/latest/download/Aris.zip" &&
              ResolveUpdateDownloadUrl(
                  "https://github.com/attacker/aris/releases/download/v1.2.0/Aris.zip") ==
                  "https://github.com/attacker/aris/releases/download/v1.2.0/Aris.zip" &&
              ResolveUpdateDownloadUrl(
                  "https://github.com/xindong/harken-search/releases/download/v1.2.0/../Aris.zip") ==
                  "https://github.com/xindong/harken-search/releases/download/v1.2.0/../Aris.zip",
          "only a versioned trusted GitHub ZIP maps to COS before network I/O");
    check(UpdateHttpTransport::IsApprovedRedirectUrl(
              "https://harkensearch.xd.com/aris/releases/download/v1.2.0/Aris-1.2.0-win-x64-update.zip") &&
              !UpdateHttpTransport::IsApprovedRedirectUrl(
                  "https://harkensearch.xd.com/other/Aris-1.2.0-win-x64-update.zip") &&
              !UpdateHttpTransport::IsApprovedRedirectUrl(
                  "http://harkensearch.xd.com/aris/releases/download/v1.2.0/Aris.zip") &&
              !UpdateHttpTransport::IsApprovedRedirectUrl(
                  "https://harkensearch.xd.com/aris/releases/download/v1.2.0/../Aris.zip"),
          "COS redirects stay on HTTPS harkensearch.xd.com/aris/");
    check(!UpdateHttpTransport::IsApprovedRedirectUrl(
              "https://github.com/zhongziyu-cmd/aris-releases/releases/download/v1.0.24/Aris.zip") &&
              !UpdateHttpTransport::IsApprovedRedirectUrl(
                  "https://github.com/attacker/aris-releases/releases/download/v1/Aris.zip"),
          "legacy and arbitrary repositories are refused on asset redirects");
    check(!UpdateHttpTransport::IsApprovedRedirectUrl(
              "http://github.com/xindong/harken-search/releases/download/v1/x.zip") &&
              !UpdateHttpTransport::IsApprovedRedirectUrl(
                  "https://github.com.evil.example/xindong/harken-search/releases/download/v1/x.zip") &&
              !UpdateHttpTransport::IsApprovedRedirectUrl(
                  "https://github.com/xindong/harken-search/blob/main/x.zip") &&
              !UpdateHttpTransport::IsApprovedRedirectUrl(
                  "https://github.com/xindong/harken-search/releases/download/../../secret"),
          "canonical redirect policy refuses plaintext HTTP, lookalike hosts, non-release paths and traversal");
}

void test_response_protocol_and_retry_classification() {
    const auto value = request();
    const auto admitted = admission(value);
    ResumeDecision resume;
    resume.mode = ResumeMode::Resume;
    resume.offset = 400;
    resume.ifRange = "etag-120";
    const auto plan = UpdateHttpTransport::BuildRequest(
        value, resume, admitted.state, *admitted.token).plan;

    HttpResponseHead partial;
    partial.statusCode = 206;
    partial.contentRange = "bytes 400-999/1000";
    partial.contentLength = 600;
    const auto accepted = UpdateHttpTransport::EvaluateResponse(plan, partial);
    check(accepted.disposition == HttpResponseDisposition::AcceptAppend &&
              accepted.bodyStart == 400 && accepted.expectedBodyBytes == 600 &&
              accepted.resumeMetadata.metaPath ==
                  "1.2.0/Aris-1.2.0-win-x64-update.zip.meta.json" &&
              accepted.resumeMetadata.metadata.downloadedBytes == 400,
          "matching 206 Content-Range appends at the negotiated offset");

    auto mismatch = partial;
    mismatch.contentRange = "bytes 399-999/1000";
    check(UpdateHttpTransport::EvaluateResponse(plan, mismatch).error ==
              DownloadErrorKind::InvalidArgument,
          "Content-Range start mismatch fails closed");

    HttpResponseHead full;
    full.statusCode = 200;
    full.contentLength = 1000;
    const auto fallback = UpdateHttpTransport::EvaluateResponse(plan, full);
    check(fallback.disposition == HttpResponseDisposition::RestartFresh &&
              fallback.cleanupIntents.size() == 2,
          "resume receiving 200 explicitly resets part and meta before fresh retry");

    HttpResponseHead notSatisfiable;
    notSatisfiable.statusCode = 416;
    const auto terminal = UpdateHttpTransport::EvaluateResponse(
        plan, notSatisfiable);
    check(terminal.error == DownloadErrorKind::Http4xx &&
              !UpdateDownloadManager::DecideRetry(
                  terminal.error, 1, RetryPolicy{}).retry,
          "416 is terminal 4xx");
    HttpResponseHead unavailable;
    unavailable.statusCode = 503;
    const auto transient = UpdateHttpTransport::EvaluateResponse(
        plan, unavailable);
    check(transient.error == DownloadErrorKind::Http5xx &&
              UpdateDownloadManager::DecideRetry(
                  transient.error, 1, RetryPolicy{}).retry,
          "5xx remains retryable through the existing retry policy");
}

void test_chunk_eof_hash_handoff_and_stale_identity() {
    const auto value = request();
    const auto admitted = admission(value);
    const auto plan = UpdateHttpTransport::BuildRequest(
        value, ResumeDecision{}, admitted.state, *admitted.token).plan;
    HttpResponseHead head;
    head.statusCode = 200;
    head.contentLength = 1000;
    const auto response = UpdateHttpTransport::EvaluateResponse(plan, head);
    auto session = UpdateHttpTransport::BeginSession(plan, response);

    std::vector<std::uint8_t> first(600, 1);
    const auto chunk = UpdateHttpTransport::AcceptChunk(
        session, admitted.state, plan.identity, first);
    check(chunk.accepted && chunk.intent.offset == 0 &&
              chunk.intent.partPath == plan.identity.partPath &&
              chunk.intent.bytes.size() == 600 &&
              chunk.resumeMetadata.metadata.downloadedBytes == 600,
          "stream chunk becomes an offset-bound append intent");
    session = chunk.session;

    auto stale = plan.identity;
    ++stale.generation;
    check(!UpdateHttpTransport::AcceptChunk(
              session, admitted.state, stale,
              std::vector<std::uint8_t>(1, 2)).accepted,
          "stale generation cannot append to a newer transfer");
    check(!UpdateHttpTransport::Finish(
              session, admitted.state, plan.identity).success,
          "truncated EOF fails closed before hash handoff");

    const auto finalChunk = UpdateHttpTransport::AcceptChunk(
        session, admitted.state, plan.identity,
        std::vector<std::uint8_t>(400, 2));
    const auto finished = UpdateHttpTransport::Finish(
        finalChunk.session, admitted.state, plan.identity);
    check(finished.success && finished.verify.partPath == plan.identity.partPath &&
              finished.verify.expectedSize == 1000 &&
              finished.verify.expectedSha256 == value.expectedSha256,
          "exact EOF hands relative part identity to trusted hash verification");

    check(!UpdateHttpTransport::AcceptChunk(
              finalChunk.session, admitted.state, plan.identity,
              std::vector<std::uint8_t>(1, 3)).accepted,
          "overlong response is rejected before an append intent is emitted");

    const auto released = UpdateDownloadManager::Release(
        admitted.state, *admitted.token);
    const auto newer = UpdateDownloadManager::Admit(released, value);
    check(!UpdateHttpTransport::BuildRequest(
              value, ResumeDecision{}, newer.state, *admitted.token).ok() &&
              !UpdateHttpTransport::AcceptChunk(
                  session, newer.state, plan.identity,
                  std::vector<std::uint8_t>(1, 4)).accepted,
          "released generation cannot build or append after same-key re-admission");
}

}  // namespace

int main() {
    test_request_headers_and_owned_relative_identity();
    test_response_protocol_and_retry_classification();
    test_chunk_eof_hash_handoff_and_stale_identity();
    return failures == 0 ? 0 : 1;
}
