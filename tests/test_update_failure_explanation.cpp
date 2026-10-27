// Every distinct update failure used to render the same sentence:
// "当前没有可用的稳定发布，或暂时无法连接 GitHub。"
//
// On 2026-08-11 a single isolated 1.0.23 install produced three unrelated
// failures in a row -- a rejected check redirect, a rejected download plan and
// a mismatched Runtime Pair sibling -- and all three showed that one
// network-flavoured line.  None of the three had anything to do with network
// reachability.  That is why the same install failure looks different and
// unexplainable on different machines: the message hides which gate refused.
//
// These assertions pin that each failure class gets its own headline and a
// next step the user can actually act on.

#include "update/HarkenSearchUpdateComposition.h"

#include <cstdio>
#include <string>

namespace {

int g_failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

bool Contains(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

int main() {
    using namespace aris::update;

    // No failure -> nothing to explain.
    const auto none = ExplainHarkenSearchUpdateFailure("");
    check(none.kind == HarkenSearchUpdateFailureKind::None && none.headline.empty(),
          "an empty detail produces no explanation");

    // Network: the release channel could not be reached at all.
    const auto network = ExplainHarkenSearchUpdateFailure(
        "download network transport failed (WinInet/system error 12007)");
    check(network.kind == HarkenSearchUpdateFailureKind::Network &&
              !network.headline.empty() && !network.guidance.empty(),
          "a transport error is classified as a network failure");
    check(!Contains(network.guidance, u8"GitHub"),
          "the network guidance no longer names GitHub as the single source");
    check(Contains(network.guidance, u8"更新服务"),
          "the network guidance keeps a source-neutral update-service message");

    // Release: the channel answered but the release was refused.  This is the
    // class that used to be indistinguishable from a network outage.
    const auto redirect =
        ExplainHarkenSearchUpdateFailure("update check redirect is not approved");
    check(redirect.kind == HarkenSearchUpdateFailureKind::Release,
          "a rejected check redirect is a release failure, not a network one");
    check(!Contains(redirect.guidance, u8"无法连接"),
          "a rejected redirect never blames network reachability");

    // Download: the plan or its verification failed.
    const auto plan = ExplainHarkenSearchUpdateFailure(
        "download plan rejected by transport (unsafe plan, unapproved redirect "
        "target, or exceeded redirect budget; system error 0)");
    check(plan.kind == HarkenSearchUpdateFailureKind::Download,
          "a rejected download plan is classified as a download failure");
    check(!Contains(plan.guidance, u8"无法连接"),
          "a rejected download plan never blames network reachability");

    // Local install admission -- the class behind "it works on one machine but
    // not another".  Every one of these is a local state problem.
    const char* localDetails[] = {
        "Runtime Pair sibling admission failed",
        "Runtime Pair launch copy admission failed",
        "Runtime Pair cleanup authority admission failed",
        "deployment identity admission failed",
        "protected data root admission failed",
        "verified update archive is missing",
        "verified staged path is unsafe",
        "Bootstrap package binding mismatch",
    };
    for (const char* detail : localDetails) {
        const auto local = ExplainHarkenSearchUpdateFailure(detail);
        const std::string what =
            std::string("local admission detail is classified locally: ") + detail;
        check(local.kind == HarkenSearchUpdateFailureKind::LocalInstall, what.c_str());
        check(!Contains(local.guidance, u8"无法连接"),
              "a local admission failure never blames network reachability");
        check(!local.guidance.empty(),
              "a local admission failure still tells the user what to do next");
    }

    // The four classes must not collapse back into one message.
    check(network.headline != redirect.headline ||
              network.guidance != redirect.guidance,
          "network and release failures do not share the same message");
    const auto local = ExplainHarkenSearchUpdateFailure("Runtime Pair sibling admission failed");
    check(local.headline != network.headline,
          "local install failures do not reuse the network headline");
    check(local.guidance != plan.guidance,
          "local install and download failures do not share guidance");

    // An unrecognised detail must stay honest rather than guessing a cause.
    const auto unknown = ExplainHarkenSearchUpdateFailure("something nobody has seen yet");
    check(unknown.kind != HarkenSearchUpdateFailureKind::None && !unknown.headline.empty() &&
              !unknown.guidance.empty(),
          "an unknown detail still yields a headline and guidance");

    if (g_failures == 0) std::printf("all update failure explanations pass\n");
    return g_failures == 0 ? 0 : 1;
}
