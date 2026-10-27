#include "AnnotationBridge.h"
#include "ThemeInspector.h"

#include "imgui.h"
#include "nlohmann_json.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) {
        std::cout << "PASS: " << message << '\n';
        return;
    }
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

} // namespace

int main() {
    ImGui::CreateContext();
    ImGui::GetIO().DisplaySize = ImVec2(1200.0f, 800.0f);

    ThemeInspector::SetActive(false);
    ThemeInspector::SetBridgeCollectionEnabled(true);
    ThemeInspector::BeginFrame();
    ThemeInspector::RegisterZone(
        ImVec2(40.0f, 30.0f),
        ImVec2(10.0f, 5.0f),
        "toolbarBG",
        "Toolbar",
        "src/main.cpp:43670");
    ThemeInspector::RegisterZone(
        ImVec2(50.0f, 10.0f),
        ImVec2(80.0f, 30.0f),
        "toolbarBG",
        "Toolbar duplicate",
        "src/main.cpp:43674");
    ThemeInspector::EndFrame();

    const ThemeInspector::AnnotationFrameSnapshot snapshot =
        ThemeInspector::CopyLatestSnapshot();

    Expect(ThemeInspector::IsCollecting(), "bridge collection works without visual inspector");
    Expect(snapshot.displayWidth == 1200.0f, "snapshot owns display width");
    Expect(snapshot.displayHeight == 800.0f, "snapshot owns display height");
    Expect(snapshot.zones.size() == 2, "snapshot contains all registered zones");
    if (snapshot.zones.size() == 2) {
        const auto& first = snapshot.zones[0];
        const auto& second = snapshot.zones[1];
        Expect(first.minX == 10.0f && first.minY == 5.0f, "zone coordinates are normalized");
        Expect(first.maxX == 40.0f && first.maxY == 30.0f, "zone maximum is normalized");
        Expect(first.key == "toolbarBG", "zone key is owned by snapshot");
        Expect(first.displayName == "Toolbar", "display name is owned by snapshot");
        Expect(first.sourceAnchor == "src/main.cpp:43670", "source anchor is preserved");
        Expect(first.stableId == "toolbarBG--0", "first duplicate receives stable ordinal zero");
        Expect(second.stableId == "toolbarBG--1", "second duplicate receives stable ordinal one");
        Expect(first.order == 0 && second.order == 1, "registration order is deterministic");
    }

    ThemeInspector::SetBridgeCollectionEnabled(false);
    ImGui::DestroyContext();

    const auto outputDirectory = std::filesystem::temp_directory_path() /
        ("aris-annotation-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    AnnotationBridge::Publisher publisher;
    Expect(publisher.Start(outputDirectory, 20.0), "publisher starts in an isolated directory");
    publisher.Submit(snapshot);

    const auto outputPath = outputDirectory / "zones.json";
    for (int attempt = 0; attempt < 100 && !std::filesystem::exists(outputPath); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    publisher.Stop();

    Expect(std::filesystem::exists(outputPath), "worker publishes zones.json");
    if (std::filesystem::exists(outputPath)) {
        std::ifstream input(outputPath, std::ios::binary);
        const nlohmann::json document = nlohmann::json::parse(input, nullptr, false);
        Expect(!document.is_discarded(), "published snapshot is valid JSON");
        if (!document.is_discarded()) {
            Expect(document.value("frameNumber", 999ULL) == 0ULL, "JSON preserves frame number");
            Expect(document.at("zones").size() == 2, "JSON contains every annotation zone");
            Expect(document.at("zones").at(0).value("stableId", "") == "toolbarBG--0", "JSON preserves stable IDs");
        }
    }
    Expect(!std::filesystem::exists(outputDirectory / "zones.json.tmp"), "atomic temporary file is removed");
    std::error_code cleanupError;
    std::filesystem::remove_all(outputDirectory, cleanupError);

    const auto existingOutputDirectory = std::filesystem::temp_directory_path() /
        ("aris-annotation-existing-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(existingOutputDirectory, cleanupError);
    AnnotationBridge::Publisher existingDirectoryPublisher;
    Expect(existingDirectoryPublisher.Start(existingOutputDirectory, 20.0),
           "publisher reuses the stable annotation directory across launches");
    existingDirectoryPublisher.Submit(snapshot);
    const auto existingOutputPath = existingOutputDirectory / "zones.json";
    for (int attempt = 0; attempt < 100 && !std::filesystem::exists(existingOutputPath); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    existingDirectoryPublisher.Stop();
    Expect(std::filesystem::exists(existingOutputPath),
           "reused annotation directory receives a fresh snapshot");
    std::filesystem::remove_all(existingOutputDirectory, cleanupError);

    const auto pressureDirectory = std::filesystem::temp_directory_path() /
        ("aris-annotation-pressure-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    AnnotationBridge::Publisher pressurePublisher;
    Expect(pressurePublisher.Start(pressureDirectory, 1000.0), "pressure publisher starts");

    ThemeInspector::AnnotationFrameSnapshot pressureSnapshot;
    pressureSnapshot.displayWidth = 2560.0f;
    pressureSnapshot.displayHeight = 1369.0f;
    pressureSnapshot.zones.reserve(500);
    for (int zoneIndex = 0; zoneIndex < 500; ++zoneIndex) {
        ThemeInspector::AnnotationZoneSnapshot zone;
        zone.stableId = "pressure.zone--" + std::to_string(zoneIndex);
        zone.key = "pressure.zone";
        zone.displayName = "Pressure zone";
        zone.sourceAnchor = "tests/test_annotation_bridge.cpp:pressure";
        zone.minX = static_cast<float>(zoneIndex % 100);
        zone.minY = static_cast<float>(zoneIndex / 100);
        zone.maxX = zone.minX + 10.0f;
        zone.maxY = zone.minY + 10.0f;
        zone.area = 100.0f;
        zone.order = zoneIndex;
        pressureSnapshot.zones.push_back(std::move(zone));
    }
    for (std::uint64_t frame = 0; frame < 300; ++frame) {
        pressureSnapshot.frameNumber = frame;
        pressurePublisher.Submit(pressureSnapshot);
    }
    pressurePublisher.Stop();

    const auto pressurePath = pressureDirectory / "zones.json";
    Expect(std::filesystem::exists(pressurePath), "pressure run publishes one latest snapshot file");
    if (std::filesystem::exists(pressurePath)) {
        std::ifstream pressureInput(pressurePath, std::ios::binary);
        const nlohmann::json pressureDocument = nlohmann::json::parse(pressureInput, nullptr, false);
        Expect(!pressureDocument.is_discarded(), "pressure output remains valid JSON");
        if (!pressureDocument.is_discarded()) {
            Expect(pressureDocument.value("frameNumber", 0ULL) == 299ULL,
                   "latest-only publication preserves the newest submitted frame");
            Expect(pressureDocument.at("zones").size() == 500,
                   "pressure output preserves all 500 zones");
        }
        Expect(std::filesystem::file_size(pressurePath) < 2U * 1024U * 1024U,
               "latest-only output remains bounded");
    }
    Expect(!std::filesystem::exists(pressureDirectory / "zones.json.tmp"),
           "pressure run leaves no temporary file");
    std::filesystem::remove_all(pressureDirectory, cleanupError);
    return failures == 0 ? 0 : 1;
}
