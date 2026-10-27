
// ReaperBridgeStub.cpp — AUR-P1-047 Slice 2
// Minimal REAPER extension DLL entry point.
//
// Purpose: ship the smallest possible extension that REAPER will load,
// register, and report as healthy. No spot/insert logic — the goal is to
// validate the install pipeline (file copy to UserPlugins/, REAPER plugin
// scan, and bridge probe check).
//
// REAPER plugin naming convention: the DLL MUST be named reaper_*.dll
// (enforced by CMake OUTPUT_NAME "reaper_harken").
//
// thirdparty/reaper-sdk uses the permissive zlib license; commercial use is allowed.
// Attribution is appreciated but not required.

#ifdef AURO_REAPER_SDK_PRESENT

#include "reaper_plugin.h"   // resolved via target_include_directories

// ReaperPluginEntry — standard REAPER extension entry point.
//
// Called by REAPER on two occasions:
//   rec != nullptr  → REAPER is loading the plugin. Perform registration.
//                     Return 1 to signal successful load; 0 to abort load.
//   rec == nullptr  → REAPER is unloading the plugin. Clean up. Return 0.

extern "C" __declspec(dllexport)
int ReaperPluginEntry(REAPER_PLUGIN_HINSTANCE /*hInstance*/,
                      reaper_plugin_info_t*    rec)
{
    if (rec == nullptr) {
        // Unload path: REAPER is shutting down or reloading extensions.
        // Nothing to tear down in this stub.
        return 0;
    }

    // Sanity-check caller_version so we bail early if REAPER is too old.
    // REAPER_PLUGIN_VERSION is 0x20E (REAPER 6.xx+). Allow any caller that
    // meets this baseline.
    if (rec->caller_version < REAPER_PLUGIN_VERSION) {
        return 0;   // refuse to load on ancient REAPER builds
    }

    // Register a no-op marker so the bridge probe can detect this extension.
    // Keep the existing probe lookup key for binary compatibility.
    // The function pointer is nullptr intentionally — probers just check
    // whether GetFunc returns non-null as an existence check.
    //
    // REAPER ignores nullptr function pointers in its API registry.
    // Use a static sentinel address so the lookup returns a non-null value.
    //       Instead we use a static sentinel byte as the "function pointer"
    //       so REAPER stores something non-null for the lookup key.
    static const char kVersionSentinel[] = "1.0.0-stub";
    // "API_*" prefix registers a host-visible function. Any non-null value is
    // accepted as the function pointer; sentinel string address suffices.
    rec->Register("API_HarkenSearchReaperBridge_Version",
                  reinterpret_cast<void*>(
                      const_cast<char*>(kVersionSentinel)));

    // Registration complete — signal successful load.
    return 1;
}

#endif // AURO_REAPER_SDK_PRESENT
