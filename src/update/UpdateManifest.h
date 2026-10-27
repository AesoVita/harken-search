#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace aris::update {

// @stage7-contract
// API contract: verification borrows all input views for the duration of the
// call and returns an entirely owned manifest/result. It performs no network,
// filesystem, process, UI, settings, or database work.

enum class UpdateChannel {
    Stable,
    Testing,
};

enum class DataCompatibility {
    BackwardCompatible,
};

enum class ManifestError {
    None,
    SignatureEncodingInvalid,
    PublicKeyEncodingInvalid,
    SignatureInvalid,
    JsonInvalid,
    SchemaUnsupported,
    MissingField,
    ProductMismatch,
    ChannelMismatch,
    PlatformMismatch,
    ArchitectureMismatch,
    VersionInvalid,
    NotNewer,
    RepositoryNotAllowed,
    AssetInvalid,
    DataCompatibilityUnsupported,
};

struct UpdateAsset {
    std::string url;
    std::uint64_t size = 0;
    std::string sha256;
};

struct UpdateManifest {
    int schemaVersion = 0;
    UpdateChannel channel = UpdateChannel::Stable;
    std::string version;
    std::string minimumBootstrapVersion;
    std::string minimumDirectVersion;
    DataCompatibility dataCompatibility = DataCompatibility::BackwardCompatible;
    UpdateAsset asset;
    std::string publishedAt;
    std::string buildId;
};

struct ManifestPolicy {
    std::string currentVersion;
    UpdateChannel selectedChannel = UpdateChannel::Stable;
    std::string platform;
    std::string architecture;
    std::string allowedRepository;
};

struct ManifestResult {
    ManifestError error = ManifestError::None;
    UpdateManifest manifest;
    std::string detail;

    bool ok() const { return error == ManifestError::None; }
};

ManifestResult VerifyAndParseManifest(std::string_view manifestBytes,
                                      std::string_view signatureHex,
                                      std::string_view publicKeyHex,
                                      const ManifestPolicy& policy);

}  // namespace aris::update
