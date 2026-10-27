#include "reaper_plugin.h"
#include "ReaperBridgeProtocol.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_set>
#include <vector>

static reaper_plugin_info_t* g_rec = nullptr;
static volatile bool g_pipeRunning = false;
static HANDLE g_pipeThread = nullptr;
static int g_actionCmdId = 0;
static std::wstring g_pipeName = HarkenSearch::ReaperBridge::kDefaultPipeName;

struct InsertBatchRequest {
    std::vector<std::string> filePaths;
    HANDLE pipe = INVALID_HANDLE_VALUE;
};

static std::queue<InsertBatchRequest> g_insertQueue;
static std::mutex g_queueMutex;

using fn_InsertMedia = int (*)(const char*, int);
using fn_GetCursorPosition = double (*)(void);
using fn_SetEditCurPos = void (*)(double, bool, bool);
using fn_CountMediaItems = int (*)(ReaProject*);
using fn_GetMediaItem = MediaItem* (*)(ReaProject*, int);
using fn_GetMediaItemInfo_Value = double (*)(MediaItem*, const char*);
using fn_SetMediaItemInfo_Value = bool (*)(MediaItem*, const char*, double);
using fn_GetMediaItemTrack = MediaTrack* (*)(MediaItem*);
using fn_DeleteTrackMediaItem = bool (*)(MediaTrack*, MediaItem*);
using fn_CountTracks = int (*)(ReaProject*);
using fn_GetTrack = MediaTrack* (*)(ReaProject*, int);
using fn_DeleteTrack = void (*)(MediaTrack*);
using fn_UpdateArrange = void (*)(void);
using fn_Undo_BeginBlock = void (*)(void);
using fn_Undo_EndBlock = void (*)(const char*, int);
using fn_ShowConsoleMsg = void (*)(const char*);

static fn_InsertMedia InsertMedia_fn = nullptr;
static fn_GetCursorPosition GetCursorPosition_fn = nullptr;
static fn_SetEditCurPos SetEditCurPos_fn = nullptr;
static fn_CountMediaItems CountMediaItems_fn = nullptr;
static fn_GetMediaItem GetMediaItem_fn = nullptr;
static fn_GetMediaItemInfo_Value GetMediaItemInfo_Value_fn = nullptr;
static fn_SetMediaItemInfo_Value SetMediaItemInfo_Value_fn = nullptr;
static fn_GetMediaItemTrack GetMediaItemTrack_fn = nullptr;
static fn_DeleteTrackMediaItem DeleteTrackMediaItem_fn = nullptr;
static fn_CountTracks CountTracks_fn = nullptr;
static fn_GetTrack GetTrack_fn = nullptr;
static fn_DeleteTrack DeleteTrack_fn = nullptr;
static fn_UpdateArrange UpdateArrange_fn = nullptr;
static fn_Undo_BeginBlock Undo_BeginBlock_fn = nullptr;
static fn_Undo_EndBlock Undo_EndBlock_fn = nullptr;
static fn_ShowConsoleMsg ShowConsoleMsg_fn = nullptr;

#define LOAD_API(name) *(void**)&name##_fn = g_rec->GetFunc(#name)

static bool LoadReaperAPI() {
    LOAD_API(InsertMedia);
    LOAD_API(GetCursorPosition);
    LOAD_API(SetEditCurPos);
    LOAD_API(CountMediaItems);
    LOAD_API(GetMediaItem);
    LOAD_API(GetMediaItemInfo_Value);
    LOAD_API(SetMediaItemInfo_Value);
    LOAD_API(GetMediaItemTrack);
    LOAD_API(DeleteTrackMediaItem);
    LOAD_API(CountTracks);
    LOAD_API(GetTrack);
    LOAD_API(DeleteTrack);
    LOAD_API(UpdateArrange);
    LOAD_API(Undo_BeginBlock);
    LOAD_API(Undo_EndBlock);
    LOAD_API(ShowConsoleMsg);
    return InsertMedia_fn && GetCursorPosition_fn && SetEditCurPos_fn &&
           CountMediaItems_fn && GetMediaItem_fn && GetMediaItemInfo_Value_fn &&
           SetMediaItemInfo_Value_fn && GetMediaItemTrack_fn &&
           DeleteTrackMediaItem_fn && CountTracks_fn && GetTrack_fn && DeleteTrack_fn;
}
static void PipeWriteResponse(HANDLE pipe, const std::string& message) {
    if (pipe == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(pipe, message.data(), static_cast<DWORD>(message.size()), &written, nullptr);
    FlushFileBuffers(pipe);
}

static bool ReadPipeMessage(HANDLE pipe, std::string& message) {
    message.clear();
    char buffer[4096];
    for (;;) {
        DWORD bytesRead = 0;
        const BOOL ok = ReadFile(pipe, buffer, sizeof(buffer), &bytesRead, nullptr);
        if (bytesRead > 0) message.append(buffer, bytesRead);
        if (ok) return !message.empty();
        if (GetLastError() != ERROR_MORE_DATA ||
            message.size() > HarkenSearch::ReaperBridge::kMaxProtocolMessageBytes) {
            return false;
        }
    }
}

static std::wstring ResolvePipeName() {
    std::wstring value(32768, L'\0');
    const DWORD length = GetEnvironmentVariableW(
        L"ARIS_REAPER_BRIDGE_PIPE_NAME", value.data(), static_cast<DWORD>(value.size()));
    if (length > 0 && length < value.size()) {
        value.resize(length);
        if (value.rfind(L"\\\\.\\pipe\\", 0) == 0) return value;
    }
    return HarkenSearch::ReaperBridge::kDefaultPipeName;
}

static bool Utf8FileExists(const std::string& path) {
    if (path.empty()) return false;
    const int size = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()),
        nullptr, 0);
    if (size <= 0) return false;
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()),
            wide.data(), size) != size) {
        return false;
    }
    const DWORD attributes = GetFileAttributesW(wide.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static DWORD WINAPI PipeServerThread(LPVOID) {
    constexpr DWORD kPipeBufferSize = 1024 * 1024;
    while (g_pipeRunning) {
        HANDLE pipe = CreateNamedPipeW(
            g_pipeName.c_str(), PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, kPipeBufferSize, kPipeBufferSize, 100, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            Sleep(200);
            continue;
        }
        if (!ConnectNamedPipe(pipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(pipe);
            continue;
        }

        std::string request;
        if (ReadPipeMessage(pipe, request)) {
            std::string requestedProtocol;
            std::string requestedArchitecture;
            std::string requestedManifest;
            if (HarkenSearch::ReaperBridge::ParseHelloRequest(
                    request, requestedProtocol, requestedArchitecture, requestedManifest)) {
                if (requestedProtocol != std::to_string(HarkenSearch::ReaperBridge::kProtocolVersion)) {
                    PipeWriteResponse(pipe, "ERR|PROTOCOL|" +
                        std::to_string(HarkenSearch::ReaperBridge::kProtocolVersion) + "\n");
                } else if (requestedArchitecture != HarkenSearch::ReaperBridge::CurrentArchitecture()) {
                    PipeWriteResponse(pipe, "ERR|ARCH|" +
                        HarkenSearch::ReaperBridge::CurrentArchitecture() + "\n");
                } else if (requestedManifest != HarkenSearch::ReaperBridge::kBridgeManifest) {
                    PipeWriteResponse(pipe, "ERR|MANIFEST|" +
                        std::string(HarkenSearch::ReaperBridge::kBridgeManifest) + "\n");
                } else {
                    PipeWriteResponse(pipe, HarkenSearch::ReaperBridge::BuildHelloResponse(g_actionCmdId));
                }
            } else if (HarkenSearch::ReaperBridge::TrimProtocolLine(request) == "PING") {
                PipeWriteResponse(pipe, HarkenSearch::ReaperBridge::BuildHelloResponse(g_actionCmdId));
            } else {
                std::vector<std::string> filePaths;
                std::string error;
                if (HarkenSearch::ReaperBridge::ParseInsertBatchRequest(request, filePaths, error)) {
                    std::lock_guard<std::mutex> lock(g_queueMutex);
                    g_insertQueue.push({std::move(filePaths), pipe});
                    pipe = INVALID_HANDLE_VALUE;
                } else {
                    PipeWriteResponse(pipe, "ERR|PROTOCOL|" + error + "\n");
                }
            }
        }
        if (pipe != INVALID_HANDLE_VALUE) {
            DisconnectNamedPipe(pipe);
            CloseHandle(pipe);
        }
    }
    return 0;
}

static std::unordered_set<MediaItem*> SnapshotItems() {
    std::unordered_set<MediaItem*> items;
    const int count = CountMediaItems_fn(nullptr);
    for (int index = 0; index < count; ++index) {
        if (MediaItem* item = GetMediaItem_fn(nullptr, index)) items.insert(item);
    }
    return items;
}

static std::unordered_set<MediaTrack*> SnapshotTracks() {
    std::unordered_set<MediaTrack*> tracks;
    const int count = CountTracks_fn(nullptr);
    for (int index = 0; index < count; ++index) {
        if (MediaTrack* track = GetTrack_fn(nullptr, index)) tracks.insert(track);
    }
    return tracks;
}

static std::vector<MediaItem*> FindNewItems(
    const std::unordered_set<MediaItem*>& before)
{
    std::vector<MediaItem*> result;
    const int count = CountMediaItems_fn(nullptr);
    for (int index = 0; index < count; ++index) {
        MediaItem* item = GetMediaItem_fn(nullptr, index);
        if (item && before.find(item) == before.end()) result.push_back(item);
    }
    return result;
}

static std::vector<HarkenSearch::ReaperBridge::ItemSpan> ReadSpans(
    const std::vector<MediaItem*>& items)
{
    std::vector<HarkenSearch::ReaperBridge::ItemSpan> spans;
    spans.reserve(items.size());
    for (MediaItem* item : items) {
        const double start = GetMediaItemInfo_Value_fn(item, "D_POSITION");
        const double length = GetMediaItemInfo_Value_fn(item, "D_LENGTH");
        spans.push_back({start, start + length});
    }
    return spans;
}

static void RollBackBatch(
    const std::vector<MediaItem*>& createdItems,
    const std::unordered_set<MediaTrack*>& tracksBefore,
    double originalCursor)
{
    for (auto it = createdItems.rbegin(); it != createdItems.rend(); ++it) {
        MediaItem* item = *it;
        if (item) {
            if (MediaTrack* track = GetMediaItemTrack_fn(item)) {
                DeleteTrackMediaItem_fn(track, item);
            }
        }
    }
    for (int index = CountTracks_fn(nullptr) - 1; index >= 0; --index) {
        MediaTrack* track = GetTrack_fn(nullptr, index);
        if (track && tracksBefore.find(track) == tracksBefore.end()) {
            DeleteTrack_fn(track);
        }
    }
    SetEditCurPos_fn(originalCursor, false, false);
    if (UpdateArrange_fn) UpdateArrange_fn();
}

static bool InsertBatchAtEditCursor(
    const std::vector<std::string>& filePaths,
    double& firstStart,
    double& finalEnd,
    std::string& error)
{
    error.clear();
    for (const std::string& path : filePaths) {
        if (!Utf8FileExists(path)) {
            error = "文件不存在或不可读：" + path;
            return false;
        }
    }

    const double originalCursor = GetCursorPosition_fn();
    double nextCursor = originalCursor;
    firstStart = originalCursor;
    finalEnd = originalCursor;
    const auto tracksBefore = SnapshotTracks();
    std::vector<MediaItem*> createdItems;
    if (Undo_BeginBlock_fn) Undo_BeginBlock_fn();

    for (std::size_t fileIndex = 0; fileIndex < filePaths.size(); ++fileIndex) {
        SetEditCurPos_fn(nextCursor, false, false);
        const auto itemsBefore = SnapshotItems();
        const int inserted = InsertMedia_fn(filePaths[fileIndex].c_str(), 0);
        std::vector<MediaItem*> newItems = FindNewItems(itemsBefore);
        createdItems.insert(createdItems.end(), newItems.begin(), newItems.end());
        if (inserted <= 0 || newItems.empty()) {
            error = "REAPER 插入第 " + std::to_string(fileIndex + 1) + " 个文件失败";
            RollBackBatch(createdItems, tracksBefore, originalCursor);
            if (Undo_EndBlock_fn) Undo_EndBlock_fn("Harken Search: REAPER batch rolled back", -1);
            return false;
        }

        auto placement = HarkenSearch::ReaperBridge::EvaluatePlacement(nextCursor, ReadSpans(newItems));
        if (!placement.valid) {
            error = placement.error;
            RollBackBatch(createdItems, tracksBefore, originalCursor);
            if (Undo_EndBlock_fn) Undo_EndBlock_fn("Harken Search: REAPER batch rolled back", -1);
            return false;
        }
        if (placement.correction > 0.0) {
            for (MediaItem* item : newItems) {
                const double position = GetMediaItemInfo_Value_fn(item, "D_POSITION");
                if (!SetMediaItemInfo_Value_fn(item, "D_POSITION",
                                               position + placement.correction)) {
                    error = "无法校正 REAPER 插入位置";
                    RollBackBatch(createdItems, tracksBefore, originalCursor);
                    if (Undo_EndBlock_fn) Undo_EndBlock_fn("Harken Search: REAPER batch rolled back", -1);
                    return false;
                }
            }
            placement = HarkenSearch::ReaperBridge::EvaluatePlacement(nextCursor, ReadSpans(newItems));
        }
        if (!placement.valid || placement.actualStart + 1.0e-9 < nextCursor) {
            error = "插入结果位于发送前编辑指针之前";
            RollBackBatch(createdItems, tracksBefore, originalCursor);
            if (Undo_EndBlock_fn) Undo_EndBlock_fn("Harken Search: REAPER batch rolled back", -1);
            return false;
        }
        if (fileIndex == 0) firstStart = placement.actualStart;
        finalEnd = placement.actualEnd;
        nextCursor = placement.nextCursor;
    }

    SetEditCurPos_fn(nextCursor, false, false);
    if (Undo_EndBlock_fn) Undo_EndBlock_fn("Harken Search: Insert Media Batch", -1);
    if (UpdateArrange_fn) UpdateArrange_fn();
    return true;
}

static void ProcessInsertQueue() {
    for (;;) {
        InsertBatchRequest request;
        {
            std::lock_guard<std::mutex> lock(g_queueMutex);
            if (g_insertQueue.empty()) break;
            request = std::move(g_insertQueue.front());
            g_insertQueue.pop();
        }
        double firstStart = 0.0;
        double finalEnd = 0.0;
        std::string error;
        const bool success = InsertBatchAtEditCursor(
            request.filePaths, firstStart, finalEnd, error);
        if (request.pipe != INVALID_HANDLE_VALUE) {
            if (success) {
                PipeWriteResponse(request.pipe,
                    HarkenSearch::ReaperBridge::BuildInsertBatchSuccessResponse(
                        static_cast<int>(request.filePaths.size()), firstStart, finalEnd));
            } else {
                PipeWriteResponse(request.pipe, "ERR|INSERT|" + error + "\n");
            }
            DisconnectNamedPipe(request.pipe);
            CloseHandle(request.pipe);
        }
    }
}

static bool OnAction(int command, int) {
    if (command != g_actionCmdId) return false;
    ProcessInsertQueue();
    return true;
}

static void OnTimer() { ProcessInsertQueue(); }
static bool HookCommand(int command, int flag) { return OnAction(command, flag); }

extern "C" __declspec(dllexport)
int ReaperPluginEntry(HINSTANCE, reaper_plugin_info_t* rec) {
    if (!rec) {
        g_pipeRunning = false;
        HANDLE wake = CreateFileW(g_pipeName.c_str(), GENERIC_READ | GENERIC_WRITE,
                                  0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (wake != INVALID_HANDLE_VALUE) {
            const char quit[] = "QUIT\n";
            DWORD written = 0;
            WriteFile(wake, quit, sizeof(quit) - 1, &written, nullptr);
            CloseHandle(wake);
        }
        if (g_pipeThread) {
            WaitForSingleObject(g_pipeThread, 3000);
            CloseHandle(g_pipeThread);
            g_pipeThread = nullptr;
        }
        std::lock_guard<std::mutex> lock(g_queueMutex);
        while (!g_insertQueue.empty()) {
            InsertBatchRequest& request = g_insertQueue.front();
            if (request.pipe != INVALID_HANDLE_VALUE) {
                PipeWriteResponse(request.pipe, "ERR|UNLOAD|REAPER 扩展正在卸载\n");
                DisconnectNamedPipe(request.pipe);
                CloseHandle(request.pipe);
            }
            g_insertQueue.pop();
        }
        return 0;
    }

    g_rec = rec;
    g_pipeName = ResolvePipeName();
    if (!LoadReaperAPI()) return 0;

    static gaccel_register_t action = {};
    action.accel.fVirt = 0;
    action.accel.key = 0;
    action.accel.cmd = 0;
    action.desc = "Harken Search: Insert Media Batch from Pipe";
    g_actionCmdId = rec->Register("command_id", (void*)"Harken Search: Insert Media Batch from Pipe");
    if (g_actionCmdId) {
        action.accel.cmd = static_cast<WORD>(g_actionCmdId);
        rec->Register("gaccel", &action);
    }
    rec->Register("hookcommand", (void*)HookCommand);
    rec->Register("timer", (void*)OnTimer);
    g_pipeRunning = true;
    g_pipeThread = CreateThread(nullptr, 0, PipeServerThread, nullptr, 0, nullptr);

    if (ShowConsoleMsg_fn) {
        const std::string message = "[Harken Search] REAPER bridge " +
            std::string(HarkenSearch::ReaperBridge::kBridgeVersion) + " protocol " +
            std::to_string(HarkenSearch::ReaperBridge::kProtocolVersion) + " loaded\n";
        ShowConsoleMsg_fn(message.c_str());
    }
    return g_pipeThread ? 1 : 0;
}
