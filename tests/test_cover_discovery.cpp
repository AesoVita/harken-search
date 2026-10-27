#include "platform/cover_discovery.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using Platform::CoverDiscovery::Find;
using Platform::CoverDiscovery::Status;
namespace CoverDiscoveryTesting = Platform::CoverDiscovery::Testing;

namespace {
int failures = 0;

void Check(bool ok, const char* message) {
    if (!ok) { ++failures; std::cerr << message << '\n'; }
}

void WriteBytes(const fs::path& path, size_t count) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    std::string bytes(count, 'x');
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::string Utf8(const fs::path& path) { return path.u8string(); }
}

int main() {
    const fs::path base = fs::current_path() / "test_cover_discovery_tmp";
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base);

    Check(Find({}, {}, false, 5).status == Status::NotFound, "empty folder");
    Check(Find(Utf8(base / "missing"), {}, false, 5).status == Status::NotFound,
          "missing folder");
    const std::string impossible = Utf8(base) + "/" + std::string(40000, 'x');
    Check(Find(impossible, {}, false, 5).status == Status::ProbeError,
          "unprobeable folder reports probe error");
    CoverDiscoveryTesting::FailAfterEntries(0);
    Check(Find(Utf8(base), {}, false, 5).status == Status::ProbeError,
          "directory open fault reports probe error");

    const fs::path strict = base / "strict";
    WriteBytes(strict / "FoLdEr.BmP", 8);
    auto result = Find(Utf8(strict), {}, false, 5);
    Check(result.status == Status::Found, "case-insensitive strict name");
    Check(fs::path(result.selectedPathUtf8).filename() == "FoLdEr.BmP",
          "strict match preserves actual path");

    const fs::path heuristic = base / "heuristic";
    WriteBytes(heuristic / "picture.jpg", 5121);
    CoverDiscoveryTesting::FailAfterEntries(1);
    Check(Find(Utf8(heuristic), {}, false, 5).status == Status::ProbeError,
          "incomplete scan discards collected candidate");
    result = Find(Utf8(heuristic), {}, false, 5);
    Check(result.status == Status::Found, "large heuristic image");
    Check(fs::path(result.selectedPathUtf8).filename() == "picture.jpg",
          "heuristic selected path");

    const fs::path small = base / "small";
    WriteBytes(small / "picture.png", 5120);
    Check(Find(Utf8(small), {}, false, 5).status == Status::NotFound,
          "heuristic threshold is greater than 5120");

    const fs::path advertiser = base / "advertiser";
    WriteBytes(advertiser / "promo.jpeg", 6000);
    Check(Find(Utf8(advertiser), {}, false, 5).status == Status::NotFound,
          "advertiser-like image is ignored");
    const fs::path unsupported = base / "unsupported";
    WriteBytes(unsupported / "picture.gif", 6000);
    WriteBytes(unsupported / "random.bmp", 6000);
    Check(Find(Utf8(unsupported), {}, false, 5).status == Status::NotFound,
          "heuristic discovery rejects unsupported extensions");

    const fs::path crowded = base / "crowded";
    for (int i = 0; i < 11; ++i)
        WriteBytes(crowded / ("image" + std::to_string(i) + ".jpg"), 6000);
    Check(Find(Utf8(crowded), {}, false, 5).status == Status::NotFound,
          "more than ten heuristic images are rejected");
    const fs::path bounded = base / "bounded";
    for (int i = 0; i < 10; ++i)
        WriteBytes(bounded / ("image" + std::to_string(i) + ".png"), 6000);
    Check(Find(Utf8(bounded), {}, false, 5).status == Status::Found,
          "ten heuristic images are accepted");

    const fs::path library = base / "Library";
    fs::create_directories(library);
    WriteBytes(base / "Artwork" / "label.bmp", 8);
    WriteBytes(base / "Artwork" / "cover.png", 8);
    WriteBytes(base / "Covers" / "cover.png", 8);
    result = Find(Utf8(library), Utf8(library), false, 5);
    Check(result.status == Status::Found, "library-root sibling probe");
    Check(fs::path(result.selectedPathUtf8).parent_path().filename() == "Artwork",
          "sibling order starts with Artwork");
    Check(fs::path(result.selectedPathUtf8).filename() == "cover.png",
          "strict sibling names use declared priority");

    const fs::path strictOnlyRoot = base / "StrictOnly" / "Library";
    fs::create_directories(strictOnlyRoot);
    WriteBytes(base / "StrictOnly" / "Covers" / "picture.jpg", 6000);
    Check(Find(Utf8(strictOnlyRoot), Utf8(strictOnlyRoot), false, 5).status == Status::NotFound,
          "sibling probes accept strict names only");

    const fs::path parent = library / "Parent";
    const fs::path child = parent / "Child";
    fs::create_directories(child);
    WriteBytes(parent / "front.jpg", 8);
    Check(Find(Utf8(child), Utf8(library), true, 0).status == Status::NotFound,
          "zero depth does not climb");
    Check(Find(Utf8(child), Utf8(library), true, 1).status == Status::Found,
          "one depth climbs one parent");

    const fs::path boundedRoot = base / "Boundary" / "BoundedLibrary";
    const fs::path boundedChild = boundedRoot / "Nested";
    fs::create_directories(boundedChild);
    WriteBytes(base / "Boundary" / "front.jpg", 8);
    Check(Find(Utf8(boundedChild), Utf8(boundedRoot), true, 10).status == Status::NotFound,
          "parent traversal never escapes the library root");

    fs::remove_all(base, ec);
    return failures;
}
