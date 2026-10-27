#include "update/HarkenSearchUpdateComposition.h"

#include <array>
#include <cstdlib>
#include <optional>
#include <string>

namespace aris::update {
namespace {

bool Contains(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

// "1.0.24" -> {1, 0, 24}.  Returns nothing unless the value is exactly three
// non-negative integers, so callers can tell "older" apart from "cannot tell".
std::optional<std::array<long, 3>> ParseVersion(const std::string& value) {
    if (value.empty()) return std::nullopt;
    std::array<long, 3> parts{};
    std::size_t begin = 0;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        const bool last = index + 1 == parts.size();
        const std::size_t dot = value.find('.', begin);
        if (last != (dot == std::string::npos)) return std::nullopt;
        const std::string part =
            value.substr(begin, last ? std::string::npos : dot - begin);
        if (part.empty()) return std::nullopt;
        for (const unsigned char digit : part) {
            if (digit < '0' || digit > '9') return std::nullopt;
        }
        parts[index] = std::strtol(part.c_str(), nullptr, 10);
        begin = last ? begin : dot + 1;
    }
    return parts;
}

}  // namespace

// `recordedVersion` supports the pure comparison helper below, but it does
// not currently come from UpdateSnapshot/patch-info.json.  The integrated
// composition therefore uses only the ready-patch signal.  Persisting that
// field later requires a compatible schema migration: UpdateStateStore's
// HasExactKeys check is a strict whitelist, so changing it carelessly would
// make old and new patch-info records mutually unreadable during an upgrade.
bool IsStaleUpdateFailure(const std::string& recordedVersion,
                          const std::string& runningVersion,
                          const std::string& readyPatchVersion) {
    const auto recorded = ParseVersion(recordedVersion);
    const auto running = ParseVersion(runningVersion);
    // Recorded by an older build: whatever refused is no longer running.
    // This branch is pure-function coverage for a future persisted version;
    // recordedVersion is not currently present in patch-info.json.
    if (recorded.has_value() && running.has_value() && *recorded < *running) {
        return true;
    }

    // A ready patch for a newer version proves the pipeline got past whatever
    // the recorded failure describes.  This branch remains usable when the
    // recorded version is unavailable, which is the current integrated path.
    const auto ready = ParseVersion(readyPatchVersion);
    if (ready.has_value() && running.has_value() && *ready > *running) {
        return true;
    }

    // Cannot establish staleness -> keep showing the failure.  Hiding a real
    // failure is worse than showing an old one.
    return false;
}

// Turn a raw failure detail into something a user can act on.
//
// Before this existed the update centre printed one sentence for every
// failure -- "当前没有可用的稳定发布，或暂时无法连接 GitHub。" -- regardless of
// what actually refused.  On 2026-08-11 one isolated 1.0.23 install hit three
// unrelated failures in a row (rejected check redirect, rejected download plan,
// mismatched Runtime Pair sibling) and every one of them rendered that same
// network-flavoured line.  None involved network reachability.  That single
// message is why the same underlying failure looked machine-specific and
// unexplainable across different installs.
//
// The detail strings matched here are produced by the update runtime and the
// Bootstrap authority.  An unrecognised detail deliberately does not fall back
// to a guessed cause: it says the cause is unrecognised and surfaces the raw
// detail, which is more useful than a confident wrong answer.
HarkenSearchUpdateFailureExplanation ExplainHarkenSearchUpdateFailure(
    const std::string& lastError) {
    HarkenSearchUpdateFailureExplanation explanation;
    if (lastError.empty()) return explanation;

    // Local install admission.  These are all local state problems: the copy of
    // Aris on this machine could not be admitted for replacement.  They are the
    // class most likely to differ between machines, so they must never be
    // reported as a connectivity problem.
    if (Contains(lastError, "Runtime Pair") ||
        Contains(lastError, "deployment identity") ||
        Contains(lastError, "protected data root") ||
        Contains(lastError, "binding mismatch") ||
        Contains(lastError, "staged path is unsafe") ||
        Contains(lastError, "archive is missing") ||
        Contains(lastError, "archive changed during admission") ||
        Contains(lastError, "payload extraction failed") ||
        Contains(lastError, "payload mutation ownership failed") ||
        Contains(lastError, "capability") ||
        Contains(lastError, "invocation namespace") ||
        Contains(lastError, "payload-cleanup-pending") ||
        Contains(lastError, "admission failed") ||
        Contains(lastError, "process launch failed")) {
        explanation.kind = HarkenSearchUpdateFailureKind::LocalInstall;
        explanation.headline = u8"更新准备失败";
        explanation.guidance =
            u8"更新已下载完成，但这台电脑上的 Harken Search 安装无法被安全替换。"
            u8"请完全退出 Harken Search 后重试；若仍然失败，请把下面的详细信息发给我们。";
        return explanation;
    }

    // Download stage: the asset itself could not be fetched or trusted.
    if (Contains(lastError, "download plan rejected") ||
        Contains(lastError, "download sink") ||
        Contains(lastError, "download cancelled") ||
        Contains(lastError, "exceeds body cap") ||
        Contains(lastError, "hash") || Contains(lastError, "sha256")) {
        explanation.kind = HarkenSearchUpdateFailureKind::Download;
        explanation.headline = u8"更新下载失败";
        explanation.guidance =
            u8"已找到新版本，但更新包没有通过下载或校验。请稍后重试。";
        return explanation;
    }

    // Network: we could not reach or read the release channel at all.  Checked
    // after the more specific cases so that a rejected plan is not mistaken for
    // an outage just because it mentions a transport.
    if (Contains(lastError, "network transport failed") ||
        Contains(lastError, "HTTPS request failed") ||
        Contains(lastError, "HTTPS session failed") ||
        Contains(lastError, "response read failed") ||
        Contains(lastError, "timeout") || Contains(lastError, "Timeout")) {
        explanation.kind = HarkenSearchUpdateFailureKind::Network;
        explanation.headline = u8"无法连接更新服务";
        explanation.guidance =
            u8"暂时无法连接更新服务。请检查网络连接后重试。";
        return explanation;
    }

    // Release stage: the channel answered, but the release was refused.  A
    // rejected redirect lands here -- the network was fine, the client declined
    // the response -- which is exactly the case the old wording hid.
    if (Contains(lastError, "redirect") || Contains(lastError, "signature") ||
        Contains(lastError, "manifest") || Contains(lastError, "HTTP status") ||
        Contains(lastError, "response status") ||
        Contains(lastError, "InvalidResponse") ||
        Contains(lastError, "not approved")) {
        explanation.kind = HarkenSearchUpdateFailureKind::Release;
        explanation.headline = u8"更新信息校验失败";
        explanation.guidance =
            u8"已连接到更新服务，但这次的发布信息没有通过校验，本次更新已停止。"
            u8"这通常需要我们修复发布配置，请把下面的详细信息发给我们。";
        return explanation;
    }

    // Unrecognised: stay honest instead of guessing.
    explanation.kind = HarkenSearchUpdateFailureKind::Release;
    explanation.headline = u8"更新失败";
    explanation.guidance =
        u8"更新未能完成，原因暂时无法归类。请稍后重试；"
        u8"若反复出现，请把下面的详细信息发给我们。";
    return explanation;
}

}  // namespace aris::update
