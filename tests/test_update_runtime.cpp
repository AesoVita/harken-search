#include "update/UpdateRuntime.h"

#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace aris::update;

static_assert(static_cast<int>(UpdateRuntimeDisposition::NoOp) == 0);
static_assert(static_cast<int>(UpdateRuntimeDisposition::Ready) == 1);
static_assert(static_cast<int>(UpdateRuntimeDisposition::RetryScheduled) == 2);
static_assert(static_cast<int>(UpdateRuntimeDisposition::Cancelled) == 3);
static_assert(static_cast<int>(UpdateRuntimeDisposition::TerminalError) == 4);
static_assert(static_cast<int>(UpdateRuntimeDisposition::CheckStarted) == 5);
static_assert(static_cast<int>(UpdateRuntimeDisposition::UpdateAvailable) == 6);
static_assert(static_cast<int>(UpdateRuntimeDisposition::NoUpdate) == 7);
static_assert(static_cast<int>(UpdateRuntimeDisposition::RestartAuthorized) == 8);

int failures = 0;
void check(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

UpdateCandidate candidate(const std::string& version, char hash = 'a') {
    UpdateCandidate value;
    value.version = version;
    value.buildId = "build-" + version;
    value.assetUrl = "https://github.com/xindong/harken-search/releases/download/v" +
                     version + "/Aris-" + version + "-win-x64-update.zip";
    value.assetSize = 4;
    value.sha256 = std::string(64, hash);
    return value;
}

std::string target(const std::string& version) {
    return "Aris-" + version + "-win-x64-update.zip";
}

class NeverCancelled final : public HttpTransportCancellation {
public:
    bool IsCancellationRequested() const override { return false; }
    std::unique_ptr<InterruptRegistration> RegisterInterrupt(
        std::function<void()>) const override {
        class Registration final : public InterruptRegistration {};
        return std::make_unique<Registration>();
    }
};

class LateCancellation final : public HttpTransportCancellation {
public:
    bool IsCancellationRequested() const override { return checks_++ != 0; }
    std::unique_ptr<InterruptRegistration> RegisterInterrupt(
        std::function<void()>) const override {
        class Registration final : public InterruptRegistration {};
        return std::make_unique<Registration>();
    }

private:
    mutable int checks_ = 0;
};

class PreCancelled final : public HttpTransportCancellation {
public:
    bool IsCancellationRequested() const override { return true; }
    std::unique_ptr<InterruptRegistration> RegisterInterrupt(
        std::function<void()>) const override {
        class Registration final : public InterruptRegistration {};
        return std::make_unique<Registration>();
    }
};

struct ScriptedAttempt {
    std::optional<HttpResponseHead> head;
    std::vector<std::vector<std::uint8_t>> chunks;
    HttpTransportExecutionError executionError =
        HttpTransportExecutionError::None;
    bool throws = false;
};

class ScriptedTransport final : public HttpTransportPort {
public:
    std::vector<ScriptedAttempt> attempts;
    std::vector<HttpTransferRequestPlan> plans;
    std::size_t executions = 0;

    HttpTransportExecutionResult Execute(
        const HttpTransferRequestPlan& plan,
        const HttpTransportCancellation&,
        HttpTransportSink& sink) override {
        plans.push_back(plan);
        HttpTransportExecutionResult result;
        if (executions >= attempts.size()) {
            result.error = HttpTransportExecutionError::Network;
            return result;
        }
        const auto script = attempts[executions++];
        if (script.throws) throw 7;
        if (script.executionError != HttpTransportExecutionError::None) {
            result.error = script.executionError;
            return result;
        }
        if (script.head && !sink.OnResponseHead(*script.head)) {
            result.error = HttpTransportExecutionError::SinkRejected;
            return result;
        }
        for (const auto& bytes : script.chunks) {
            if (!sink.OnChunk(bytes)) {
                result.error = HttpTransportExecutionError::SinkRejected;
                return result;
            }
            result.streamedBytes += bytes.size();
        }
        if (!sink.OnEof()) result.error = HttpTransportExecutionError::SinkRejected;
        return result;
    }
};

class FakePersistence final : public UpdatePersistencePort {
public:
    FakePersistence() { metadata.status = ResumeMetadataStatus::Missing; }

    ResumeMetadataObservation metadata;
    std::uint64_t partSize = 0;
    std::string partHash = std::string(64, 'a');
    std::optional<HttpTransferIdentity> active;
    int activates = 0;
    int deactivates = 0;
    int storePlans = 0;
    int metadataObservations = 0;
    int stagedObservations = 0;
    int failStoreAfter = -1;
    int failStoreAt = -1;
    bool failTransferChunk = false;
    bool throwActivate = false;
    bool throwDeactivate = false;
    std::vector<DownloadCleanupIntent> cleanup;
    std::vector<std::uint8_t> bytes;

    UpdatePersistenceResult ExecuteStoreIntents(
        const std::vector<StoreIntent>& intents) override {
        UpdatePersistenceResult result;
        if (failStoreAt >= 0 && storePlans == failStoreAt) {
            ++storePlans;
            result.error = UpdatePersistenceError::WriteFailed;
            return result;
        }
        if (failStoreAfter >= 0 && storePlans >= failStoreAfter) {
            ++storePlans;
            result.error = UpdatePersistenceError::WriteFailed;
            return result;
        }
        result.completedIntents = intents.size();
        ++storePlans;
        return result;
    }

    UpdatePersistenceResult ExecuteDownloadIntents(
        const std::vector<DownloadCleanupIntent>& intents) override {
        cleanup.insert(cleanup.end(), intents.begin(), intents.end());
        UpdatePersistenceResult result;
        result.completedIntents = intents.size();
        for (const auto& intent : intents) {
            if (intent.kind == DownloadCleanupIntentKind::DeletePart) {
                partSize = 0;
                bytes.clear();
            }
        }
        return result;
    }

    UpdatePersistenceResult ActivateTransfer(
        const DownloadFlightState& flight,
        const DownloadFlightToken& token,
        const HttpTransferIdentity& identity) override {
        UpdatePersistenceResult result;
        if (flight.activeKey != token.key() ||
            flight.generation != token.generation()) {
            result.error = UpdatePersistenceError::StaleGeneration;
            return result;
        }
        active = identity;
        ++activates;
        if (throwActivate) throw 11;
        return result;
    }

    UpdatePersistenceResult DeactivateTransfer(
        const DownloadFlightToken& token,
        const HttpTransferIdentity& identity) override {
        UpdatePersistenceResult result;
        if (active && (active->flightKey != token.key() ||
                       active->generation != token.generation() ||
                       active->partPath != identity.partPath)) {
            result.error = UpdatePersistenceError::StaleGeneration;
            return result;
        }
        active.reset();
        ++deactivates;
        if (throwDeactivate) throw 12;
        return result;
    }

    UpdatePersistenceResult ExecuteTransferChunk(
        const TransferAppendIntent& append,
        const ResumeMetadataIntent& meta) override {
        UpdatePersistenceResult result;
        if (failTransferChunk) {
            result.error = UpdatePersistenceError::WriteFailed;
            return result;
        }
        if (!active || active->generation != append.identity.generation ||
            append.offset != partSize ||
            meta.metadata.downloadedBytes != append.offset + append.bytes.size()) {
            result.error = UpdatePersistenceError::StaleGeneration;
            return result;
        }
        bytes.insert(bytes.end(), append.bytes.begin(), append.bytes.end());
        partSize += append.bytes.size();
        metadata.status = ResumeMetadataStatus::Present;
        metadata.metadata = meta.metadata;
        return result;
    }

    ResumeMetadataObservation ObserveResumeMetadata(
        const std::string&) override {
        ++metadataObservations;
        return metadata;
    }

    StagedIdentityObservation ObserveStagedPatch(
        const std::string& path) override {
        ++stagedObservations;
        StagedIdentityObservation result;
        result.identity.relativePath = path;
        if (partSize == 0) {
            result.status = StagedIdentityStatus::Missing;
            return result;
        }
        result.status = StagedIdentityStatus::Present;
        result.identity.exists = true;
        result.identity.size = partSize;
        result.identity.sha256 = partHash;
        return result;
    }
};

HttpResponseHead freshHead(int status = 200) {
    HttpResponseHead head;
    head.statusCode = status;
    head.contentLength = 4;
    head.etag = "etag-v1";
    return head;
}

UpdateRuntimeConfig config() {
    UpdateRuntimeConfig value;
    value.channel = UpdateChannel::Stable;
    value.retryPolicy.maxAttempts = 3;
    value.retryPolicy.baseDelayMs = 1000;
    value.retryPolicy.maxDelayMs = 4000;
    return value;
}

bool sameCandidate(const UpdateCandidate& left, const UpdateCandidate& right) {
    return left.version == right.version && left.buildId == right.buildId &&
           left.assetUrl == right.assetUrl && left.assetSize == right.assetSize &&
           left.sha256 == right.sha256;
}

UpdateStoreRecord readyRecord(const UpdateCandidate& patch) {
    UpdateStoreRecord record;
    record.snapshot.phase = UpdatePhase::Ready;
    record.snapshot.readyPatch = patch;
    record.snapshot.downloadedBytes = patch.assetSize;
    record.stagedRelativePath = patch.version + "/" + target(patch.version);
    return record;
}

bool hasHeader(const HttpTransferRequestPlan& plan,
               const std::string& name,
               const std::string& value) {
    for (const auto& header : plan.headers) {
        if (header.name == name && header.value == value) return true;
    }
    return false;
}

void test_restored_sidecar_resumes_and_reaches_ready() {
    ScriptedTransport transport;
    auto head = freshHead(206);
    head.contentLength = 2;
    head.contentRange = "bytes 2-3/4";
    transport.attempts.push_back({head, {{3, 4}}});
    FakePersistence persistence;
    persistence.partSize = 2;
    persistence.metadata.status = ResumeMetadataStatus::Present;
    persistence.metadata.metadata.url = candidate("1.2.0").assetUrl;
    persistence.metadata.metadata.expectedSize = 4;
    persistence.metadata.metadata.expectedSha256 = std::string(64, 'a');
    persistence.metadata.metadata.downloadedBytes = 2;
    persistence.metadata.metadata.etag = "etag-v1";
    UpdateStoreRecord record;
    record.snapshot.phase = UpdatePhase::Downloading;
    record.snapshot.candidate = candidate("1.2.0");
    record.snapshot.downloadedBytes = 2;
    NeverCancelled cancellation;
    UpdateRuntime runtime(transport, persistence, config(), record);

    const auto result = runtime.ResumeInterrupted(
        target("1.2.0"), 1024, cancellation);
    check(result.disposition == UpdateRuntimeDisposition::Ready,
          "restored valid sidecar reaches ready");
    check(transport.plans.size() == 1 &&
              hasHeader(transport.plans[0], "Range", "bytes=2-") &&
              hasHeader(transport.plans[0], "If-Range", "etag-v1"),
          "restored sidecar and actual part size produce Range and If-Range");
    check(persistence.activates == 1 && persistence.deactivates == 1,
          "resumed attempt balances transfer lease exactly once");
}

void test_fresh_download_streams_promotes_and_persists_ready() {
    ScriptedTransport transport;
    transport.attempts.push_back({freshHead(), {{1, 2}, {3, 4}}});
    FakePersistence persistence;
    persistence.metadata.status = ResumeMetadataStatus::Missing;
    NeverCancelled cancellation;
    UpdateRuntime runtime(transport, persistence, config());

    const auto result = runtime.DownloadCandidate(
        candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
    check(result.disposition == UpdateRuntimeDisposition::Ready &&
              result.snapshot.phase == UpdatePhase::Ready &&
              result.snapshot.readyPatch.has_value(),
          "fresh download becomes a ready patch");
    check(persistence.activates == 1 && persistence.deactivates == 1 &&
              persistence.bytes.size() == 4,
          "fresh download activates, streams metadata, and deactivates once");
    check(!persistence.cleanup.empty() &&
              persistence.cleanup[0].kind ==
                  DownloadCleanupIntentKind::RenamePartToTarget,
          "checksum success promotes part to final target");
    check(persistence.storePlans >= 1 &&
              runtime.record().snapshot.phase == UpdatePhase::Ready &&
              runtime.record().stagedRelativePath ==
                  "1.2.0/" + target("1.2.0"),
          "ready snapshot and staged identity are persisted");
}

void test_restart_confirmation_is_transactional_and_consume_once() {
    ScriptedTransport transport;
    FakePersistence persistence;
    UpdateRuntime runtime(transport, persistence, config(),
                          readyRecord(candidate("1.2.0")));

    const auto authorized = runtime.ConfirmRestart();
    check(authorized.disposition ==
              UpdateRuntimeDisposition::RestartAuthorized &&
              authorized.snapshot.restartConfirmed,
          "Ready runtime persists one restart authorization");
    const int stores = persistence.storePlans;
    const auto replay = runtime.ConfirmRestart();
    check(replay.disposition == UpdateRuntimeDisposition::NoOp &&
              persistence.storePlans == stores,
          "restart authorization replay is a zero-persistence NoOp");

    FakePersistence failingPersistence;
    failingPersistence.failStoreAt = 0;
    UpdateRuntime retryable(transport, failingPersistence, config(),
                            readyRecord(candidate("1.2.0")));
    const auto failed = retryable.ConfirmRestart();
    check(failed.disposition == UpdateRuntimeDisposition::TerminalError &&
              failed.error == DownloadErrorKind::Disk &&
              !failed.snapshot.restartConfirmed,
          "restart persistence failure stays Disk and retains Ready state");
    failingPersistence.failStoreAt = -1;
    const auto retried = retryable.ConfirmRestart();
    check(retried.disposition ==
              UpdateRuntimeDisposition::RestartAuthorized &&
              retried.snapshot.restartConfirmed,
          "failed restart persistence can be retried transactionally");
}

void test_resumed_200_resets_and_restarts_fresh_without_stale_append() {
    ScriptedTransport transport;
    transport.attempts.push_back({freshHead(), {}});
    transport.attempts.push_back({freshHead(), {{1, 2, 3, 4}}});
    FakePersistence persistence;
    persistence.partSize = 2;
    persistence.metadata.status = ResumeMetadataStatus::Present;
    persistence.metadata.metadata.url = candidate("1.2.0").assetUrl;
    persistence.metadata.metadata.expectedSize = 4;
    persistence.metadata.metadata.expectedSha256 = std::string(64, 'a');
    persistence.metadata.metadata.downloadedBytes = 2;
    persistence.metadata.metadata.etag = "etag-v1";
    UpdateStoreRecord record;
    record.snapshot.phase = UpdatePhase::Downloading;
    record.snapshot.candidate = candidate("1.2.0");
    record.snapshot.downloadedBytes = 2;
    NeverCancelled cancellation;
    UpdateRuntime runtime(transport, persistence, config(), record);

    const auto first = runtime.ResumeInterrupted(
        target("1.2.0"), 1024, cancellation);
    check(first.disposition == UpdateRuntimeDisposition::RetryScheduled &&
              first.retry && first.retry->delayMs == 0 &&
              persistence.bytes.empty(),
          "resume 200 cleans stale bytes and emits an immediate fresh intent");
    const auto second = runtime.RunScheduledRetry(
        *first.retry, 1024, cancellation);
    check(second.disposition == UpdateRuntimeDisposition::Ready &&
              transport.plans.size() == 2 && transport.plans[1].headers.empty(),
          "resume 200 retry is a fresh request with no stale Range header");
}

void test_retry_terminal_hash_disk_and_cancel_classification() {
    NeverCancelled cancellation;
    {
        ScriptedTransport transport;
        transport.attempts.push_back(
            {{}, {}, HttpTransportExecutionError::Network});
        FakePersistence persistence;
        UpdateRuntime runtime(transport, persistence, config());
        const auto result = runtime.DownloadCandidate(
            candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
        check(result.disposition == UpdateRuntimeDisposition::RetryScheduled &&
                  result.retry && result.retry->delayMs == 1000 &&
                  persistence.deactivates == 1,
              "network failure emits capped backoff and releases lease");
    }
    {
        ScriptedTransport transport;
        transport.attempts.push_back(
            {{}, {}, HttpTransportExecutionError::Network});
        FakePersistence persistence;
        persistence.failStoreAfter = 1;
        UpdateRuntime runtime(transport, persistence, config());
        const auto result = runtime.DownloadCandidate(
            candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
        check(result.disposition == UpdateRuntimeDisposition::TerminalError &&
                  !result.retry && persistence.deactivates == 1 &&
                  runtime.flight().activeKey.empty(),
              "retry snapshot persistence failure is terminal, not scheduled");
    }
    {
        ScriptedTransport transport;
        FakePersistence persistence;
        persistence.failStoreAfter = 0;
        UpdateRuntime runtime(transport, persistence, config());
        const auto result = runtime.DownloadCandidate(
            candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
        check(result.disposition == UpdateRuntimeDisposition::TerminalError &&
                  result.snapshot.phase == UpdatePhase::Idle &&
                  transport.executions == 0,
              "failed state persistence does not publish an uncommitted phase");
    }
    {
        ScriptedTransport transport;
        transport.attempts.push_back({freshHead(503), {}});
        FakePersistence persistence;
        UpdateRuntime runtime(transport, persistence, config());
        const auto result = runtime.DownloadCandidate(
            candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
        check(result.disposition == UpdateRuntimeDisposition::RetryScheduled,
              "5xx is retryable through existing policy");
    }
    {
        ScriptedTransport transport;
        transport.attempts.push_back({freshHead(404), {}});
        FakePersistence persistence;
        UpdateRuntime runtime(transport, persistence, config());
        const auto result = runtime.DownloadCandidate(
            candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
        check(result.disposition == UpdateRuntimeDisposition::TerminalError &&
                  !result.retry,
              "4xx is terminal with no hidden retry");
    }
    {
        ScriptedTransport transport;
        transport.attempts.push_back({freshHead(), {{1, 2, 3, 4}}});
        FakePersistence persistence;
        persistence.partHash = std::string(64, 'b');
        UpdateRuntime runtime(transport, persistence, config());
        const auto result = runtime.DownloadCandidate(
            candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
        check(result.disposition == UpdateRuntimeDisposition::TerminalError &&
                  !result.retry && persistence.deactivates == 1,
              "checksum mismatch cleans up and is terminal");
    }
    {
        ScriptedTransport transport;
        FakePersistence persistence;
        UpdateRuntime runtime(transport, persistence, config());
        const auto result = runtime.DownloadCandidate(
            candidate("1.2.0"), target("1.2.0"), 2, cancellation);
        check(result.disposition == UpdateRuntimeDisposition::TerminalError &&
                  transport.executions == 0 && persistence.activates == 0,
              "disk preflight is terminal before transport or lease");
    }
    {
        ScriptedTransport transport;
        transport.attempts.push_back(
            {{}, {}, HttpTransportExecutionError::Cancelled});
        FakePersistence persistence;
        UpdateRuntime runtime(transport, persistence, config());
        const auto result = runtime.DownloadCandidate(
            candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
        check(result.disposition == UpdateRuntimeDisposition::Cancelled &&
                  result.snapshot.phase == UpdatePhase::Idle &&
                  persistence.activates == 1 && persistence.deactivates == 1 &&
                  !result.retry,
              "blocked transport cancellation deactivates and releases once");
    }
    {
        ScriptedTransport transport;
        transport.attempts.push_back(
            {{}, {}, HttpTransportExecutionError::Cancelled});
        FakePersistence persistence;
        persistence.failStoreAt = 1;
        UpdateRuntime runtime(transport, persistence, config());
        const auto result = runtime.DownloadCandidate(
            candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
        check(result.disposition == UpdateRuntimeDisposition::TerminalError &&
                  result.snapshot.phase == UpdatePhase::Error &&
                  persistence.deactivates == 1 &&
                  runtime.flight().activeKey.empty(),
              "cancel persistence failure is explicit terminal state");
    }
    {
        ScriptedTransport transport;
        transport.attempts.push_back({freshHead(), {{1, 2, 3, 4}}});
        FakePersistence persistence;
        LateCancellation cancellationAfterCommit;
        UpdateRuntime runtime(transport, persistence, config());
        const auto result = runtime.DownloadCandidate(
            candidate("1.2.0"), target("1.2.0"), 1024,
            cancellationAfterCommit);
        check(result.disposition == UpdateRuntimeDisposition::Ready &&
                  result.snapshot.phase == UpdatePhase::Ready,
              "completed promotion wins over a cancellation observed afterward");
    }
    {
        ScriptedTransport transport;
        ScriptedAttempt throwing;
        throwing.throws = true;
        transport.attempts.push_back(throwing);
        FakePersistence persistence;
        UpdateRuntime runtime(transport, persistence, config());
        bool escaped = false;
        UpdateRuntimeResult result;
        try {
            result = runtime.DownloadCandidate(
                candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
        } catch (...) {
            escaped = true;
        }
        check(!escaped &&
                  result.disposition == UpdateRuntimeDisposition::RetryScheduled &&
                  persistence.deactivates == 1 && runtime.flight().activeKey.empty(),
              "unexpected transport exception still releases lease and flight");
    }
    {
        ScriptedTransport transport;
        FakePersistence persistence;
        persistence.throwActivate = true;
        UpdateRuntime runtime(transport, persistence, config());
        bool escaped = false;
        UpdateRuntimeResult result;
        try {
            result = runtime.DownloadCandidate(
                candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
        } catch (...) {
            escaped = true;
        }
        check(!escaped &&
                  result.disposition == UpdateRuntimeDisposition::TerminalError &&
                  persistence.deactivates == 1 &&
                  runtime.flight().activeKey.empty(),
              "throwing activation still deactivates and releases ownership");
    }
    {
        ScriptedTransport transport;
        transport.attempts.push_back(
            {{}, {}, HttpTransportExecutionError::Network});
        FakePersistence persistence;
        persistence.throwDeactivate = true;
        UpdateRuntime runtime(transport, persistence, config());
        bool escaped = false;
        UpdateRuntimeResult result;
        try {
            result = runtime.DownloadCandidate(
                candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
        } catch (...) {
            escaped = true;
        }
        check(!escaped &&
                  result.disposition == UpdateRuntimeDisposition::TerminalError &&
                  runtime.flight().activeKey.empty(),
              "throwing deactivation cannot prevent flight release");
    }
    {
        ScriptedTransport transport;
        auto protocol = freshHead(206);
        protocol.contentRange = "bytes 0-3/4";
        transport.attempts.push_back({protocol, {}});
        FakePersistence persistence;
        UpdateRuntime runtime(transport, persistence, config());
        const auto result = runtime.DownloadCandidate(
            candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
        check(result.disposition == UpdateRuntimeDisposition::TerminalError &&
                  !result.retry && persistence.deactivates == 1 &&
                  runtime.flight().activeKey.empty(),
              "fresh protocol mismatch is terminal and releases ownership");
    }
    {
        ScriptedTransport transport;
        transport.attempts.push_back({freshHead(), {{1, 2}}});
        FakePersistence persistence;
        persistence.failTransferChunk = true;
        UpdateRuntime runtime(transport, persistence, config());
        const auto result = runtime.DownloadCandidate(
            candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
        check(result.disposition == UpdateRuntimeDisposition::TerminalError &&
                  persistence.deactivates == 1 &&
                  runtime.flight().activeKey.empty(),
              "stream persistence failure releases lease and flight");
    }
}

void test_retry_cap_and_restart_restore_are_deterministic() {
    NeverCancelled cancellation;
    ScriptedTransport transport;
    for (int index = 0; index < 3; ++index) {
        transport.attempts.push_back(
            {{}, {}, HttpTransportExecutionError::Network});
    }
    FakePersistence persistence;
    UpdateStoreRecord record;
    record.snapshot.phase = UpdatePhase::Downloading;
    record.snapshot.candidate = candidate("1.2.0");
    UpdateRuntime runtime(transport, persistence, config(), record);
    check(persistence.storePlans == 0,
          "restore construction emits no duplicate persistence or install effect");

    const auto first = runtime.ResumeInterrupted(
        target("1.2.0"), 1024, cancellation);
    check(first.retry && first.retry->delayMs == 1000 &&
              persistence.storePlans == 1,
          "interrupted restore reissues one attempt and one retry snapshot");
    const auto second = runtime.RunScheduledRetry(
        *first.retry, 1024, cancellation);
    check(second.retry && second.retry->delayMs == 2000,
          "second network failure emits deterministic capped backoff");
    const auto third = runtime.RunScheduledRetry(
        *second.retry, 1024, cancellation);
    check(third.disposition == UpdateRuntimeDisposition::TerminalError &&
              !third.retry && persistence.activates == 3 &&
              persistence.deactivates == 3 && runtime.flight().activeKey.empty(),
          "retry cap terminates and balances every attempt ownership");
}

void test_pre_cancelled_entries_skip_all_trusted_observation_and_transport() {
    PreCancelled cancellation;
    {
        ScriptedTransport transport;
        FakePersistence persistence;
        UpdateRuntime runtime(transport, persistence, config());
        const auto result = runtime.DownloadCandidate(
            candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
        check(result.disposition == UpdateRuntimeDisposition::Cancelled &&
                  result.snapshot.phase == UpdatePhase::Idle &&
                  persistence.metadataObservations == 0 &&
                  persistence.stagedObservations == 0 &&
                  persistence.activates == 0 && transport.executions == 0 &&
                  runtime.flight().activeKey.empty(),
              "pre-cancelled candidate skips trusted observations and transport");
    }
    {
        ScriptedTransport transport;
        FakePersistence persistence;
        UpdateStoreRecord record;
        record.snapshot.phase = UpdatePhase::Downloading;
        record.snapshot.candidate = candidate("1.2.0");
        UpdateRuntime runtime(transport, persistence, config(), record);
        const auto result = runtime.ResumeInterrupted(
            target("1.2.0"), 1024, cancellation);
        check(result.disposition == UpdateRuntimeDisposition::Cancelled &&
                  result.snapshot.phase == UpdatePhase::Idle &&
                  persistence.metadataObservations == 0 &&
                  persistence.stagedObservations == 0 &&
                  persistence.activates == 0 && transport.executions == 0 &&
                  runtime.flight().activeKey.empty(),
              "pre-cancelled restore skips trusted observations and transport");
    }
    {
        ScriptedTransport transport;
        FakePersistence persistence;
        persistence.failStoreAt = 0;
        UpdateRuntime runtime(transport, persistence, config());
        const auto result = runtime.DownloadCandidate(
            candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
        check(result.disposition == UpdateRuntimeDisposition::Cancelled &&
                  result.error == DownloadErrorKind::Aborted &&
                  result.snapshot.phase == UpdatePhase::Idle &&
                  persistence.storePlans == 0 &&
                  persistence.metadataObservations == 0 &&
                  persistence.stagedObservations == 0 &&
                  transport.executions == 0,
              "pre-cancelled admission cannot reach a configured persistence failure");
    }
}

void test_stale_retry_and_superseding_failure_cannot_replace_ready_patch() {
    NeverCancelled cancellation;
    ScriptedTransport transport;
    transport.attempts.push_back(
        {{}, {}, HttpTransportExecutionError::Network});
    transport.attempts.push_back({freshHead(), {{1, 2, 3, 4}}});
    FakePersistence persistence;
    UpdateRuntime runtime(transport, persistence, config());
    const auto oldAttempt = runtime.DownloadCandidate(
        candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
    check(oldAttempt.retry.has_value(), "first generation emits retry token");
    const auto beforeForged = transport.executions;
    auto forgedAttempt = *oldAttempt.retry;
    forgedAttempt.completedAttempts = 1;
    const auto forgedAttemptResult = runtime.RunScheduledRetry(
        forgedAttempt, 1024, cancellation);
    check(forgedAttemptResult.disposition == UpdateRuntimeDisposition::NoOp &&
              transport.executions == beforeForged,
          "scheduled retry cannot roll back its completed attempt count");
    auto forged = *oldAttempt.retry;
    forged.request.relativeTarget = "forged.zip";
    const auto forgedResult = runtime.RunScheduledRetry(
        forged, 1024, cancellation);
    check(forgedResult.disposition == UpdateRuntimeDisposition::NoOp &&
              transport.executions == beforeForged,
          "scheduled retry cannot forge a different staging identity");
    const auto newer = runtime.DownloadCandidate(
        candidate("1.3.0"), target("1.3.0"), 1024, cancellation);
    check(newer.disposition == UpdateRuntimeDisposition::Ready,
          "newer generation can supersede pending retry");
    const auto executions = transport.executions;
    const auto stale = runtime.RunScheduledRetry(
        *oldAttempt.retry, 1024, cancellation);
    check(stale.disposition == UpdateRuntimeDisposition::NoOp &&
              transport.executions == executions,
          "stale scheduled retry cannot mutate or revoke newer flight");

    ScriptedTransport failingTransport;
    failingTransport.attempts.push_back({freshHead(404), {}});
    FakePersistence secondPersistence;
    UpdateStoreRecord readyRecord = runtime.record();
    UpdateRuntime superseding(
        failingTransport, secondPersistence, config(), readyRecord);
    const auto failed = superseding.DownloadCandidate(
        candidate("1.4.0"), target("1.4.0"), 1024, cancellation);
    check(failed.disposition == UpdateRuntimeDisposition::TerminalError &&
              failed.snapshot.phase == UpdatePhase::Ready &&
              failed.snapshot.readyPatch &&
              failed.snapshot.readyPatch->version == "1.3.0" &&
              superseding.record().stagedRelativePath ==
                  "1.3.0/" + target("1.3.0"),
          "failed superseding candidate preserves prior ready patch");

    ScriptedTransport readyWriteTransport;
    readyWriteTransport.attempts.push_back({freshHead(), {{1, 2, 3, 4}}});
    FakePersistence readyWritePersistence;
    readyWritePersistence.failStoreAt = 2;
    UpdateRuntime readyWriteFailure(
        readyWriteTransport, readyWritePersistence, config(), readyRecord);
    const auto notPersistedReady = readyWriteFailure.DownloadCandidate(
        candidate("1.4.0"), target("1.4.0"), 1024, cancellation);
    check(notPersistedReady.disposition ==
              UpdateRuntimeDisposition::TerminalError &&
              notPersistedReady.snapshot.phase == UpdatePhase::Ready &&
              notPersistedReady.snapshot.readyPatch &&
              notPersistedReady.snapshot.readyPatch->version == "1.3.0" &&
              readyWriteFailure.record().stagedRelativePath ==
                  "1.3.0/" + target("1.3.0"),
          "failed ready snapshot write retains prior patch path atomically");
}

void test_explicit_check_lifecycle_has_no_duplicate_check_transition() {
    ScriptedTransport transport;
    transport.attempts.push_back({freshHead(), {{1, 2, 3, 4}}});
    FakePersistence persistence;
    NeverCancelled cancellation;
    UpdateRuntime runtime(transport, persistence, config());
    const auto patch = candidate("1.2.0");

    const auto begun = runtime.BeginCheck();
    check(begun.disposition == UpdateRuntimeDisposition::CheckStarted &&
              begun.checkToken.has_value() &&
              begun.snapshot.phase == UpdatePhase::Checking &&
              persistence.storePlans == 1,
          "begin check owns one persisted active generation");
    const auto flightBeforeCompletion = runtime.flight();
    const auto completed = runtime.CompleteCheckedCandidate(
        *begun.checkToken, patch);
    check(completed.disposition == UpdateRuntimeDisposition::UpdateAvailable &&
              completed.snapshot.phase == UpdatePhase::Available &&
              completed.snapshot.candidate.has_value() &&
              sameCandidate(*completed.snapshot.candidate, patch) &&
              completed.candidateHandoff.has_value() &&
              persistence.storePlans == 2,
          "candidate completion persists once without a duplicate check request");
    check(runtime.flight().generation == flightBeforeCompletion.generation &&
              runtime.flight().activeKey == flightBeforeCompletion.activeKey,
          "check completion does not revoke or advance download flight ownership");

    const auto downloaded = runtime.DownloadCandidate(
        *completed.candidateHandoff, target(patch.version), 1024,
        cancellation);
    check(downloaded.disposition == UpdateRuntimeDisposition::Ready &&
              downloaded.snapshot.readyPatch.has_value() &&
              sameCandidate(*downloaded.snapshot.readyPatch, patch) &&
              persistence.storePlans == 5,
          "download path accepts the checked candidate without replaying check transitions");
}

void test_no_update_completion_from_checking_and_superseding() {
    {
        ScriptedTransport transport;
        FakePersistence persistence;
        UpdateRuntime runtime(transport, persistence, config());
        const auto begun = runtime.BeginCheck();
        const auto noUpdate = runtime.CompleteNoUpdate(*begun.checkToken);
        check(noUpdate.disposition == UpdateRuntimeDisposition::NoUpdate &&
                  noUpdate.snapshot.phase == UpdatePhase::Idle &&
                  persistence.storePlans == 2,
              "ordinary no-update completion returns to idle and persists once");
    }
    {
        ScriptedTransport transport;
        FakePersistence persistence;
        const auto oldPatch = candidate("1.2.0");
        const auto original = readyRecord(oldPatch);
        UpdateRuntime runtime(transport, persistence, config(), original);
        const auto begun = runtime.BeginCheck();
        check(begun.snapshot.phase == UpdatePhase::Superseding,
              "ready patch begins a superseding check");
        const auto noUpdate = runtime.CompleteNoUpdate(*begun.checkToken);
        check(noUpdate.disposition == UpdateRuntimeDisposition::NoUpdate &&
                  noUpdate.snapshot.phase == UpdatePhase::Ready &&
                  noUpdate.snapshot.readyPatch.has_value() &&
                  sameCandidate(*noUpdate.snapshot.readyPatch, oldPatch) &&
                  noUpdate.snapshot.downloadedBytes ==
                      original.snapshot.downloadedBytes &&
                  runtime.record().stagedRelativePath ==
                      original.stagedRelativePath,
              "superseding no-update completion preserves exact ready metadata");
    }
}

void test_superseding_candidate_waits_for_download_admission() {
    ScriptedTransport transport;
    transport.attempts.push_back({freshHead(), {{1, 2, 3, 4}}});
    transport.attempts.push_back({freshHead(), {{1, 2, 3, 4}}});
    FakePersistence persistence;
    NeverCancelled cancellation;
    UpdateRuntime runtime(transport, persistence, config());
    const auto oldPatch = candidate("1.2.0");
    const auto newerPatch = candidate("1.3.0");

    const auto oldDownload = runtime.DownloadCandidate(
        oldPatch, target(oldPatch.version), 1024, cancellation);
    check(oldDownload.disposition == UpdateRuntimeDisposition::Ready,
          "initial compatibility download establishes a ready patch");
    const auto oldStagedPath = runtime.record().stagedRelativePath;
    const auto flightBeforeCheck = runtime.flight();

    const auto begun = runtime.BeginCheck();
    const auto checked = runtime.CompleteCheckedCandidate(
        *begun.checkToken, newerPatch);
    check(checked.disposition == UpdateRuntimeDisposition::UpdateAvailable &&
              checked.snapshot.phase == UpdatePhase::Ready &&
              checked.snapshot.readyPatch.has_value() &&
              sameCandidate(*checked.snapshot.readyPatch, oldPatch) &&
              !checked.snapshot.candidate &&
              checked.candidateHandoff.has_value() &&
              runtime.record().stagedRelativePath == oldStagedPath,
          "superseding completion keeps the prior ready patch until download admission");
    check(runtime.flight().generation == flightBeforeCheck.generation &&
              runtime.flight().activeKey == flightBeforeCheck.activeKey,
          "superseding completion leaves an established flight generation untouched");

    PreCancelled preCancelled;
    const auto storesBeforeCancellation = persistence.storePlans;
    const auto cancelled = runtime.DownloadCandidate(
        *checked.candidateHandoff, target(newerPatch.version), 1024,
        preCancelled);
    check(cancelled.disposition == UpdateRuntimeDisposition::Cancelled &&
              cancelled.error == DownloadErrorKind::Aborted &&
              cancelled.snapshot.phase == UpdatePhase::Ready &&
              cancelled.snapshot.readyPatch.has_value() &&
              sameCandidate(*cancelled.snapshot.readyPatch, oldPatch) &&
              runtime.record().stagedRelativePath == oldStagedPath &&
              persistence.storePlans == storesBeforeCancellation,
          "pre-cancelled superseding admission preserves ready state without persistence churn");

    const auto newerDownload = runtime.DownloadCandidate(
        *checked.candidateHandoff, target(newerPatch.version), 1024,
        cancellation);
    check(newerDownload.disposition == UpdateRuntimeDisposition::Ready &&
              newerDownload.snapshot.readyPatch.has_value() &&
              sameCandidate(*newerDownload.snapshot.readyPatch, newerPatch) &&
              runtime.record().stagedRelativePath ==
                  newerPatch.version + "/" + target(newerPatch.version),
          "next download path atomically accepts and promotes the newer patch");

    const auto flightAfterDownload = runtime.flight();
    const auto staleFailure = runtime.FailActiveCheck(
        *begun.checkToken, DownloadErrorKind::Network,
        "late stale failure");
    check(staleFailure.disposition == UpdateRuntimeDisposition::NoOp &&
              runtime.snapshot().phase == UpdatePhase::Ready &&
              runtime.flight().generation == flightAfterDownload.generation &&
              runtime.flight().activeKey == flightAfterDownload.activeKey,
          "stale check failure cannot alter the newer download flight or ready patch");
}

void test_old_handoff_and_raw_candidate_cannot_replace_new_validation() {
    ScriptedTransport transport;
    transport.attempts.push_back({freshHead(), {{1, 2, 3, 4}}});
    FakePersistence persistence;
    NeverCancelled cancellation;
    UpdateRuntime runtime(transport, persistence, config());
    const auto oldCandidate = candidate("1.2.0");
    const auto newerCandidate = candidate("1.3.0");

    const auto firstBegin = runtime.BeginCheck();
    const auto firstCompletion = runtime.CompleteCheckedCandidate(
        *firstBegin.checkToken, oldCandidate);
    check(firstCompletion.candidateHandoff.has_value(),
          "first checked generation returns an opaque candidate handoff");
    const auto oldHandoff = *firstCompletion.candidateHandoff;

    const auto secondBegin = runtime.BeginCheck();
    const auto secondCompletion = runtime.CompleteCheckedCandidate(
        *secondBegin.checkToken, newerCandidate);
    check(secondCompletion.candidateHandoff.has_value(),
          "second checked generation replaces pending handoff ownership");
    const auto storesBeforeStale = persistence.storePlans;
    const auto executionsBeforeStale = transport.executions;
    const auto flightBeforeStale = runtime.flight();

    const auto stale = runtime.DownloadCandidate(
        oldHandoff, target(oldCandidate.version), 1024, cancellation);
    check(stale.disposition == UpdateRuntimeDisposition::NoOp &&
              stale.snapshot.phase == UpdatePhase::Available &&
              stale.snapshot.candidate.has_value() &&
              sameCandidate(*stale.snapshot.candidate, newerCandidate) &&
              persistence.storePlans == storesBeforeStale &&
              transport.executions == executionsBeforeStale &&
              runtime.flight().generation == flightBeforeStale.generation &&
              runtime.flight().activeKey == flightBeforeStale.activeKey,
          "generation-one handoff is inert after generation-two completion");

    const auto rawDifferent = runtime.DownloadCandidate(
        oldCandidate, target(oldCandidate.version), 1024, cancellation);
    check(rawDifferent.disposition == UpdateRuntimeDisposition::NoOp &&
              rawDifferent.snapshot.phase == UpdatePhase::Available &&
              rawDifferent.snapshot.candidate.has_value() &&
              sameCandidate(*rawDifferent.snapshot.candidate,
                            newerCandidate) &&
              persistence.storePlans == storesBeforeStale &&
              transport.executions == executionsBeforeStale,
          "raw compatibility admission cannot overwrite a validated available candidate");

    const auto admitted = runtime.DownloadCandidate(
        *secondCompletion.candidateHandoff,
        target(newerCandidate.version), 1024, cancellation);
    check(admitted.disposition == UpdateRuntimeDisposition::Ready &&
              admitted.snapshot.readyPatch.has_value() &&
              sameCandidate(*admitted.snapshot.readyPatch, newerCandidate),
          "current generation handoff remains admissible after stale attempts");
}

void test_copied_handoff_is_consumed_once() {
    ScriptedTransport transport;
    transport.attempts.push_back({freshHead(), {{1, 2, 3, 4}}});
    FakePersistence persistence;
    NeverCancelled cancellation;
    UpdateRuntime runtime(transport, persistence, config());
    const auto patch = candidate("1.2.0");

    const auto begun = runtime.BeginCheck();
    const auto completed = runtime.CompleteCheckedCandidate(
        *begun.checkToken, patch);
    const auto admittedHandoff = *completed.candidateHandoff;
    const auto copiedHandoff = admittedHandoff;
    const auto admitted = runtime.DownloadCandidate(
        admittedHandoff, target(patch.version), 1024, cancellation);
    check(admitted.disposition == UpdateRuntimeDisposition::Ready,
          "first copied handoff admission succeeds");

    const auto storesAfterAdmission = persistence.storePlans;
    const auto executionsAfterAdmission = transport.executions;
    const auto flightAfterAdmission = runtime.flight();
    const auto replayed = runtime.DownloadCandidate(
        copiedHandoff, target(patch.version), 1024, cancellation);
    check(replayed.disposition == UpdateRuntimeDisposition::NoOp &&
              replayed.snapshot.phase == UpdatePhase::Ready &&
              persistence.storePlans == storesAfterAdmission &&
              transport.executions == executionsAfterAdmission &&
              runtime.flight().generation == flightAfterAdmission.generation &&
              runtime.flight().activeKey == flightAfterAdmission.activeKey,
          "copied handoff replay is inert after first successful admission");
}

void test_runtime_capability_rejects_foreign_token_and_handoff() {
    ScriptedTransport transportA;
    ScriptedTransport transportB;
    FakePersistence persistenceA;
    FakePersistence persistenceB;
    NeverCancelled cancellation;
    UpdateRuntime runtimeA(transportA, persistenceA, config());
    UpdateRuntime runtimeB(transportB, persistenceB, config());
    const auto candidateA = candidate("1.2.0");
    const auto candidateB = candidate("1.3.0");

    const auto begunA = runtimeA.BeginCheck();
    const auto begunB = runtimeB.BeginCheck();
    const auto storesBeforeForeignToken = persistenceB.storePlans;
    const auto foreignCompletion = runtimeB.CompleteCheckedCandidate(
        *begunA.checkToken, candidateA);
    check(foreignCompletion.disposition == UpdateRuntimeDisposition::NoOp &&
              runtimeB.snapshot().phase == UpdatePhase::Checking &&
              persistenceB.storePlans == storesBeforeForeignToken,
          "equal numeric generations cannot cross runtime token ownership");

    const auto completedA = runtimeA.CompleteCheckedCandidate(
        *begunA.checkToken, candidateA);
    const auto completedB = runtimeB.CompleteCheckedCandidate(
        *begunB.checkToken, candidateB);
    const auto storesBeforeForeignHandoff = persistenceB.storePlans;
    const auto flightBeforeForeignHandoff = runtimeB.flight();
    const auto foreignAdmission = runtimeB.DownloadCandidate(
        *completedA.candidateHandoff, target(candidateA.version), 1024,
        cancellation);
    check(foreignAdmission.disposition == UpdateRuntimeDisposition::NoOp &&
              runtimeB.snapshot().phase == UpdatePhase::Available &&
              runtimeB.snapshot().candidate.has_value() &&
              sameCandidate(*runtimeB.snapshot().candidate, candidateB) &&
              persistenceB.storePlans == storesBeforeForeignHandoff &&
              transportB.executions == 0 &&
              runtimeB.flight().generation ==
                  flightBeforeForeignHandoff.generation &&
              runtimeB.flight().activeKey ==
                  flightBeforeForeignHandoff.activeKey &&
              completedB.candidateHandoff.has_value(),
          "handoff capability from runtime A is inert in runtime B");
}

void test_handoff_survives_pre_admission_cancellation_and_persistence_failure() {
    ScriptedTransport transport;
    transport.attempts.push_back({freshHead(), {{1, 2, 3, 4}}});
    FakePersistence persistence;
    UpdateRuntime runtime(transport, persistence, config());
    const auto patch = candidate("1.2.0");
    const auto begun = runtime.BeginCheck();
    const auto completed = runtime.CompleteCheckedCandidate(
        *begun.checkToken, patch);
    const auto handoff = *completed.candidateHandoff;

    PreCancelled preCancelled;
    const auto storesBeforeCancellation = persistence.storePlans;
    const auto cancelled = runtime.DownloadCandidate(
        handoff, target(patch.version), 1024, preCancelled);
    check(cancelled.disposition == UpdateRuntimeDisposition::Cancelled &&
              cancelled.error == DownloadErrorKind::Aborted &&
              cancelled.snapshot.phase == UpdatePhase::Available &&
              persistence.storePlans == storesBeforeCancellation &&
              transport.executions == 0,
          "pre-admission cancellation retains the pending handoff");

    NeverCancelled cancellation;
    persistence.failStoreAt = persistence.storePlans;
    const auto flightBeforeFailure = runtime.flight();
    const auto persistenceFailure = runtime.DownloadCandidate(
        handoff, target(patch.version), 1024, cancellation);
    check(persistenceFailure.disposition ==
                  UpdateRuntimeDisposition::TerminalError &&
              persistenceFailure.error == DownloadErrorKind::Disk &&
              persistenceFailure.snapshot.phase == UpdatePhase::Available &&
              transport.executions == 0 &&
              runtime.flight().generation == flightBeforeFailure.generation &&
              runtime.flight().activeKey == flightBeforeFailure.activeKey,
          "handoff admission persistence failure is Disk and retains state");

    persistence.failStoreAt = -1;
    const auto retried = runtime.DownloadCandidate(
        handoff, target(patch.version), 1024, cancellation);
    check(retried.disposition == UpdateRuntimeDisposition::Ready &&
              retried.snapshot.readyPatch.has_value() &&
              sameCandidate(*retried.snapshot.readyPatch, patch),
          "pending handoff can retry after cancellation and persistence failure");
}

void test_active_check_failure_preserves_explicit_classification() {
    struct FailureCase {
        DownloadErrorKind error;
        const char* detail;
    };
    const FailureCase cases[] = {
        {DownloadErrorKind::Network, "offline"},
        {DownloadErrorKind::InvalidArgument,
         "signature policy rejected manifest"},
        {DownloadErrorKind::Http4xx, "manifest request rejected"},
        {DownloadErrorKind::Http5xx, "release service unavailable"},
    };

    for (const auto& failure : cases) {
        ScriptedTransport transport;
        FakePersistence persistence;
        UpdateRuntime runtime(transport, persistence, config());
        const auto begun = runtime.BeginCheck();
        const auto failed = runtime.FailActiveCheck(
            *begun.checkToken, failure.error, failure.detail);
        check(failed.disposition == UpdateRuntimeDisposition::TerminalError &&
                  failed.error == failure.error &&
                  failed.snapshot.phase == UpdatePhase::Error &&
                  failed.snapshot.lastError == failure.detail,
              "check failure retains its explicit classification");
    }
}

void test_active_check_failure_and_stale_results_are_generation_bound() {
    ScriptedTransport transport;
    FakePersistence persistence;
    const auto oldPatch = candidate("1.2.0");
    const auto original = readyRecord(oldPatch);
    UpdateRuntime runtime(transport, persistence, config(), original);

    const auto first = runtime.BeginCheck();
    const auto second = runtime.BeginCheck();
    check(first.checkToken && second.checkToken,
          "restarted check returns a new opaque token");
    const auto storesBeforeStale = persistence.storePlans;
    const auto staleCompletion = runtime.CompleteCheckedCandidate(
        *first.checkToken, candidate("1.3.0"));
    const auto staleFailure = runtime.FailActiveCheck(
        *first.checkToken, DownloadErrorKind::Network,
        "stale offline result");
    check(staleCompletion.disposition == UpdateRuntimeDisposition::NoOp &&
              staleFailure.disposition == UpdateRuntimeDisposition::NoOp &&
              runtime.snapshot().phase == UpdatePhase::Superseding &&
              runtime.snapshot().lastError.empty() &&
              persistence.storePlans == storesBeforeStale,
          "stale completion and failure cannot mutate a newer active check");

    const auto flightBeforeFailure = runtime.flight();
    const auto failed = runtime.FailActiveCheck(
        *second.checkToken, DownloadErrorKind::Network,
        "manifest unavailable");
    check(failed.disposition == UpdateRuntimeDisposition::Ready &&
              failed.error == DownloadErrorKind::Network &&
              failed.detail == "manifest unavailable" &&
              failed.snapshot.phase == UpdatePhase::Ready &&
              failed.snapshot.readyPatch.has_value() &&
              sameCandidate(*failed.snapshot.readyPatch, oldPatch) &&
              failed.snapshot.lastError == "manifest unavailable" &&
              runtime.record().stagedRelativePath == original.stagedRelativePath,
          "active check failure persists diagnostics and preserves ready patch");
    check(runtime.flight().generation == flightBeforeFailure.generation &&
              runtime.flight().activeKey == flightBeforeFailure.activeKey,
          "active check failure does not disturb download flight ownership");
}

void test_check_cancellation_and_persistence_failure_do_not_churn_state() {
    {
        ScriptedTransport transport;
        FakePersistence persistence;
        UpdateRuntime runtime(transport, persistence, config());
        const auto begun = runtime.BeginCheck();
        persistence.failStoreAfter = persistence.storePlans;
        const auto failedCancel = runtime.CancelActiveCheck(
            *begun.checkToken, "update check cancelled");
        check(failedCancel.disposition ==
                  UpdateRuntimeDisposition::TerminalError &&
                  failedCancel.error == DownloadErrorKind::Disk &&
                  failedCancel.snapshot.phase == UpdatePhase::Checking,
              "check cancellation persistence failure retains active token");
        persistence.failStoreAfter = -1;
        const auto retried = runtime.CancelActiveCheck(
            *begun.checkToken, "update check cancelled");
        check(retried.disposition == UpdateRuntimeDisposition::Cancelled &&
                  retried.error == DownloadErrorKind::Aborted &&
                  retried.detail == "update check cancelled" &&
                  retried.snapshot.phase == UpdatePhase::Idle,
              "token-bound check cancellation retries to idle");
    }
    {
        ScriptedTransport transport;
        FakePersistence persistence;
        PreCancelled cancellation;
        UpdateRuntime runtime(transport, persistence, config());
        const auto result = runtime.DownloadCandidate(
            candidate("1.2.0"), target("1.2.0"), 1024, cancellation);
        check(result.disposition == UpdateRuntimeDisposition::Cancelled &&
                  result.error == DownloadErrorKind::Aborted &&
                  result.snapshot.phase == UpdatePhase::Idle &&
                  persistence.storePlans == 0 && transport.executions == 0,
              "pre-cancelled download admission is explicit and causes no state churn");
    }
    {
        ScriptedTransport transport;
        FakePersistence persistence;
        persistence.failStoreAfter = 0;
        UpdateRuntime runtime(transport, persistence, config());
        const auto failedBegin = runtime.BeginCheck();
        check(failedBegin.disposition == UpdateRuntimeDisposition::TerminalError &&
                  failedBegin.error == DownloadErrorKind::Disk &&
                  !failedBegin.checkToken &&
                  failedBegin.snapshot.phase == UpdatePhase::Idle,
              "begin-check persistence failure is Disk terminal without publishing checking");
    }
    {
        ScriptedTransport transport;
        FakePersistence persistence;
        UpdateRuntime runtime(transport, persistence, config());
        const auto begun = runtime.BeginCheck();
        persistence.failStoreAfter = persistence.storePlans;
        const auto failedNoUpdate = runtime.CompleteNoUpdate(*begun.checkToken);
        check(failedNoUpdate.disposition ==
                  UpdateRuntimeDisposition::TerminalError &&
                  failedNoUpdate.error == DownloadErrorKind::Disk &&
                  failedNoUpdate.snapshot.phase == UpdatePhase::Checking,
              "no-update persistence failure stays checking and is not reported as no update");
        persistence.failStoreAfter = -1;
        const auto retried = runtime.CompleteNoUpdate(*begun.checkToken);
        check(retried.disposition == UpdateRuntimeDisposition::NoUpdate &&
                  retried.snapshot.phase == UpdatePhase::Idle,
              "failed completion retains active generation for an explicit retry");
    }
    {
        ScriptedTransport transport;
        FakePersistence persistence;
        const auto oldPatch = candidate("1.2.0");
        const auto original = readyRecord(oldPatch);
        UpdateRuntime runtime(transport, persistence, config(), original);
        const auto begun = runtime.BeginCheck();
        persistence.failStoreAfter = persistence.storePlans;
        const auto failedFailure = runtime.FailActiveCheck(
            *begun.checkToken, DownloadErrorKind::InvalidArgument,
            "invalid signature");
        check(failedFailure.disposition ==
                  UpdateRuntimeDisposition::TerminalError &&
                  failedFailure.error == DownloadErrorKind::Disk &&
                  failedFailure.snapshot.phase == UpdatePhase::Superseding &&
                  failedFailure.snapshot.readyPatch.has_value() &&
                  sameCandidate(*failedFailure.snapshot.readyPatch, oldPatch) &&
                  failedFailure.snapshot.lastError.empty(),
              "failure persistence error is Disk without publishing supplied diagnostic");
        persistence.failStoreAfter = -1;
        const auto retried = runtime.FailActiveCheck(
            *begun.checkToken, DownloadErrorKind::InvalidArgument,
            "invalid signature");
        check(retried.disposition == UpdateRuntimeDisposition::Ready &&
                  retried.error == DownloadErrorKind::InvalidArgument &&
                  retried.snapshot.lastError == "invalid signature",
              "failed failure persistence retains the active generation for retry");
    }
}

}  // namespace

int main() {
    test_restored_sidecar_resumes_and_reaches_ready();
    test_fresh_download_streams_promotes_and_persists_ready();
    test_restart_confirmation_is_transactional_and_consume_once();
    test_resumed_200_resets_and_restarts_fresh_without_stale_append();
    test_retry_terminal_hash_disk_and_cancel_classification();
    test_retry_cap_and_restart_restore_are_deterministic();
    test_pre_cancelled_entries_skip_all_trusted_observation_and_transport();
    test_stale_retry_and_superseding_failure_cannot_replace_ready_patch();
    test_explicit_check_lifecycle_has_no_duplicate_check_transition();
    test_no_update_completion_from_checking_and_superseding();
    test_superseding_candidate_waits_for_download_admission();
    test_old_handoff_and_raw_candidate_cannot_replace_new_validation();
    test_copied_handoff_is_consumed_once();
    test_runtime_capability_rejects_foreign_token_and_handoff();
    test_handoff_survives_pre_admission_cancellation_and_persistence_failure();
    test_active_check_failure_preserves_explicit_classification();
    test_active_check_failure_and_stale_results_are_generation_bound();
    test_check_cancellation_and_persistence_failure_do_not_churn_state();
    return failures == 0 ? 0 : 1;
}
