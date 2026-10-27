#include "update/UpdateCheckHttpsTransport.h"
#include "update/UpdateReleaseConfig.h"

#include <cstdio>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

namespace {

int failures = 0;

void check(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

}  // namespace

class InterruptingCancellation final
    : public aris::update::UpdateCheckCancellation {
public:
    bool IsCancellationRequested() const override { return false; }
    std::unique_ptr<InterruptRegistration> RegisterInterrupt(
        std::function<void()> handler) const override {
        ++registrations;
        handler();
        class Registration final : public InterruptRegistration {};
        return std::make_unique<Registration>();
    }
    mutable int registrations = 0;
};

class NeverCancelled final : public aris::update::UpdateCheckCancellation {
public:
    bool IsCancellationRequested() const override { return false; }
    std::unique_ptr<InterruptRegistration> RegisterInterrupt(
        std::function<void()>) const override {
        class Registration final : public InterruptRegistration {};
        return std::make_unique<Registration>();
    }
};

class FakeReadPort final : public aris::update::UpdateCheckHttpsReadPort {
public:
    aris::update::UpdateCheckReadResult Read(
        std::size_t maxBytes, std::uint64_t timeoutMs) override {
        ++reads;
        lastMaxBytes = maxBytes;
        lastTimeoutMs = timeoutMs;
        if (abortCount != 0) {
            return {aris::update::UpdateCheckReadStatus::Aborted, {}};
        }
        if (script.empty()) return {aris::update::UpdateCheckReadStatus::End, {}};
        const auto result = script.front();
        script.erase(script.begin());
        return result;
    }
    void Abort() noexcept override { ++abortCount; }

    std::vector<aris::update::UpdateCheckReadResult> script;
    int reads = 0;
    int abortCount = 0;
    std::size_t lastMaxBytes = 0;
    std::uint64_t lastTimeoutMs = 0;
};

int main() {
    using namespace aris::update;
    check(IsApprovedUpdateCheckUrl(
              "https://github.com/xindong/harken-search/releases/download/v1/update-manifest.json",
              true),
          "public Aris release URL is accepted by the default build");
    check(IsApprovedUpdateCheckUrl(
              "https://github.com/xindong/harken-search/releases/latest/download/manifest-harken-search-win-x64-stable.json",
              true),
          "public Aris latest release URL used by product composition is accepted");
    check(IsApprovedUpdateCheckUrl(
              "https://harkensearch.xd.com/aris/releases/latest/download/manifest-harken-search-win-x64-stable.json",
              true) &&
              IsApprovedUpdateCheckUrl(
                  "https://harkensearch.xd.com/aris/releases/latest/download/manifest-harken-search-win-x64-stable.json.sig",
                  true) &&
              IsApprovedUpdateCheckUrl(
                  "https://harkensearch.xd.com/aris/releases/latest/download/manifest-harken-search-win-x64-stable.json",
                  false),
          "company cloud stable manifest and signature are accepted");
    check(!IsApprovedUpdateCheckUrl(
              "https://harkensearch.xd.com/aris/releases/latest/download/other.json",
              true) &&
              !IsApprovedUpdateCheckUrl(
                  "https://harkensearch.xd.com.evil/aris/releases/latest/download/manifest-harken-search-win-x64-stable.json",
                  true) &&
              !IsApprovedUpdateCheckUrl(
                  "http://harkensearch.xd.com/aris/releases/latest/download/manifest-harken-search-win-x64-stable.json",
                  true),
          "company cloud approval is limited to the stable manifest pair");
    check(IsApprovedUpdateCheckUrl(
              "https://harkensearch.xd.com/aris/releases/download/v1.0.34/ArisBootstrap.package.stable.json",
              true) &&
              IsApprovedUpdateCheckUrl(
                  "https://harkensearch.xd.com/aris/releases/download/v1.0.34/ArisBootstrap.package.stable.json.sig",
                  true) &&
              IsApprovedUpdateCheckUrl(
                  "https://harkensearch.xd.com/aris/releases/download/v1.0.34/ArisBootstrap.package.json",
                  true) &&
              IsApprovedUpdateCheckUrl(
                  "https://harkensearch.xd.com/aris/releases/download/v1.0.34/ArisBootstrap.package.json.sig",
                  true),
          "company cloud Bootstrap package plan and signature URLs are accepted");
    check(IsApprovedUpdateCheckUrl(
              "https://harkensearch.xd.com/aris/releases/download/v1.0.34/ArisBootstrap.package.stable.json",
              false) &&
              IsApprovedUpdateCheckUrl(
                  "https://harkensearch.xd.com/aris/releases/download/v1.0.34/ArisBootstrap.package.stable.json.sig",
                  false),
          "company cloud Bootstrap package URLs survive the redirect hop");
    check(!IsApprovedUpdateCheckUrl(
              "https://harkensearch.xd.com.evil.example/aris/releases/download/v1.0.34/ArisBootstrap.package.stable.json",
              true) &&
              !IsApprovedUpdateCheckUrl(
                  "https://harkensearch.xd.com/attacker/releases/download/v1.0.34/ArisBootstrap.package.stable.json",
                  true) &&
              !IsApprovedUpdateCheckUrl(
                  "https://harkensearch.xd.com/aris/v1.0.34/ArisBootstrap.package.stable.json",
                  true) &&
              !IsApprovedUpdateCheckUrl(
                  "http://harkensearch.xd.com/aris/releases/download/v1.0.34/ArisBootstrap.package.stable.json",
                  true) &&
              !IsApprovedUpdateCheckUrl(
                  "https://harkensearch.xd.com/aris/releases/download/../../secret",
                  true) &&
              !IsApprovedUpdateCheckUrl(
                  "https://harkensearch.xd.com/aris/releases/download/v1.0.34/..\\secret",
                  true),
          "company cloud authority refuses lookalike hosts, neighbor paths, plaintext HTTP and traversal");
    check(!IsApprovedUpdateCheckUrl(
              "https://harkensearch.xd.com/aris/releases/download/v1.0.34/evil.json",
              true) &&
              !IsApprovedUpdateCheckUrl(
                  "https://harkensearch.xd.com/aris/releases/download/v1.0.34/ArisBootstrap.package.stable.json?x=1",
                  true) &&
              !IsApprovedUpdateCheckUrl(
                  "https://harkensearch.xd.com/aris/releases/download/v1.0.34/ArisBootstrap.package.stable.json#frag",
                  true) &&
              !IsApprovedUpdateCheckUrl(
                  "https://harkensearch.xd.com/aris/releases/download/v1.0.34/ArisBootstrap.package.stable.json/extra",
                  true) &&
              !IsApprovedUpdateCheckUrl(
                  "https://harkensearch.xd.com/aris/releases/download/v1.0.34/ArisBootstrap.package.stable.json/",
                  true) &&
              !IsApprovedUpdateCheckUrl(
                  "https://harkensearch.xd.com/aris/releases/download/v1.0.34/HarkenSearchBootstrap.package.stable.json",
                  true),
          "company cloud authority accepts only the exact Bootstrap package plan/signature basenames");
    check(!IsApprovedUpdateCheckUrl(
              "https://github.com/attacker/aris-releases/releases/download/v1/update-manifest.json",
              true) &&
              !IsApprovedUpdateCheckUrl("http://github.com/xindong/harken-search/releases/download/v1/x", true),
          "repository substitution and plaintext HTTP are rejected");
    check(ResolveUpdateDownloadUrl(
              "https://github.com/xindong/harken-search/releases/download/v1.2.3/Aris-1.2.3-win-x64-update.zip") ==
              "https://harkensearch.xd.com/aris/releases/download/v1.2.3/Aris-1.2.3-win-x64-update.zip" &&
              ResolveUpdateDownloadUrl(
                  "https://github.com/xindong/harken-search/releases/latest/download/manifest-harken-search-win-x64-stable.json") ==
                  "https://github.com/xindong/harken-search/releases/latest/download/manifest-harken-search-win-x64-stable.json" &&
              ResolveUpdateDownloadUrl("https://evil.example/x") ==
                  "https://evil.example/x",
          "control files stay on GitHub; only a versioned trusted ZIP maps to COS");
    check(IsApprovedUpdateCheckUrl(
              "https://release-assets.githubusercontent.com/github-production-release-asset/x",
              false) &&
              IsApprovedUpdateCheckUrl(
                  "https://objects.githubusercontent.com/github-production-release-asset/x",
                  false) &&
              !IsApprovedUpdateCheckUrl("https://evil.example/x", false),
          "only exact HTTPS GitHub asset redirect hosts are accepted");
    check(IsApprovedUpdateCheckUrl(
              "https://github.com/xindong/harken-search/releases/download/v1.0.4/manifest-harken-search-win-x64-stable.json",
              false),
          "GitHub resolves /releases/latest/download/ via a same-origin 302 to /releases/download/vX.Y.Z/ so this hop must be accepted");
    // Redirect approval stays on the same compile-time repository coordinate.
    check(!IsApprovedUpdateCheckUrl(
              "http://github.com/xindong/harken-search/releases/download/v1/manifest.json",
              false) &&
              !IsApprovedUpdateCheckUrl(
                  "http://github.com/xindong/harken-search/releases/latest/download/manifest.json",
                  false),
          "redirect approval refuses plaintext HTTP regardless of repository");
    check(!IsApprovedUpdateCheckUrl(
              "https://attacker.example/aris-releases/releases/download/v1/manifest.json",
              false) &&
              !IsApprovedUpdateCheckUrl(
                  "https://release-assets.githubusercontent.com.evil.example/x",
                  false),
          "redirect approval never leaves the exact approved hosts");
    check(!IsApprovedUpdateCheckUrl(
              "https://github.com/attacker/aris-releases/releases/download/v1/manifest.json",
              true) &&
              IsApprovedUpdateCheckUrl(
                  "https://github.com/xindong/harken-search/releases/latest/download/manifest.json",
                  true),
          "the initial request stays pinned to the compile-time repository coordinate");
    check(IsApprovedUpdateCheckUrl(
              "https://github.com/xindong/harken-search/releases/latest/download/manifest-harken-search-win-x64-stable.json",
              false),
          "a repository rename 301 that still targets releases/latest/download must be accepted");
    check(IsApprovedUpdateCheckUrl(
              "https://github.com/xindong/harken-search/releases/download/v1.0.24/manifest-harken-search-win-x64-stable.json",
              false),
          "the renamed repository's resolved version hop must be accepted");
    check(!IsApprovedUpdateCheckUrl(
              "https://github.com/zhongziyu-cmd/aris-releases/releases/latest/download/manifest-harken-search-win-x64-stable.json",
              false) &&
              !IsApprovedUpdateCheckUrl(
                  "https://github.com/attacker/aris-releases/releases/download/v1/manifest.json",
                  false),
          "legacy and arbitrary repositories are refused on redirect hops");
    check(!IsApprovedUpdateCheckUrl(
              "https://github.com/xindong/harken-search/blob/main/manifest.json",
              false) &&
              !IsApprovedUpdateCheckUrl(
                  "https://github.com.evil.example/xindong/harken-search/releases/download/v1/x",
                  false) &&
              !IsApprovedUpdateCheckUrl(
                  "https://github.com/xindong/harken-search/releases/download/../../secret",
                  false) &&
              !IsApprovedUpdateCheckUrl(
                  "https://release-assets.githubusercontent.com/../secret",
                  false),
          "canonical redirects refuse non-release paths, lookalike hosts and traversal");
    check(ClassifyUpdateCheckHttpStatus(200) == UpdateCheckFetchError::None &&
              ClassifyUpdateCheckHttpStatus(404) == UpdateCheckFetchError::Http4xx &&
              ClassifyUpdateCheckHttpStatus(503) == UpdateCheckFetchError::Http5xx &&
              ClassifyUpdateCheckHttpStatus(302) == UpdateCheckFetchError::InvalidResponse,
          "HTTP status classification stays explicit");

    {
        FakeReadPort read;
        read.script.push_back({UpdateCheckReadStatus::Chunk, {1, 2, 3}});
        read.script.push_back({UpdateCheckReadStatus::End, {}});
        NeverCancelled cancellation;
        const auto body = ReadUpdateCheckHttpsBody(
            read, cancellation, kUpdateCheckHttpsMaximumBodyBytes);
        check(body.ok() && body.body.size() == 3 && read.reads == 2 &&
                  read.lastTimeoutMs == kUpdateCheckHttpsReadTimeoutMs,
              "bounded read helper uses fixed timeout and owned chunks");
    }
    {
        FakeReadPort read;
        InterruptingCancellation cancellation;
        const auto aborted = ReadUpdateCheckHttpsBody(
            read, cancellation, kUpdateCheckHttpsMaximumBodyBytes);
        check(aborted.error == UpdateCheckFetchError::Cancelled &&
                  cancellation.registrations == 1 && read.abortCount == 1 &&
                  read.reads == 1,
              "cancellation registration invokes the adapter abort path during a read");
    }
    {
        FakeReadPort read;
        NeverCancelled cancellation;
        const auto rejected = ReadUpdateCheckHttpsBody(
            read, cancellation, std::numeric_limits<std::size_t>::max());
        check(rejected.error == UpdateCheckFetchError::InvalidResponse &&
                  read.reads == 0 && read.abortCount == 0,
              "UINT_MAX body cap is rejected before read or allocation");
    }
    const auto created = CreateUpdateCheckHttpsTransport();
#if defined(_WIN32)
    check(created.ok(), "production WinINet check transport opens a session");
#else
    check(created.status == UpdateCheckHttpsCreateStatus::Unsupported,
          "non-Windows factory fails closed");
#endif
    return failures == 0 ? 0 : 1;
}
