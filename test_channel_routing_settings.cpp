// Layer 1 + 2: Channel Routing settings persistence (F-033)
// Validates: defaults, INI round-trip, boundary clamping.
// No ImGui/OpenGL deps — mirrors the save/load logic from main.cpp.
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <cmath>

static int  failed = 0;

#define PASS(msg) std::cout << "PASS: " << (msg) << "\n"
#define FAIL(msg) do { std::cerr << "FAIL: " << (msg) << "\n"; failed++; } while(0)
#define CHECK(cond, msg) do { if(cond) PASS(msg); else FAIL(msg); } while(0)

// Mirrors the AppSettings multichannel fields
struct ChannelRoutingSettings {
    int   monitorLayout    = 0;      // 0=Auto … 6=Binaural
    int   downmixMode      = 0;      // 0=Auto, 1=AlwaysStereo, 2=Native
    bool  binauralMonitor  = false;
    float downmixCenterDb  = -3.0f;
    float downmixSurroundDb= -3.0f;
};

struct HandoffProfileSettings {
    std::string defaultExportPath;
    int  defaultTransferFolderStrategy = 0;
    int  defaultTransferFileType = 0;
    int  defaultTransferExistingBehavior = 0;
    bool defaultTransferUseSourceFormat = true;
    bool defaultTransferEmbedMetadata = true;
};

// Mirrors Save logic
static std::string serializeSettings(const ChannelRoutingSettings& s) {
    std::ostringstream out;
    out << "monitorLayout="    << s.monitorLayout    << "\n";
    out << "downmixMode="      << s.downmixMode      << "\n";
    out << "binauralMonitor="  << (s.binauralMonitor ? "1" : "0") << "\n";
    out << "downmixCenterDb="  << s.downmixCenterDb  << "\n";
    out << "downmixSurroundDb="<< s.downmixSurroundDb<< "\n";
    return out.str();
}

// Mirrors Load logic (ParseInt/ParseFloat/ParseBool inlined)
static std::unordered_map<std::string, std::string> parseIni(const std::string& data) {
    std::unordered_map<std::string, std::string> kv;
    std::istringstream ss(data);
    std::string line;
    while (std::getline(ss, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        kv[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return kv;
}

static std::string serializeHandoffSettings(const HandoffProfileSettings& s) {
    std::ostringstream out;
    out << "defaultExportPath=" << s.defaultExportPath << "\n";
    out << "defaultTransferFolderStrategy=" << s.defaultTransferFolderStrategy << "\n";
    out << "defaultTransferFileType=" << s.defaultTransferFileType << "\n";
    out << "defaultTransferExistingBehavior=" << s.defaultTransferExistingBehavior << "\n";
    out << "defaultTransferUseSourceFormat=" << (s.defaultTransferUseSourceFormat ? "1" : "0") << "\n";
    out << "defaultTransferEmbedMetadata=" << (s.defaultTransferEmbedMetadata ? "1" : "0") << "\n";
    return out.str();
}

static HandoffProfileSettings loadHandoffSettings(const std::unordered_map<std::string, std::string>& kv) {
    HandoffProfileSettings s;

    auto parseInt = [&](const std::string& key, int def) -> int {
        auto it = kv.find(key);
        if (it == kv.end()) return def;
        try { return std::stoi(it->second); } catch(...) { return def; }
    };
    auto parseBool = [&](const std::string& key, bool def) -> bool {
        auto it = kv.find(key);
        if (it == kv.end()) return def;
        return it->second == "1";
    };

    auto itPath = kv.find("defaultExportPath");
    if (itPath != kv.end()) s.defaultExportPath = itPath->second;
    s.defaultTransferFolderStrategy = std::clamp(parseInt("defaultTransferFolderStrategy", 0), 0, 16);
    s.defaultTransferFileType = std::clamp(parseInt("defaultTransferFileType", 0), 0, 4);
    s.defaultTransferExistingBehavior = std::clamp(parseInt("defaultTransferExistingBehavior", 0), 0, 3);
    s.defaultTransferUseSourceFormat = parseBool("defaultTransferUseSourceFormat", true);
    s.defaultTransferEmbedMetadata = parseBool("defaultTransferEmbedMetadata", true);
    return s;
}

static ChannelRoutingSettings loadSettings(const std::unordered_map<std::string, std::string>& kv) {
    ChannelRoutingSettings s;

    auto parseInt = [&](const std::string& key, int def) -> int {
        auto it = kv.find(key);
        if (it == kv.end()) return def;
        try { return std::stoi(it->second); } catch(...) { return def; }
    };
    auto parseBool = [&](const std::string& key, bool def) -> bool {
        auto it = kv.find(key);
        if (it == kv.end()) return def;
        return it->second == "1";
    };
    auto parseFloat = [&](const std::string& key, float def) -> float {
        auto it = kv.find(key);
        if (it == kv.end()) return def;
        try { return std::stof(it->second); } catch(...) { return def; }
    };

    s.monitorLayout    = std::clamp(parseInt("monitorLayout",    0), 0, 6);
    s.downmixMode      = std::clamp(parseInt("downmixMode",      0), 0, 2);
    s.binauralMonitor  = parseBool("binauralMonitor",  false);
    s.downmixCenterDb  = parseFloat("downmixCenterDb",  -3.0f);
    s.downmixSurroundDb= parseFloat("downmixSurroundDb",-3.0f);
    return s;
}

// Tests

static void testDefaults() {
    ChannelRoutingSettings s;
    CHECK(s.monitorLayout    == 0,     "default monitorLayout == 0 (Auto)");
    CHECK(s.downmixMode      == 0,     "default downmixMode == 0 (Auto)");
    CHECK(s.binauralMonitor  == false, "default binauralMonitor == false");
    CHECK(std::fabs(s.downmixCenterDb   - (-3.0f)) < 0.001f, "default downmixCenterDb == -3.0");
    CHECK(std::fabs(s.downmixSurroundDb - (-3.0f)) < 0.001f, "default downmixSurroundDb == -3.0");
}

static void testRoundTrip() {
    ChannelRoutingSettings orig;
    orig.monitorLayout    = 5;      // 7.1.4
    orig.downmixMode      = 2;      // Native
    orig.binauralMonitor  = true;
    orig.downmixCenterDb  = -4.5f;
    orig.downmixSurroundDb= -6.0f;

    std::string ini = serializeSettings(orig);
    auto kv = parseIni(ini);
    ChannelRoutingSettings loaded = loadSettings(kv);

    CHECK(loaded.monitorLayout    == 5,     "round-trip monitorLayout == 5");
    CHECK(loaded.downmixMode      == 2,     "round-trip downmixMode == 2");
    CHECK(loaded.binauralMonitor  == true,  "round-trip binauralMonitor == true");
    CHECK(std::fabs(loaded.downmixCenterDb   - (-4.5f)) < 0.001f, "round-trip downmixCenterDb == -4.5");
    CHECK(std::fabs(loaded.downmixSurroundDb - (-6.0f)) < 0.001f, "round-trip downmixSurroundDb == -6.0");
}

static void testClamping() {
    // monitorLayout out-of-range
    {
        auto kv = parseIni("monitorLayout=99\ndownmixMode=5\n");
        auto s = loadSettings(kv);
        CHECK(s.monitorLayout == 6, "monitorLayout=99 clamped to 6");
        CHECK(s.downmixMode   == 2, "downmixMode=5 clamped to 2");
    }
    // negative values
    {
        auto kv = parseIni("monitorLayout=-3\ndownmixMode=-1\n");
        auto s = loadSettings(kv);
        CHECK(s.monitorLayout == 0, "monitorLayout=-3 clamped to 0");
        CHECK(s.downmixMode   == 0, "downmixMode=-1 clamped to 0");
    }
}

static void testMissingKeys() {
    // Empty INI → all defaults
    auto kv = parseIni("");
    auto s = loadSettings(kv);
    CHECK(s.monitorLayout    == 0,     "missing monitorLayout defaults to 0");
    CHECK(s.downmixMode      == 0,     "missing downmixMode defaults to 0");
    CHECK(s.binauralMonitor  == false, "missing binauralMonitor defaults to false");
    CHECK(std::fabs(s.downmixCenterDb   - (-3.0f)) < 0.001f, "missing downmixCenterDb defaults to -3.0");
    CHECK(std::fabs(s.downmixSurroundDb - (-3.0f)) < 0.001f, "missing downmixSurroundDb defaults to -3.0");
}

static void testBoundaryLayouts() {
    for (int i = 0; i <= 6; ++i) {
        ChannelRoutingSettings s;
        s.monitorLayout = i;
        auto kv = parseIni(serializeSettings(s));
        auto loaded = loadSettings(kv);
        CHECK(loaded.monitorLayout == i, "layout " + std::to_string(i) + " survives round-trip");
    }
}

static void testHandoffProfileDefaults() {
    HandoffProfileSettings s;
    CHECK(s.defaultExportPath.empty(), "handoff default export path starts empty");
    CHECK(s.defaultTransferFolderStrategy == 0, "handoff default folder strategy == 0");
    CHECK(s.defaultTransferFileType == 0, "handoff default file type == 0");
    CHECK(s.defaultTransferExistingBehavior == 0, "handoff default existing behavior == 0");
    CHECK(s.defaultTransferUseSourceFormat == true, "handoff defaults to source format");
    CHECK(s.defaultTransferEmbedMetadata == true, "handoff defaults to embedding metadata");
}

static void testHandoffProfileRoundTrip() {
    HandoffProfileSettings orig;
    orig.defaultExportPath = "D:/Deliveries/GameA";
    orig.defaultTransferFolderStrategy = 5;
    orig.defaultTransferFileType = 3;
    orig.defaultTransferExistingBehavior = 2;
    orig.defaultTransferUseSourceFormat = false;
    orig.defaultTransferEmbedMetadata = false;

    HandoffProfileSettings loaded = loadHandoffSettings(parseIni(serializeHandoffSettings(orig)));
    CHECK(loaded.defaultExportPath == "D:/Deliveries/GameA", "handoff export path survives round-trip");
    CHECK(loaded.defaultTransferFolderStrategy == 5, "handoff folder strategy survives round-trip");
    CHECK(loaded.defaultTransferFileType == 3, "handoff file type survives round-trip");
    CHECK(loaded.defaultTransferExistingBehavior == 2, "handoff existing behavior survives round-trip");
    CHECK(!loaded.defaultTransferUseSourceFormat, "handoff source-format flag survives round-trip");
    CHECK(!loaded.defaultTransferEmbedMetadata, "handoff embed-metadata flag survives round-trip");
}

static void testHandoffProfileClamping() {
    auto s = loadHandoffSettings(parseIni(
        "defaultTransferFolderStrategy=999\n"
        "defaultTransferFileType=999\n"
        "defaultTransferExistingBehavior=999\n"));
    CHECK(s.defaultTransferFolderStrategy == 16, "handoff folder strategy clamps high");
    CHECK(s.defaultTransferFileType == 4, "handoff file type clamps high");
    CHECK(s.defaultTransferExistingBehavior == 3, "handoff existing behavior clamps high");

    s = loadHandoffSettings(parseIni(
        "defaultTransferFolderStrategy=-5\n"
        "defaultTransferFileType=-2\n"
        "defaultTransferExistingBehavior=-1\n"));
    CHECK(s.defaultTransferFolderStrategy == 0, "handoff folder strategy clamps low");
    CHECK(s.defaultTransferFileType == 0, "handoff file type clamps low");
    CHECK(s.defaultTransferExistingBehavior == 0, "handoff existing behavior clamps low");
}

int main() {
    std::cout << "=== TestChannelRoutingSettings ===\n";
    testDefaults();
    testRoundTrip();
    testClamping();
    testMissingKeys();
    testBoundaryLayouts();
    testHandoffProfileDefaults();
    testHandoffProfileRoundTrip();
    testHandoffProfileClamping();

    if (failed == 0)
        std::cout << "\nAll tests PASSED\n";
    else
        std::cerr << "\n" << failed << " test(s) FAILED\n";

    return failed == 0 ? 0 : 1;
}
