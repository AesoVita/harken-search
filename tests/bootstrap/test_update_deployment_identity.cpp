#include "platform/update_deployment_identity.h"

#include <cstdio>
#include <string>

namespace {

int failures = 0;

void check(bool value, const char* message) {
    if (value) return;
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", message);
}

aris::platform::DeploymentIdentitySnapshot portableSnapshot() {
    aris::platform::DeploymentIdentitySnapshot snapshot;
    snapshot.mode = aris::platform::DeploymentMode::Portable;
    snapshot.openedRoot = "C:/HarkenSearchPortable";
    snapshot.executablePath = "C:/HarkenSearchPortable/Aris.exe";
    snapshot.rootWritable = true;
    snapshot.rootIsSystemDirectory = false;
    snapshot.rootIsReparsePoint = false;
    snapshot.oldExecutable = {"C:/HarkenSearchPortable/Aris.exe", 42U, {1, 2, 3}, true};
    snapshot.openedRootIdentity = {"C:/HarkenSearchPortable", 42U, {4, 5, 6}, true};
    return snapshot;
}

void test_portable_identity() {
    const auto result = aris::platform::ResolveDeploymentIdentity(portableSnapshot());
    check(result.ok(), "portable root with stable identity is admitted");
    check(result.identity.has_value() &&
              result.identity->installationId.rfind("portable-", 0U) == 0U,
          "portable installation id is derived from opened root identity");
}

void test_installed_appid_and_conflicts() {
    auto snapshot = portableSnapshot();
    snapshot.mode = aris::platform::DeploymentMode::Installed;
    snapshot.registry = {{"{31B6E215-3D2B-43EA-88C8-72D73ABC9A0C}",
                          "C:/HarkenSearchPortable", 0, 64}};
    auto admitted = aris::platform::ResolveDeploymentIdentity(snapshot);
    check(admitted.ok(), "exact installed AppId is admitted");
    check(admitted.identity.has_value() &&
              admitted.identity->installationId.rfind("installed-", 0U) == 0U,
          "installed installation id is bound to exact AppId");
    snapshot.registry.push_back(snapshot.registry.front());
    check(aris::platform::ResolveDeploymentIdentity(snapshot).error ==
              aris::platform::DeploymentIdentityError::RegistryConflict,
          "duplicate registry registrations fail closed");
}

void test_installed_root_trailing_separator_is_the_same_directory() {
    auto snapshot = portableSnapshot();
    snapshot.mode = aris::platform::DeploymentMode::Installed;
    snapshot.openedRoot = "C:/HarkenSearchPortable/";
    snapshot.openedRootIdentity.canonicalPath = snapshot.openedRoot;
    snapshot.registry = {{aris::platform::kArisInstalledAppId,
                          "C:/HarkenSearchPortable/", 0, 64}};
    check(aris::platform::ResolveDeploymentIdentity(snapshot).ok(),
          "installed root trailing separator does not reject the running executable");
}

void test_portable_risks_and_mode_closed_set() {
    auto snapshot = portableSnapshot();
    snapshot.rootIsReparsePoint = true;
    check(aris::platform::ResolveDeploymentIdentity(snapshot).error ==
              aris::platform::DeploymentIdentityError::RootReparsePoint,
          "portable reparse root fails closed");
    snapshot = portableSnapshot();
    snapshot.rootWritable = false;
    check(aris::platform::ResolveDeploymentIdentity(snapshot).error ==
              aris::platform::DeploymentIdentityError::RootNotWritable,
          "portable non-writable root fails closed");
}

void test_app_id_matching_is_exact_not_case_folded() {
    auto installed = portableSnapshot();
    installed.mode = aris::platform::DeploymentMode::Installed;
    installed.registry = {{"{31b6e215-3d2b-43ea-88c8-72d73abc9a0c}",
                           "C:/HarkenSearchPortable", 0, 64}};
    check(aris::platform::ResolveDeploymentIdentity(installed).error ==
              aris::platform::DeploymentIdentityError::RegistryMissing,
          "installed case-variant AppId is not accepted as exact registration");
    installed.registry = {{"{OTHER-APP-ID}", "C:/HarkenSearchPortable", 0, 64}};
    check(aris::platform::ResolveDeploymentIdentity(installed).error ==
              aris::platform::DeploymentIdentityError::RegistryMissing,
          "installed unrelated AppId is not accepted");
    installed.registry = {{"{A8B3C9D1-E4F5-4A6B-8C9D-0E1F2A3B4C5D}",
                           "C:/HarkenSearchPortable", 0, 64}};
    check(aris::platform::ResolveDeploymentIdentity(installed).error ==
              aris::platform::DeploymentIdentityError::RegistryMissing,
          "legacy A8B3 AppId is not accepted as the current installed identity");
    installed.mode = aris::platform::DeploymentMode::Portable;
    check(aris::platform::ResolveDeploymentIdentity(installed).ok(),
          "portable case-variant or unrelated AppId does not imply installed mode");
    installed.registry = {{aris::platform::kArisInstalledAppId,
                           "C:/HarkenSearchPortable", 0, 64}};
    check(aris::platform::ResolveDeploymentIdentity(installed).error ==
              aris::platform::DeploymentIdentityError::PortableAppIdPresent,
          "portable exact AppId registration is rejected");
}

}  // namespace

int main() {
    test_portable_identity();
    test_installed_appid_and_conflicts();
    test_installed_root_trailing_separator_is_the_same_directory();
    test_portable_risks_and_mode_closed_set();
    test_app_id_matching_is_exact_not_case_folded();
    return failures == 0 ? 0 : 1;
}
