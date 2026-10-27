// ============================================================================
// test_reaper_bridge_stub.cpp — AUR-P1-047 Slice 2
// Load-test for reaper_harken.dll.
//
// Validates that:
//   1. The DLL exports ReaperPluginEntry (via GetProcAddress).
//   2. Calling entry(hInstance, &mock_rec) returns 1 (load success).
//   3. Calling entry(hInstance, nullptr) returns 0 (unload).
//   4. The mock rec->Register callback is invoked at least once.
//
// When AURO_REAPER_SDK_PRESENT is not defined (SDK absent), the test prints
// "Skipped" and exits 0 so CI doesn't fail on machines without the submodule.
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef AURO_REAPER_SDK_PRESENT
#include <windows.h>
#include "reaper_plugin.h"   // reaper_plugin_info_t, REAPER_PLUGIN_VERSION
#endif

// ---------------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------------
static int g_pass = 0;
static int g_fail = 0;

static void Check(bool cond, const char* desc) {
    if (cond) {
        std::printf("PASS: %s\n", desc);
        ++g_pass;
    } else {
        std::printf("FAIL: %s\n", desc);
        ++g_fail;
    }
}

// ---------------------------------------------------------------------------
// Tests (only compiled when SDK is present)
// ---------------------------------------------------------------------------

#ifdef AURO_REAPER_SDK_PRESENT

// Typedef for the plugin entry point.
using ReaperPluginEntryFn = int (*)(REAPER_PLUGIN_HINSTANCE, reaper_plugin_info_t*);

// ---------------------------------------------------------------------------
// Mock helpers — fill the fields that ReaperBridgeStub.cpp touches.
// ---------------------------------------------------------------------------

// Mock Register callback: records whether it was called.
static int g_registerCallCount = 0;
static const char* g_lastRegisteredName = nullptr;
static int MockRegister(const char* name, void* /*infostruct*/) {
    ++g_registerCallCount;
    g_lastRegisteredName = name;
    return 1; // success
}

// Mock GetFunc callback (not used by the stub, but must be non-null).
static void* MockGetFunc(const char* /*name*/) {
    return nullptr;
}

// Build a minimal reaper_plugin_info_t that looks plausible.
static reaper_plugin_info_t MakeMockRec() {
    reaper_plugin_info_t rec;
    std::memset(&rec, 0, sizeof(rec));
    rec.caller_version = REAPER_PLUGIN_VERSION;
    rec.Register       = MockRegister;
    rec.GetFunc        = MockGetFunc;
    return rec;
}

// ---------------------------------------------------------------------------
// Test body
// ---------------------------------------------------------------------------

static void Test_DllExportsReaperPluginEntry(HMODULE hDll) {
    FARPROC fn = GetProcAddress(hDll, "ReaperPluginEntry");
    Check(fn != nullptr, "DLL exports ReaperPluginEntry");
}

static void Test_LoadReturnsOne(HMODULE hDll) {
    auto entry = reinterpret_cast<ReaperPluginEntryFn>(
        GetProcAddress(hDll, "ReaperPluginEntry"));
    if (!entry) {
        Check(false, "ReaperPluginEntry obtainable for load test");
        return;
    }

    g_registerCallCount = 0;
    g_lastRegisteredName = nullptr;

    reaper_plugin_info_t rec = MakeMockRec();
    const int result = entry(GetModuleHandleW(nullptr), &rec);
    Check(result == 1, "ReaperPluginEntry(hInstance, &rec) returns 1 (load success)");
}

static void Test_RegisterCalledOnLoad(HMODULE hDll) {
    // Re-enter so register count is fresh.
    auto entry = reinterpret_cast<ReaperPluginEntryFn>(
        GetProcAddress(hDll, "ReaperPluginEntry"));
    if (!entry) {
        Check(false, "ReaperPluginEntry obtainable for Register test");
        return;
    }

    g_registerCallCount = 0;
    reaper_plugin_info_t rec = MakeMockRec();
    entry(GetModuleHandleW(nullptr), &rec);
    Check(g_registerCallCount >= 1,
          "rec->Register() invoked at least once during load");
}

static void Test_UnloadReturnsZero(HMODULE hDll) {
    auto entry = reinterpret_cast<ReaperPluginEntryFn>(
        GetProcAddress(hDll, "ReaperPluginEntry"));
    if (!entry) {
        Check(false, "ReaperPluginEntry obtainable for unload test");
        return;
    }

    // rec == nullptr signals unload.
    const int result = entry(GetModuleHandleW(nullptr), nullptr);
    Check(result == 0, "ReaperPluginEntry(hInstance, nullptr) returns 0 (unload)");
}

static void Test_CallerVersionTooOld_ReturnsZero(HMODULE hDll) {
    auto entry = reinterpret_cast<ReaperPluginEntryFn>(
        GetProcAddress(hDll, "ReaperPluginEntry"));
    if (!entry) {
        Check(false, "ReaperPluginEntry obtainable for version-check test");
        return;
    }

    reaper_plugin_info_t rec = MakeMockRec();
    rec.caller_version = 0x0001; // Ancient, below REAPER_PLUGIN_VERSION
    const int result = entry(GetModuleHandleW(nullptr), &rec);
    Check(result == 0, "ReaperPluginEntry refuses ancient caller_version (returns 0)");
}

static void RunAllTests() {
    // Find reaper_harken.dll next to this test executable.
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    // Replace exe filename with reaper_harken.dll.
    std::wstring dllPath(exePath);
    auto lastSlash = dllPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        dllPath = dllPath.substr(0, lastSlash + 1);
    }
    dllPath += L"reaper_harken.dll";

    std::printf("Loading DLL: ");
    // Print narrow version
    char narrow[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, dllPath.c_str(), -1, narrow, MAX_PATH, NULL, NULL);
    std::printf("%s\n", narrow);

    HMODULE hDll = LoadLibraryW(dllPath.c_str());
    if (!hDll) {
        const DWORD err = GetLastError();
        std::printf("FAIL: LoadLibraryW failed (error %lu)\n", err);
        std::printf("      Is reaper_harken.dll built? Run:\n");
        std::printf("      cmake --build build_dbg --config Debug --target HarkenSearchReaperBridge --parallel 1\n");
        ++g_fail;
        return;
    }

    std::printf("PASS: LoadLibraryW succeeded\n");
    ++g_pass;

    Test_DllExportsReaperPluginEntry(hDll);
    Test_LoadReturnsOne(hDll);
    Test_RegisterCalledOnLoad(hDll);
    Test_UnloadReturnsZero(hDll);
    Test_CallerVersionTooOld_ReturnsZero(hDll);

    FreeLibrary(hDll);
}

#endif // AURO_REAPER_SDK_PRESENT

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    std::printf("=== TestReaperBridgeStub ===\n");

#ifdef AURO_REAPER_SDK_PRESENT
    RunAllTests();
    std::printf("\n--- Results: %d passed, %d failed ---\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
#else
    std::printf("Skipped: AURO_REAPER_SDK_PRESENT not defined (REAPER SDK not supplied).\n");
    std::printf("Point CMake at a local checkout: -DAURO_REAPER_SDK_ROOT=<path>/reaper-sdk/sdk\n");
    return 0;
#endif
}
