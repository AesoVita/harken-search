#include "platform/update_http_transport_test_hooks.h"
#include "update/UpdateHttpTransport.h"

#include <cstdio>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include <thread>

namespace {

using namespace aris::update;
using Platform::UpdateHttp::Testing::NativeHttpApi;
using Platform::UpdateHttp::Testing::NativeReadResult;
using Platform::UpdateHttp::Testing::NativeReadStatus;

int failures = 0;
void check(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

struct FakeApi final : NativeHttpApi {
    bool Open(const std::string& urlValue,
              const std::string& headersValue,
              std::uint32_t&) override {
        ++openCalls;
        url = urlValue;
        urls.push_back(urlValue);
        headers = headersValue;
        return openSucceeds;
    }
    bool QueryResponseHead(HttpResponseHead& value,
                           std::uint32_t&) override {
        if (!heads.empty()) {
            value = heads.front();
            heads.erase(heads.begin());
        } else {
            value = head;
        }
        return querySucceeds;
    }
    NativeReadResult Read() override {
        ++readCalls;
        if (blockRead) {
            std::unique_lock<std::mutex> lock(blockMutex);
            readEntered = true;
            blockCv.notify_all();
            blockCv.wait(lock, [this] { return interrupted; });
            return {NativeReadStatus::Error, {}, 995};
        }
        if (!reads.empty()) {
            auto value = std::move(reads.front());
            reads.erase(reads.begin());
            return value;
        }
        if (chunks.empty()) return {NativeReadStatus::Eof, {}, 0};
        auto value = std::move(chunks.front());
        chunks.erase(chunks.begin());
        return {NativeReadStatus::Chunk, std::move(value), 0};
    }
    void Interrupt() override {
        ++interruptCalls;
        {
            std::lock_guard<std::mutex> lock(blockMutex);
            interrupted = true;
        }
        blockCv.notify_all();
    }
    void Close() override { ++closeCalls; }

    bool openSucceeds = true;
    bool querySucceeds = true;
    int openCalls = 0;
    int readCalls = 0;
    int closeCalls = 0;
    int interruptCalls = 0;
    std::string url;
    std::string headers;
    std::vector<std::string> urls;
    HttpResponseHead head;
    std::vector<HttpResponseHead> heads;
    std::vector<NativeReadResult> reads;
    std::vector<std::vector<std::uint8_t>> chunks;
    bool blockRead = false;
    bool readEntered = false;
    bool interrupted = false;
    std::mutex blockMutex;
    std::condition_variable blockCv;
};

struct Cancellation final : HttpTransportCancellation {
    struct SharedState {
        std::mutex mutex;
        std::condition_variable idle;
        std::function<void()> handler;
        std::size_t inFlight = 0;
    };
    struct Registration final : InterruptRegistration {
        explicit Registration(std::shared_ptr<SharedState> shared)
            : shared_(std::move(shared)) {}
        ~Registration() override {
            std::unique_lock<std::mutex> lock(shared_->mutex);
            shared_->handler = {};
            shared_->idle.wait(lock, [this] { return shared_->inFlight == 0; });
        }
        std::shared_ptr<SharedState> shared_;
    };

    bool IsCancellationRequested() const override {
        if (externallyCancelled.load()) return true;
        if (calls++ < cancelAfterChecks) return false;
        Cancel();
        return true;
    }
    std::unique_ptr<InterruptRegistration> RegisterInterrupt(
        std::function<void()> handler) const override {
        std::lock_guard<std::mutex> lock(shared->mutex);
        shared->handler = std::move(handler);
        return std::make_unique<Registration>(shared);
    }
    void Cancel() const {
        externallyCancelled.store(true);
        std::function<void()> handler;
        {
            std::lock_guard<std::mutex> lock(shared->mutex);
            handler = shared->handler;
            if (handler) ++shared->inFlight;
        }
        if (!handler) return;
        handler();
        {
            std::lock_guard<std::mutex> lock(shared->mutex);
            --shared->inFlight;
        }
        shared->idle.notify_all();
    }
    mutable int calls = 0;
    int cancelAfterChecks = 100;
    std::shared_ptr<SharedState> shared = std::make_shared<SharedState>();
    mutable std::atomic<bool> externallyCancelled{false};
};

struct Sink final : HttpTransportSink {
    bool OnResponseHead(const HttpResponseHead& value) override {
        head = value;
        return acceptHead;
    }
    bool OnChunk(const std::vector<std::uint8_t>& value) override {
        chunks.push_back(value);
        return acceptChunks;
    }
    bool OnEof() override { eof = true; return acceptEof; }
    bool acceptHead = true;
    bool acceptChunks = true;
    bool acceptEof = true;
    bool eof = false;
    HttpResponseHead head;
    std::vector<std::vector<std::uint8_t>> chunks;
};

bool waitForBlockedRead(FakeApi& fake,
                        std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(fake.blockMutex);
    return fake.blockCv.wait_for(
        lock, timeout, [&fake] { return fake.readEntered; });
}

HttpTransferRequestPlan plan() {
    HttpTransferRequestPlan value;
    value.url = "https://github.com/xindong/harken-search/releases/download/v1.2.0/Aris.zip";
    value.headers = {{"Range", "bytes=400-"}, {"If-Range", "etag-120"}};
    value.identity.flightKey = "flight";
    value.identity.generation = 1;
    value.identity.partPath = "1.2.0/Aris.zip.part";
    value.expectedSize = 1000;
    value.expectedSha256 = std::string(64, 'a');
    value.resumeOffset = 400;
    value.metaPath = "1.2.0/Aris.zip.meta.json";
    return value;
}

void test_adapter_serializes_headers_streams_chunks_and_eof() {
    auto fake = std::make_unique<FakeApi>();
    auto* observed = fake.get();
    fake->head.statusCode = 206;
    fake->head.contentRange = "bytes 400-999/1000";
    fake->chunks = {std::vector<std::uint8_t>(256, 1),
                    std::vector<std::uint8_t>(344, 2)};
    auto port = Platform::UpdateHttp::Testing::CreateWithNativeApi(std::move(fake));
    Cancellation cancellation;
    Sink sink;
    const auto result = port->Execute(plan(), cancellation, sink);
    check(result.ok() && result.streamedBytes == 600 && sink.eof &&
              sink.chunks.size() == 2,
          "adapter streams deterministic chunks and signals exact EOF");
    check(observed->url ==
              "https://harkensearch.xd.com/aris/releases/download/v1.2.0/Aris.zip" &&
              observed->headers == "Range: bytes=400-\r\nIf-Range: etag-120\r\n",
          "adapter opens the COS ZIP URL and serializes owned headers");
    check(observed->closeCalls == 1,
          "native request is closed after successful streaming");
}

void test_previous_repository_is_rejected_before_network_io() {
    auto fake = std::make_unique<FakeApi>();
    auto* observed = fake.get();
    auto port = Platform::UpdateHttp::Testing::CreateWithNativeApi(std::move(fake));
    auto previous = plan();
    previous.url = "https://github.com/xindong/aris/releases/download/v1.2.0/Aris.zip";
    Cancellation cancellation;
    Sink sink;
    check(!port->Execute(previous, cancellation, sink).ok() && observed->openCalls == 0,
          "new clients reject the old repository before opening a download");
}

void test_cancellation_and_sink_rejection_stop_without_sleep_or_extra_reads() {
    auto preCancelled = std::make_unique<FakeApi>();
    auto* preObserved = preCancelled.get();
    auto prePort = Platform::UpdateHttp::Testing::CreateWithNativeApi(
        std::move(preCancelled));
    Cancellation cancellation;
    cancellation.cancelAfterChecks = 0;
    Sink sink;
    check(prePort->Execute(plan(), cancellation, sink).error ==
              HttpTransportExecutionError::Cancelled && preObserved->openCalls == 0,
          "pre-cancelled transfer performs no native request");

    auto rejected = std::make_unique<FakeApi>();
    auto* rejectedObserved = rejected.get();
    rejected->head.statusCode = 206;
    rejected->head.contentRange = "bytes 400-999/1000";
    auto rejectedPort = Platform::UpdateHttp::Testing::CreateWithNativeApi(
        std::move(rejected));
    Cancellation active;
    Sink rejectingSink;
    rejectingSink.acceptHead = false;
    check(rejectedPort->Execute(plan(), active, rejectingSink).error ==
              HttpTransportExecutionError::SinkRejected &&
              rejectedObserved->readCalls == 0,
          "protocol sink rejection prevents body reads");

    auto interrupted = std::make_unique<FakeApi>();
    auto* interruptedObserved = interrupted.get();
    interrupted->head.statusCode = 206;
    interrupted->chunks = {std::vector<std::uint8_t>(32, 1),
                           std::vector<std::uint8_t>(32, 2)};
    auto interruptedPort = Platform::UpdateHttp::Testing::CreateWithNativeApi(
        std::move(interrupted));
    Cancellation midStream;
    midStream.cancelAfterChecks = 3;
    Sink partialSink;
    const auto interruptedResult = interruptedPort->Execute(
        plan(), midStream, partialSink);
    check(interruptedResult.error == HttpTransportExecutionError::Cancelled &&
              interruptedResult.streamedBytes == 32 &&
              partialSink.chunks.size() == 1 && !partialSink.eof &&
              interruptedObserved->interruptCalls >= 1,
          "mid-stream cancellation stops before another read and preserves explicit offset");
}

void test_redirects_are_manual_https_and_host_allowlisted() {
    auto redirected = std::make_unique<FakeApi>();
    auto* observed = redirected.get();
    HttpResponseHead redirect;
    redirect.statusCode = 302;
    redirect.location =
        "https://harkensearch.xd.com/aris/releases/download/v1.2.0/cdn/Aris.zip";
    HttpResponseHead final;
    final.statusCode = 206;
    final.contentRange = "bytes 400-999/1000";
    redirected->heads = {redirect, final};
    redirected->chunks = {std::vector<std::uint8_t>(600, 1)};
    auto port = Platform::UpdateHttp::Testing::CreateWithNativeApi(
        std::move(redirected));
    Cancellation cancellation;
    Sink sink;
    const auto result = port->Execute(plan(), cancellation, sink);
    check(result.ok() && observed->openCalls == 2 &&
              observed->urls.back() == redirect.location,
          "approved COS redirect is followed explicitly");

    auto rejected = std::make_unique<FakeApi>();
    auto* rejectedObserved = rejected.get();
    redirect.location = "https://example.invalid/stolen.zip";
    rejected->heads = {redirect};
    auto rejectedPort = Platform::UpdateHttp::Testing::CreateWithNativeApi(
        std::move(rejected));
    Cancellation active;
    Sink unused;
    check(rejectedPort->Execute(plan(), active, unused).error ==
              HttpTransportExecutionError::InvalidPlan &&
              rejectedObserved->openCalls == 1 && rejectedObserved->readCalls == 0,
          "unapproved redirect target is rejected before credentials or body follow");
}

void test_zip_download_uses_cos_and_does_not_fall_back_to_github() {
    auto fallback = std::make_unique<FakeApi>();
    auto* observed = fallback.get();
    HttpResponseHead unavailable;
    unavailable.statusCode = 503;
    fallback->heads = {unavailable};
    auto port = Platform::UpdateHttp::Testing::CreateWithNativeApi(
        std::move(fallback));
    Cancellation cancellation;
    Sink sink;
    const auto result = port->Execute(plan(), cancellation, sink);
    check(!result.ok() && observed->urls.size() == 1 &&
              observed->urls[0] == "https://harkensearch.xd.com/aris/releases/download/v1.2.0/Aris.zip",
          "ZIP download uses COS only and stops on HTTP availability failure");
}

void test_cross_thread_cancel_interrupts_blocked_read_and_reports_cancelled() {
    auto blocked = std::make_unique<FakeApi>();
    auto* observed = blocked.get();
    blocked->head.statusCode = 206;
    blocked->blockRead = true;
    auto port = Platform::UpdateHttp::Testing::CreateWithNativeApi(
        std::move(blocked));
    Cancellation cancellation;
    cancellation.cancelAfterChecks = 100;
    Sink sink;
    HttpTransportExecutionResult result;
    std::thread worker([&] { result = port->Execute(plan(), cancellation, sink); });
    const bool readStarted = waitForBlockedRead(
        *observed, std::chrono::seconds(2));
    if (!readStarted) {
        check(false,
              "blocked-read precondition must start within the bounded wait");
        cancellation.Cancel();
        worker.join();
        return;
    }
    cancellation.Cancel();
    worker.join();
    check(result.error == HttpTransportExecutionError::Cancelled &&
              observed->interruptCalls >= 1,
          "cross-thread cancel closes a blocked native read and is not retryable Network");
}

void test_blocked_read_wait_fails_fast_when_precondition_is_absent() {
    FakeApi idle;
    const auto started = std::chrono::steady_clock::now();
    check(!waitForBlockedRead(idle, std::chrono::milliseconds(5)) &&
              std::chrono::steady_clock::now() - started <
                  std::chrono::seconds(1),
          "missing blocked-read precondition fails within a bounded timeout");
}

}  // namespace

int main() {
    test_previous_repository_is_rejected_before_network_io();
    test_adapter_serializes_headers_streams_chunks_and_eof();
    test_cancellation_and_sink_rejection_stop_without_sleep_or_extra_reads();
    test_redirects_are_manual_https_and_host_allowlisted();
    test_zip_download_uses_cos_and_does_not_fall_back_to_github();
    test_cross_thread_cancel_interrupts_blocked_read_and_reports_cancelled();
    test_blocked_read_wait_fails_fast_when_precondition_is_absent();
    return failures == 0 ? 0 : 1;
}
