#include "ProductEdition.h"

#include <cstdio>
#include <string>

namespace FeatureManifest {
bool loadAndVerify(const std::string&) { return false; }
bool isEnabled(const char*) { return true; }
bool isLoaded() { return false; }
} // namespace FeatureManifest

namespace Platform {
std::string GetMachineId() { return "test-machine"; }
} // namespace Platform

static int s_pass = 0;
static int s_fail = 0;

#define EXPECT(cond, msg) do { \
    if (cond) { ++s_pass; std::printf("PASS: %s\n", msg); } \
    else { ++s_fail; std::printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } \
} while (0)

static void testOpenSourceBuildKeepsOneCompleteFeatureSurface() {
    ProductEdition::LicenseState legacyState;
    legacyState.keyValid = true;
    legacyState.grantedEdition = ProductEdition::Edition::Pro;
    legacyState.effectiveEdition = ProductEdition::Edition::Pro;
    ProductEdition::setLicenseState(&legacyState);

    EXPECT(ProductEdition::resolveEdition(legacyState) == ProductEdition::Edition::Ultimate,
           "open-source release ignores legacy license tiers");
    EXPECT(ProductEdition::current() == ProductEdition::Edition::Ultimate,
           "open-source release exposes the complete feature surface");
    EXPECT(ProductEdition::hasFeature("sub_playlist"),
           "open-source release exposes former Standard features");
    EXPECT(ProductEdition::hasFeature("reverse_playback"),
           "open-source release exposes former Ultimate features");

    ProductEdition::setIntegrityCheckFailed();
    EXPECT(ProductEdition::current() == ProductEdition::Edition::Ultimate,
           "a stale legacy sidecar cannot downgrade an open-source build");
    EXPECT(ProductEdition::hasFeature("column_preset_dial"),
           "a stale legacy sidecar cannot hide the preset dial");
}

int main() {
    testOpenSourceBuildKeepsOneCompleteFeatureSurface();
    std::printf("\n%d passed, %d failed\n", s_pass, s_fail);
    return s_fail == 0 ? 0 : 1;
}
