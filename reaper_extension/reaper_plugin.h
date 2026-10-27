/***************************************
*** REAPER Plug-in API
**
** Copyright (C) 2006-2015, Cockos Incorporated
**
**    This software is provided 'as-is', without any express or implied
**    warranty. In no event will the authors be held liable for any damages
**    arising from the use of this software.
**
**    Permission is granted to anyone to use this software for any purpose,
**    including commercial applications, and to alter it and redistribute it
**    freely, subject to the following restrictions:
**
**    1. The origin of this software must not be misrepresented; you must not
**       claim that you wrote the original software. If you use this software
**       in a product, an acknowledgment in the product documentation would be
**       appreciated but is not required.
**    2. Altered source versions must be plainly marked as such, and must not be
**       misrepresented as being the original software.
**    3. This notice may not be removed or altered from any source distribution.
***************************************/

// Aris alteration: this is a minimal ABI subset of the official REAPER SDK
// header, limited to declarations needed by the Named Pipe + InsertMedia bridge.
// Upstream: https://github.com/justinfrankel/reaper-sdk/blob/main/sdk/reaper_plugin.h

#pragma once
#include <windows.h>

// Forward declarations used by REAPER SDK
typedef struct MediaTrack MediaTrack;
typedef struct MediaItem MediaItem;
typedef struct MediaItem_Take MediaItem_Take;
typedef struct ReaProject ReaProject;
typedef struct PCM_source PCM_source;

// The main info structure passed to ReaperPluginEntry
typedef struct reaper_plugin_info_t {
    int    caller_version;    // REAPER version code
    HWND   hwnd_main;         // Main REAPER window handle
    // Register services (actions, hooks, etc.)
    int  (*Register)(const char* name, void* infostruct);
    // Get API function pointer by name
    void*(*GetFunc)(const char* name);
} reaper_plugin_info_t;

// Accelerator registration for custom actions
typedef struct {
    ACCEL accel;        // Keyboard accelerator (fVirt, key, cmd)
    const char* desc;   // Description shown in Actions list
} gaccel_register_t;

// The entry point every REAPER extension must export
// Returns 1 on success, 0 to unload.
// When rec==NULL, plugin is being unloaded.
#ifdef __cplusplus
extern "C" {
#endif
__declspec(dllexport) int ReaperPluginEntry(HINSTANCE hInstance, reaper_plugin_info_t* rec);
#ifdef __cplusplus
}
#endif
