#include "ProToolsBridge.h"
#include "platform/filesystem.h"
#include "platform/pro_tools_path_encoding.h"

#if defined(AURO_PROTOOLS_PTSL_ENABLED)
#include "PTSLC_CPP/CppPTSLClient.h"
#include "PTSLC_CPP/CppPTSLRequest.h"
#include "thirdparty/nlohmann_json.hpp"
#endif

#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct RequestedFile {
    std::string stagedPathUtf8;
    std::string displayNameUtf8;
    // S4/S8 v2 metadata (zero = absent/unknown)
    std::uint32_t sourceSampleRate = 0;
    std::uint16_t channelCount = 0;
    std::uint64_t durationSamples = 0;
    std::uint64_t bwfTimeReferenceSamples = 0;
};

struct TimelineTargetAudioTrack {
    std::string id;
    std::string name;
    std::string resolutionCode;
    // M16: raw GetTrackList entry for the picked track (dump()-serialized JSON)
    // so DumpLastSendState can surface type/format/attributes to explain why
    // Spot silently fails. Populated by resolver only if a track is found.
    std::string rawTrackJson;
};

std::string ArgValue(int argc, char** argv, const char* key) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == key) {
            return argv[i + 1];
        }
    }
    return {};
}

bool ArgPresent(int argc, char** argv, const char* key) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == key) {
            return true;
        }
    }
    return false;
}

std::string Utf8PathForPtslImport(const std::string& stagedPathUtf8) {
    return Platform::ProTools::PathEncoding::ToImportPath(stagedPathUtf8);
}

bool WriteResponse(const std::string& path, const HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    if (path.empty()) {
        return false;
    }
    return Platform::FileSystem::WriteFileText(
        path, HarkenSearch::ProTools::BuildLivePtslSpotResponseJson(response));
}

std::string ReadTextFile(const std::string& path) {
    std::string text;
    if (!Platform::FileSystem::ReadFileText(path, text)) return {};
    return text;
}

std::string JsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out.push_back(ch); break;
        }
    }
    return out;
}

std::string JsonStringFieldFrom(const std::string& json, std::size_t from, const std::string& key) {
    const std::string marker = "\"" + key + "\":\"";
    const auto pos = json.find(marker, from);
    if (pos == std::string::npos) return {};
    std::string value;
    bool escape = false;
    for (std::size_t i = pos + marker.size(); i < json.size(); ++i) {
        const char ch = json[i];
        if (escape) {
            switch (ch) {
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            default: value.push_back(ch); break;
            }
            escape = false;
        } else if (ch == '\\') {
            escape = true;
        } else if (ch == '"') {
            break;
        } else {
            value.push_back(ch);
        }
    }
    return value;
}

// Returns the uint64 value of a JSON number field (unquoted), 0 if absent.
std::uint64_t JsonUInt64FieldFrom(const std::string& json, std::size_t from, const std::string& key) {
    const std::string marker = "\"" + key + "\":";
    const auto pos = json.find(marker, from);
    if (pos == std::string::npos) return 0;
    std::size_t start = pos + marker.size();
    while (start < json.size() && json[start] == ' ') ++start;
    if (start >= json.size() || json[start] < '0' || json[start] > '9') return 0;
    std::uint64_t value = 0;
    for (std::size_t i = start; i < json.size() && json[i] >= '0' && json[i] <= '9'; ++i) {
        value = value * 10 + static_cast<std::uint64_t>(json[i] - '0');
    }
    return value;
}

std::vector<RequestedFile> ParseRequestedFiles(const std::string& json) {
    std::vector<RequestedFile> files;
    std::size_t pos = 0;
    while (true) {
        pos = json.find("\"stagedPathUtf8\":\"", pos);
        if (pos == std::string::npos) break;
        RequestedFile file;
        file.stagedPathUtf8 = JsonStringFieldFrom(json, pos, "stagedPathUtf8");
        file.displayNameUtf8 = JsonStringFieldFrom(json, pos, "displayNameUtf8");
        file.sourceSampleRate = static_cast<std::uint32_t>(JsonUInt64FieldFrom(json, pos, "sourceSampleRate"));
        file.channelCount = static_cast<std::uint16_t>(JsonUInt64FieldFrom(json, pos, "channelCount"));
        file.durationSamples = JsonUInt64FieldFrom(json, pos, "durationSamples");
        file.bwfTimeReferenceSamples = JsonUInt64FieldFrom(json, pos, "bwfTimeReferenceSamples");
        if (!file.stagedPathUtf8.empty()) {
            files.push_back(file);
        }
        pos += 18;
    }
    return files;
}

HarkenSearch::ProTools::SpotMode ParseSpotModeFromJson(const std::string& json) {
    const auto pos = json.find("\"spotMode\":\"");
    if (pos == std::string::npos) return HarkenSearch::ProTools::SpotMode::AtCursor;
    return HarkenSearch::ProTools::SpotModeFromKey(JsonStringFieldFrom(json, pos, "spotMode"));
}

HarkenSearch::ProTools::TrackFallback ParseTrackFallbackFromJson(const std::string& json) {
    const auto pos = json.find("\"trackFallback\":\"");
    if (pos == std::string::npos) return HarkenSearch::ProTools::TrackFallback::AutoCreateNew;
    return HarkenSearch::ProTools::TrackFallbackFromKey(JsonStringFieldFrom(json, pos, "trackFallback"));
}

#if defined(AURO_PROTOOLS_PTSL_ENABLED)
std::string ResponseFailureMessage(const PTSLC_CPP::CppPTSLResponse& rsp) {
    std::ostringstream oss;
    if (!rsp.GetResponseErrorJson().empty()) {
        oss << rsp.GetResponseErrorJson();
    } else if (!rsp.GetResponseBodyJson().empty()) {
        oss << rsp.GetResponseBodyJson();
    } else {
        oss << "PTSL command failed with status " << static_cast<int>(rsp.GetStatus());
    }
    return oss.str();
}

bool EnsureCompleted(const PTSLC_CPP::CppPTSLResponse& rsp,
                     const char* commandName,
                     HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    if (rsp.GetStatus() == PTSLC_CPP::CommandStatusType::TStatus_Completed) {
        return true;
    }
    response.ok = false;
    response.errorCode = std::string("ptsl") + commandName + "Failed";
    response.errorMessage = ResponseFailureMessage(rsp);
    return false;
}

std::string TypedResponseFailureMessage(const PTSLC_CPP::CommandResponse& rsp) {
    if (!rsp.jsonError.empty()) {
        return rsp.jsonError;
    }
    if (!rsp.jsonBody.empty()) {
        return rsp.jsonBody;
    }
    std::ostringstream oss;
    oss << "PTSL command failed with status " << static_cast<int>(rsp.status.type);
    if (!rsp.status.typeClarification.empty()) {
        oss << ": " << rsp.status.typeClarification;
    }
    if (!rsp.errors.empty()) {
        oss << " errors=[";
        for (std::size_t i = 0; i < rsp.errors.size(); ++i) {
            if (i) oss << "; ";
            if (rsp.errors[i]) {
                oss << rsp.errors[i]->errorMessage;
            }
        }
        oss << "]";
    }
    return oss.str();
}

bool EnsureTypedCompleted(const std::shared_ptr<PTSLC_CPP::CommandResponse>& rsp,
                          const char* commandName,
                          HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    if (rsp && rsp->status.type == PTSLC_CPP::TaskStatus::TStatus_Completed) {
        return true;
    }
    response.ok = false;
    response.errorCode = std::string("ptsl") + commandName + "Failed";
    response.errorMessage = rsp ? TypedResponseFailureMessage(*rsp) : "PTSL command returned null response";
    return false;
}

PTSLC_CPP::CppPTSLResponse SendPtsl(PTSLC_CPP::CppPTSLClient& client,
                                    PTSLC_CPP::CommandId commandId,
                                    const std::string& bodyJson,
                                    const char* traceName,
                                    HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    response.ptslCommandTrace.push_back(traceName);
    PTSLC_CPP::CppPTSLRequest request(commandId, bodyJson);
    return client.SendRequest(request).get();
}

bool WaitForHostReadyBeforeRegister(PTSLC_CPP::CppPTSLClient& client,
                                    HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    response.ptslCommandTrace.push_back("HostReadyCheckBeforeRegister");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    std::string lastFailure;
    do {
        PTSLC_CPP::CppPTSLRequest request(PTSLC_CPP::CommandId::CId_HostReadyCheck, "");
        const auto rsp = client.SendRequest(request).get();
        if (rsp.GetStatus() == PTSLC_CPP::CommandStatusType::TStatus_Completed) {
            return true;
        }
        lastFailure = ResponseFailureMessage(rsp);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    } while (std::chrono::steady_clock::now() < deadline);

    response.ok = false;
    response.errorCode = "ptslHostReadyCheckBeforeRegisterFailed";
    response.errorMessage =
        "Pro Tools did not become ready for PTSL before RegisterConnection. "
        "Close any Pro Tools modal dialog/save prompt and make sure the session window is active. "
        "Last HostReadyCheck response: " + lastFailure;
    return false;
}

std::string PtslSessionCachePath() {
    const char* temp = std::getenv("TEMP");
    std::string root = (temp && *temp) ? temp : ".";
    if (!root.empty() && root.back() != '\\' && root.back() != '/') {
        root += "\\";
    }
    return root + "auro-protools-ptsl-session.txt";
}

std::string TrimAscii(std::string value) {
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }
    std::size_t first = 0;
    while (first < value.size() && (value[first] == '\r' || value[first] == '\n' || value[first] == ' ' || value[first] == '\t')) {
        ++first;
    }
    return first == 0 ? value : value.substr(first);
}

std::string ReadCachedPtslSessionId() {
    std::string cached;
    if (!Platform::FileSystem::ReadFileText(PtslSessionCachePath(), cached)) {
        return {};
    }
    return TrimAscii(cached);
}

void WriteCachedPtslSessionId(const std::string& sessionId) {
    if (sessionId.empty()) {
        return;
    }
    Platform::FileSystem::WriteFileText(PtslSessionCachePath(), sessionId);
}

void ClearCachedPtslSessionId() {
    std::remove(PtslSessionCachePath().c_str());
}

bool RegisterPtslConnection(PTSLC_CPP::CppPTSLClient& client,
                            HarkenSearch::ProTools::LivePtslSpotResponse& response,
                            bool allowSessionCacheWrite) {
    if (!WaitForHostReadyBeforeRegister(client, response)) {
        ClearCachedPtslSessionId();
        return false;
    }
    auto rsp = SendPtsl(client, PTSLC_CPP::CommandId::CId_RegisterConnection,
        R"({"company_name":"Harken Search","application_name":"HarkenSearchProToolsBridge"})",
        "RegisterConnection", response);
    if (!EnsureCompleted(rsp, "RegisterConnection", response)) {
        ClearCachedPtslSessionId();
        return false;
    }
    const std::string sessionId = client.GetSessionId();
    if (!sessionId.empty()) {
        response.sessionName = sessionId;
        if (allowSessionCacheWrite) {
            WriteCachedPtslSessionId(sessionId);
            response.ptslCommandTrace.push_back("WriteCachedSession");
        }
    }
    return true;
}

bool PreparePtslSession(PTSLC_CPP::CppPTSLClient& client,
                        HarkenSearch::ProTools::LivePtslSpotResponse& response,
                        bool allowSessionCacheWrite,
                        bool allowSessionCacheRead) {
    const std::string cachedSessionId = ReadCachedPtslSessionId();
    bool usedCachedSession = false;
    if (allowSessionCacheRead && !cachedSessionId.empty()) {
        client.SetSessionId(cachedSessionId);
        response.sessionName = cachedSessionId;
        response.ptslCommandTrace.push_back("ReuseCachedSession");
        usedCachedSession = true;
    } else if (!RegisterPtslConnection(client, response, allowSessionCacheWrite)) {
        return false;
    }

    auto rsp = SendPtsl(client, PTSLC_CPP::CommandId::CId_HostReadyCheck, "", "HostReadyCheck", response);
    if (EnsureCompleted(rsp, "HostReadyCheck", response)) {
        return true;
    }

    if (!usedCachedSession) {
        return false;
    }

    ClearCachedPtslSessionId();
    response.ptslCommandTrace.push_back("CachedSessionInvalid");
    response.errorCode.clear();
    response.errorMessage.clear();
    if (!RegisterPtslConnection(client, response, allowSessionCacheWrite)) {
        return false;
    }
    rsp = SendPtsl(client, PTSLC_CPP::CommandId::CId_HostReadyCheck, "", "HostReadyCheck", response);
    return EnsureCompleted(rsp, "HostReadyCheck", response);
}

bool SmokePtslConnection(HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    // PTSL smoke does not import audio; it only proves that Pro Tools accepts the
    // connection and HostReadyCheck before any timeline/track/new-track fallback.
    response.ptslCommandTrace.push_back("CreateClient");
    PTSLC_CPP::ClientConfig config{
        "localhost:31416",
        PTSLC_CPP::Mode::Mode_ProTools,
        PTSLC_CPP::SkipHostLaunch::No
    };
    PTSLC_CPP::CppPTSLClient client(config);

    if (!PreparePtslSession(client, response, false, false)) {
        return false;
    }

    response.ok = true;
    response.errorCode = "ptslSmoke";
    response.errorMessage = "PTSL smoke connection passed; Pro Tools accepted RegisterConnection and HostReadyCheck.";
    return true;
}

// S3: Populates response.ptVersionMajor/Minor/Revision. Non-fatal: zeros mean legacy routing in S6.
void FetchPTSLVersion(PTSLC_CPP::CppPTSLClient& client,
                      HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    const auto rsp = SendPtsl(client, PTSLC_CPP::CommandId::CId_GetPTSLVersion, "",
                              "GetPTSLVersion", response);
    if (rsp.GetStatus() != PTSLC_CPP::CommandStatusType::TStatus_Completed) {
        return;
    }
    const auto root = nlohmann::json::parse(rsp.GetResponseBodyJson(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        return;
    }
    if (root.contains("version") && root["version"].is_number_integer())
        response.ptVersionMajor = root["version"].get<std::int32_t>();
    if (root.contains("version_minor") && root["version_minor"].is_number_integer())
        response.ptVersionMinor = root["version_minor"].get<std::int32_t>();
    if (root.contains("version_revision") && root["version_revision"].is_number_integer())
        response.ptVersionRevision = root["version_revision"].get<std::int32_t>();
}

// S3: Populates response.sessionSampleRate. Non-fatal: zero means SR unknown (timecode mode uses source SR as fallback).
void FetchSessionSampleRate(PTSLC_CPP::CppPTSLClient& client,
                            HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    const auto rsp = SendPtsl(client, PTSLC_CPP::CommandId::CId_GetSessionSampleRate, "",
                              "GetSessionSampleRate", response);
    if (rsp.GetStatus() != PTSLC_CPP::CommandStatusType::TStatus_Completed) {
        return;
    }
    const auto root = nlohmann::json::parse(rsp.GetResponseBodyJson(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        return;
    }
    const auto it = root.find("sample_rate");
    if (it == root.end() || it->is_null()) {
        return;
    }
    if (it->is_number_integer()) {
        const int v = it->get<int>();
        if (v > 1) response.sessionSampleRate = v; // SRate_None=1; skip Unknown=0 and None=1
    } else if (it->is_string()) {
        static constexpr struct { const char* key; int hz; } kMap[] = {
            {"SRate_44100", 44100}, {"SR_44100", 44100},
            {"SRate_48000", 48000}, {"SR_48000", 48000},
            {"SRate_88200", 88200}, {"SR_88200", 88200},
            {"SRate_96000", 96000}, {"SR_96000", 96000},
            {"SRate_176400", 176400}, {"SR_176400", 176400},
            {"SRate_192000", 192000}, {"SR_192000", 192000},
        };
        const std::string srStr = it->get<std::string>();
        for (const auto& e : kMap) {
            if (srStr == e.key) { response.sessionSampleRate = e.hz; break; }
        }
    }
}

std::string JsonStringArray(const std::vector<std::string>& values) {
    std::string json = "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) json += ",";
        json += "\"";
        json += JsonEscape(values[i]);
        json += "\"";
    }
    json += "]";
    return json;
}

std::vector<std::string> ExtractStringListFromNamedArrays(const std::string& json,
                                                          const std::string& key) {
    std::vector<std::string> values;
    const std::string marker = "\"" + key + "\":[";
    std::size_t search = 0;
    while (true) {
        const auto start = json.find(marker, search);
        if (start == std::string::npos) break;
        const auto end = json.find(']', start + marker.size());
        if (end == std::string::npos) break;
        std::size_t pos = start + marker.size();
        while (pos < end) {
            const auto quote = json.find('"', pos);
            if (quote == std::string::npos || quote >= end) break;
            std::string value;
            bool escape = false;
            for (std::size_t i = quote + 1; i < end; ++i) {
                const char ch = json[i];
                if (escape) {
                    switch (ch) {
                    case 'n': value.push_back('\n'); break;
                    case 'r': value.push_back('\r'); break;
                    case 't': value.push_back('\t'); break;
                    default: value.push_back(ch); break;
                    }
                    escape = false;
                } else if (ch == '\\') {
                    escape = true;
                } else if (ch == '"') {
                    values.push_back(value);
                    pos = i + 1;
                    break;
                } else {
                    value.push_back(ch);
                }
            }
            if (quote == std::string::npos || pos <= quote) break;
        }
        search = end + 1;
    }
    return values;
}

std::string JsonStringValue(const nlohmann::json& object, const char* key) {
    if (!object.is_object()) {
        return {};
    }
    const auto it = object.find(key);
    if (it == object.end() || it->is_null()) {
        return {};
    }
    if (it->is_string()) {
        return it->get<std::string>();
    }
    if (it->is_number_integer()) {
        return std::to_string(it->get<long long>());
    }
    return {};
}

bool JsonBoolValue(const nlohmann::json& object, const char* key) {
    if (!object.is_object()) {
        return false;
    }
    const auto it = object.find(key);
    if (it == object.end() || it->is_null()) {
        return false;
    }
    if (it->is_boolean()) {
        return it->get<bool>();
    }
    if (it->is_number_integer()) {
        return it->get<int>() != 0;
    }
    if (it->is_string()) {
        const std::string value = it->get<std::string>();
        return value == "true" || value == "True" || value == "1";
    }
    return false;
}

bool TrackAttributeIsSet(const nlohmann::json& attributes, const char* key) {
    const std::string value = JsonStringValue(attributes, key);
    if (value.empty()) {
        return false;
    }
    // Accept any state except None/Unknown — "0"/"1" exclusion removed (not observed in practice).
    return value != "None" &&
           value != "Unknown" &&
           value != "TAState_None" &&
           value != "TAState_Unknown";
}

// S5: True when the attribute is explicitly set by the user (not auto-expanded by Pro Tools).
bool TrackAttributeIsExplicit(const nlohmann::json& attributes, const char* key) {
    const std::string value = JsonStringValue(attributes, key);
    return value == "TAState_SetExplicitly" ||
           value == "TAState_SetExplicitlyAndImplicitly" ||
           value == "SetExplicitly" ||
           value == "SetExplicitlyAndImplicitly";
}

bool TrackTypeIsAudio(const nlohmann::json& track) {
    const auto it = track.find("type");
    if (it == track.end() || it->is_null()) {
        return false;
    }
    if (it->is_number_integer()) {
        return it->get<int>() == static_cast<int>(PTSLC_CPP::TrackType::TType_Audio);
    }
    const std::string value = JsonStringValue(track, "type");
    return value == "TType_Audio" ||
           value == "TT_Audio" ||
           value == "AudioTrack" ||
           value == "Audio";
}

bool ResolveTimelineTargetAudioTrack(PTSLC_CPP::CppPTSLClient& client,
                                     TimelineTargetAudioTrack& target,
                                     HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    // Step 0 (M6): Ask PT directly for its "selected" tracks — server-side filter.
    // Works on older PT (2023.09+) where the has_edit_selection attribute is missing
    // from GetTrackList's JSON body. If PT has "Link Track and Edit Selection" enabled
    // (default), clicking anywhere in a track's timeline area marks it selected too.
    {
        const std::string selectedBody =
            R"({"track_filter_list":[{"filter":"TLFilter_Selected","is_inverted":false}],"is_filter_list_additive":true,"pagination_request":{"limit":0,"offset":0}})";
        const auto selRsp = SendPtsl(client,
                                     PTSLC_CPP::CommandId::CId_GetTrackList,
                                     selectedBody,
                                     "GetTrackListSelected",
                                     response);
        if (selRsp.GetStatus() == PTSLC_CPP::CommandStatusType::TStatus_Completed) {
            const auto selRoot = nlohmann::json::parse(selRsp.GetResponseBodyJson(), nullptr, false);
            if (!selRoot.is_discarded() && selRoot.is_object() &&
                selRoot.contains("track_list") && selRoot["track_list"].is_array()) {
                for (const auto& track : selRoot["track_list"]) {
                    if (!track.is_object() || !TrackTypeIsAudio(track)) continue;
                    const auto attributesIt = track.find("track_attributes");
                    const nlohmann::json attributes = attributesIt != track.end() && attributesIt->is_object()
                        ? *attributesIt
                        : nlohmann::json::object();
                    if (JsonBoolValue(attributes, "is_locked") ||
                        JsonBoolValue(attributes, "is_frozen") ||
                        TrackAttributeIsSet(attributes, "is_inactive")) {
                        continue;
                    }
                    // M17: PT 2024.10's TLFilter_Selected is NOT a true "only selected"
                    // filter — it returns tracks with is_selected="None" (empirically
                    // verified via M16 dump). Trust only what track_attributes.is_selected
                    // actually reports; skip anything not explicitly/implicitly selected.
                    if (!TrackAttributeIsSet(attributes, "is_selected")) {
                        continue;
                    }
                    target.id = JsonStringValue(track, "id");
                    target.name = JsonStringValue(track, "name");
                    if (!target.id.empty() && !target.name.empty()) {
                        target.resolutionCode = "ptFilterSelected";
                        target.rawTrackJson = track.dump(); // M16 diagnostic capture
                        return true;
                    }
                }
            }
        }
    }

    // Fallback: attribute-based walk over ALL tracks (needed on older PT where server-side
    // selected filter returns empty, and for the singleAudioTrackFallback tier).
    const std::string body =
        R"({"track_filter_list":[{"filter":"TLFilter_All","is_inverted":false}],"is_filter_list_additive":true,"pagination_request":{"limit":0,"offset":0}})";
    const auto rsp = SendPtsl(client,
                              PTSLC_CPP::CommandId::CId_GetTrackList,
                              body,
                              "GetTrackListForEditSelection",
                              response);
    if (!EnsureCompleted(rsp, "GetTrackListForEditSelection", response)) {
        return false;
    }

    const auto root = nlohmann::json::parse(rsp.GetResponseBodyJson(), nullptr, false);
    if (root.is_discarded() || !root.is_object() || !root.contains("track_list") || !root["track_list"].is_array()) {
        response.ok = false;
        response.insertedCount = 0;
        response.errorCode = "ptslGetTrackListForEditSelectionInvalid";
        response.errorMessage = rsp.GetResponseBodyJson().empty()
            ? "PTSL CId_GetTrackList completed but did not return a track_list body"
            : rsp.GetResponseBodyJson();
        return false;
    }

    // S5: 6-tier priority — first match wins. Multi-track selection is common; "exactly one" was too strict.
    std::vector<TimelineTargetAudioTrack> eligibleTracks;
    TimelineTargetAudioTrack editExplicit, selectedExplicit, editImplicit, selectedAny;
    bool foundEditExplicit = false, foundSelectedExplicit = false;
    bool foundEditImplicit = false, foundSelectedAny = false;

    for (const auto& track : root["track_list"]) {
        if (!track.is_object() || !TrackTypeIsAudio(track)) {
            continue;
        }
        const auto attributesIt = track.find("track_attributes");
        const nlohmann::json attributes = attributesIt != track.end() && attributesIt->is_object()
            ? *attributesIt
            : nlohmann::json::object();
        if (JsonBoolValue(attributes, "is_locked") ||
            JsonBoolValue(attributes, "is_frozen") ||
            TrackAttributeIsSet(attributes, "is_inactive")) {
            continue;
        }

        TimelineTargetAudioTrack candidate;
        candidate.id = JsonStringValue(track, "id");
        candidate.name = JsonStringValue(track, "name");
        if (candidate.id.empty() || candidate.name.empty()) {
            continue;
        }
        candidate.rawTrackJson = track.dump(); // M16 diagnostic capture
        eligibleTracks.push_back(candidate);

        // Priority 1: has_edit_selection explicitly set by user
        if (!foundEditExplicit && TrackAttributeIsExplicit(attributes, "has_edit_selection")) {
            editExplicit = candidate;
            editExplicit.resolutionCode = "editSelectionExplicit";
            foundEditExplicit = true;
        }
        // Priority 2: is_selected explicitly set by user
        if (!foundSelectedExplicit && TrackAttributeIsExplicit(attributes, "is_selected")) {
            selectedExplicit = candidate;
            selectedExplicit.resolutionCode = "selectedExplicit";
            foundSelectedExplicit = true;
        }
        // Priority 3: has_edit_selection in any set state (implicit = PT auto-expanded)
        if (!foundEditImplicit && TrackAttributeIsSet(attributes, "has_edit_selection")) {
            editImplicit = candidate;
            editImplicit.resolutionCode = "editSelectionImplicit";
            foundEditImplicit = true;
        }
        // Priority 4: is_selected in any set state
        if (!foundSelectedAny && TrackAttributeIsSet(attributes, "is_selected")) {
            selectedAny = candidate;
            selectedAny.resolutionCode = "selectedTrack";
            foundSelectedAny = true;
        }
    }

    // Pick first match across 6 tiers (tier 5 = single eligible, tier 6 = caller handles fallback)
    if (foundEditExplicit)    { target = editExplicit;    return true; }
    if (foundSelectedExplicit){ target = selectedExplicit; return true; }
    if (foundEditImplicit)    { target = editImplicit;    return true; }
    if (foundSelectedAny)     { target = selectedAny;     return true; }
    if (eligibleTracks.size() == 1) {
        target = eligibleTracks.front();
        target.resolutionCode = "singleAudioTrackFallback";
        return true;
    }

    // Tier 6: no track found — caller applies TrackFallback (AutoCreateNew handled in ExecuteSpotRequest)
    // M7+M8 diagnostic: dump raw GetTrackList body, EditModeOptions state, and TLFilter_Selected
    // response so we can see exactly why PT isn't exposing the cursor track.
    {
        const char* tempDir = std::getenv("TEMP");
        if (tempDir && *tempDir) {
            std::string dumpPath = tempDir;
            if (dumpPath.back() != '\\' && dumpPath.back() != '/') dumpPath += '\\';
            dumpPath += "auro-protools-resolver-dump.json";

            // Additional probes (all non-fatal, purely for diagnostics).
            std::string editModeOptionsBody;
            {
                HarkenSearch::ProTools::LivePtslSpotResponse scratch;
                const auto emoRsp = SendPtsl(client, PTSLC_CPP::CommandId::CId_GetEditModeOptions, "",
                                             "DiagGetEditModeOptions", scratch);
                if (emoRsp.GetStatus() == PTSLC_CPP::CommandStatusType::TStatus_Completed) {
                    editModeOptionsBody = emoRsp.GetResponseBodyJson();
                }
            }
            std::string selectedListBody;
            {
                HarkenSearch::ProTools::LivePtslSpotResponse scratch;
                const auto selRsp = SendPtsl(client, PTSLC_CPP::CommandId::CId_GetTrackList,
                    R"({"track_filter_list":[{"filter":"TLFilter_Selected","is_inverted":false}],"is_filter_list_additive":true,"pagination_request":{"limit":0,"offset":0}})",
                    "DiagGetTrackListSelected", scratch);
                if (selRsp.GetStatus() == PTSLC_CPP::CommandStatusType::TStatus_Completed) {
                    selectedListBody = selRsp.GetResponseBodyJson();
                }
            }

            std::ostringstream dump;
            if (dump) {
                dump << "{\n  \"ptVersionMajor\": " << response.ptVersionMajor
                     << ",\n  \"ptVersionMinor\": " << response.ptVersionMinor
                     << ",\n  \"eligibleAudioTracks\": " << eligibleTracks.size()
                     << ",\n  \"editModeOptions\":\n" << (editModeOptionsBody.empty() ? "null" : editModeOptionsBody)
                     << ",\n  \"trackListSelectedFilter\":\n" << (selectedListBody.empty() ? "null" : selectedListBody)
                     << ",\n  \"trackListAll\":\n" << rsp.GetResponseBodyJson()
                     << "\n}\n";
                Platform::FileSystem::WriteFileText(dumpPath, dump.str());
            }
        }
    }

    response.ok = false;
    response.insertedCount = 0;
    response.errorCode = "noTimelineTargetAudioTrack";
    std::ostringstream message;
    message << "Pro Tools did not expose a selected audio track";
    message << " (eligible audio tracks=" << eligibleTracks.size() << "). ";
    message << "Diagnostic dump: %TEMP%\\auro-protools-resolver-dump.json — share it to investigate. ";
    message << "For now, click the track NAME (left panel) in PT, or enable 'Options > Link Track and Edit Selection', then retry.";
    response.errorMessage = message.str();
    return false;
}

std::vector<std::string> ImportAudioToClipList(PTSLC_CPP::CppPTSLClient& client,
                                               const std::vector<RequestedFile>& files,
                                               HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    std::vector<std::string> paths;
    paths.reserve(files.size());
    for (const auto& file : files) {
        paths.push_back(file.stagedPathUtf8);
    }
    std::string body = "{\"file_list\":";
    body += JsonStringArray(paths);
    body += ",\"audio_operations\":\"AOperations_CopyAudio\"}";
    const auto rsp = SendPtsl(client,
                              PTSLC_CPP::CommandId::CId_ImportAudioToClipList,
                              body,
                              "ImportAudioToClipList",
                              response);
    if (!EnsureCompleted(rsp, "ImportAudioToClipList", response)) {
        return {};
    }
    std::string bodyJson = rsp.GetResponseBodyJson();
    auto clipIds = ExtractStringListFromNamedArrays(bodyJson, "clip_id_list");
    if (clipIds.empty()) {
        response.ok = false;
        response.insertedCount = 0;
        response.errorCode = "ptslImportAudioToClipListNoClips";
        response.errorMessage = bodyJson.empty()
            ? "PTSL ImportAudioToClipList completed but returned no clip IDs"
            : bodyJson;
    }
    return clipIds;
}

bool ImportAudioToClipListFallback(PTSLC_CPP::CppPTSLClient& client,
                                   const std::vector<RequestedFile>& files,
                                   const std::string& reasonCode,
                                   const std::string& reasonMessage,
                                   HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    response.ptslCommandTrace.push_back("ImportAudioToClipListFallback");
    PTSLC_CPP::ImportRequest request{};
    request.importType = PTSLC_CPP::ImportType::IType_Audio;
    request.audioOptions = PTSLC_CPP::AudioMediaOptions::AMOptions_CopyFromSource;
    request.audioData.audioOperations = PTSLC_CPP::AudioOperations::AOperations_CopyAudio;
    request.audioData.destination = PTSLC_CPP::MediaDestination::MDestination_ClipList;
    request.audioData.location = PTSLC_CPP::MediaLocation::MLocation_None;
    request.audioData.filesList.reserve(files.size());
    for (const auto& file : files) {
        request.audioData.filesList.push_back(Utf8PathForPtslImport(file.stagedPathUtf8));
    }

    const auto rsp = client.Import(request);
    if (!EnsureTypedCompleted(rsp, "ImportAudioToClipListFallback", response)) {
        return false;
    }

    response.ok = true;
    response.insertedCount = static_cast<int>(files.size());
    response.trackName = "Clip List";
    response.errorCode = reasonCode.empty() ? "clipListFallback" : reasonCode;
    response.errorMessage = reasonMessage.empty()
        ? "Imported into the Pro Tools Clip List without creating a new track."
        : reasonMessage;
    return true;
}

std::shared_ptr<PTSLC_CPP::ImportResponse> ImportFilesToNewTrackAtSamples(
    PTSLC_CPP::CppPTSLClient& client,
    const std::vector<RequestedFile>& files,
    const std::string& samplesLocation,
    HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    response.ptslCommandTrace.push_back("ImportLegacyNewTrackFallback");

    PTSLC_CPP::ImportRequest request{};
    request.importType = PTSLC_CPP::ImportType::IType_Audio;
    request.audioOptions = PTSLC_CPP::AudioMediaOptions::AMOptions_CopyFromSource;
    request.audioData.audioOperations = PTSLC_CPP::AudioOperations::AOperations_CopyAudio;
    request.audioData.destination = PTSLC_CPP::MediaDestination::MDestination_NewTrack;
    request.audioData.location = PTSLC_CPP::MediaLocation::MLocation_Spot;
    request.locationOptions = PTSLC_CPP::TrackOffsetOptions::TOOptions_Samples;
    request.locationType = PTSLC_CPP::SpotLocationType::SLType_Start;
    request.locationValue = samplesLocation;
    request.spotLocationData.locationOptions = PTSLC_CPP::TrackOffsetOptions::TOOptions_Samples;
    request.spotLocationData.locationType = PTSLC_CPP::SpotLocationType::SLType_Start;
    request.spotLocationData.locationValue = samplesLocation;
    request.audioData.filesList.reserve(files.size());
    for (const auto& file : files) {
        request.audioData.filesList.push_back(Utf8PathForPtslImport(file.stagedPathUtf8));
    }

    return client.Import(request);
}

bool ImportLegacyNewTrackFallback(PTSLC_CPP::CppPTSLClient& client,
                                  const std::vector<RequestedFile>& files,
                                  const std::string& samplesLocation,
                                  const std::string& reasonCode,
                                  const std::string& reasonMessage,
                                  HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    auto importRsp = ImportFilesToNewTrackAtSamples(client, files, samplesLocation, response);
    if (!EnsureTypedCompleted(importRsp, "ImportLegacyNewTrackFallback", response)) {
        return false;
    }
    response.ok = true;
    response.insertedCount = static_cast<int>(files.size());
    response.trackName = "New Track";
    response.errorCode = "legacyNewTrackFallback";
    response.errorMessage = reasonCode + ": " + reasonMessage +
        " Sent using the legacy Pro Tools new-track fallback to preserve DAW delivery.";
    return true;
}

bool SpotClipsByIDToSelectedTrack(PTSLC_CPP::CppPTSLClient& client,
                                  const std::vector<std::string>& clipIds,
                                  const std::string& trackId,
                                  const std::string& trackName,
                                  const std::string& samplesLocation,
                                  HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    std::string body = "{\"src_clips\":";
    body += JsonStringArray(clipIds);
    body += ",\"dst_track_id\":\"";
    body += JsonEscape(trackId);
    body += "\",\"dst_track_name\":\"";
    body += JsonEscape(trackName);
    body += "\",\"dst_location_data\":{\"location_type\":\"SLType_Start\",";
    body += "\"location\":{\"time_type\":\"TLType_Samples\",\"location\":\"";
    body += JsonEscape(samplesLocation);
    body += "\"}}}";
    const auto rsp = SendPtsl(client,
                              PTSLC_CPP::CommandId::CId_SpotClipsByID,
                              body,
                              "SpotClipsByID",
                              response);
    return EnsureCompleted(rsp, "SpotClipsByID", response);
}

// M8: Ensure "Link Track and Edit Selection" preference is enabled in PT so that
// clicking anywhere in a track's timeline area also marks that track as selected.
// This lets older PT versions expose the user's clicked track via GetTrackList /
// TLFilter_Selected.
//
// Uses CId_GetEditModeOptions (79) + CId_SetEditModeOptions (80), both @since PT 2023.09.
// Preserves all other 7 EditModeOptions booleans (get-then-set dance) to avoid
// clobbering user's PT preferences.
//
// Non-fatal: on any failure the send still proceeds (falls back to legacy new-track).
void EnableLinkTrackAndEditSelection(PTSLC_CPP::CppPTSLClient& client,
                                     HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    // Step 1: Read current EditModeOptions.
    const auto getRsp = SendPtsl(client, PTSLC_CPP::CommandId::CId_GetEditModeOptions, "",
                                 "GetEditModeOptions", response);
    if (getRsp.GetStatus() != PTSLC_CPP::CommandStatusType::TStatus_Completed) {
        return;
    }
    const auto root = nlohmann::json::parse(getRsp.GetResponseBodyJson(), nullptr, false);
    if (root.is_discarded() || !root.is_object() || !root.contains("edit_mode_options") ||
        !root["edit_mode_options"].is_object()) {
        return;
    }
    const auto& opts = root["edit_mode_options"];
    // Already on? Nothing to do.
    if (JsonBoolValue(opts, "link_track_and_edit_selection")) {
        return;
    }

    // Step 2: Send back the FULL EditModeOptions, preserving other 7 booleans, only
    // flipping link_track_and_edit_selection to true. Unset booleans default to false,
    // so we must explicitly enumerate every field.
    auto b = [&opts](const char* key) -> const char* {
        return JsonBoolValue(opts, key) ? "true" : "false";
    };
    std::string body = "{\"edit_mode_options\":{";
    body += "\"tab_to_transients\":";                body += b("tab_to_transients");                body += ",";
    body += "\"link_timeline_and_edit_selection\":"; body += b("link_timeline_and_edit_selection"); body += ",";
    body += "\"link_track_and_edit_selection\":true,"; // The one we care about.
    body += "\"insertion_follows_playback\":";       body += b("insertion_follows_playback");       body += ",";
    body += "\"automation_follows_edit\":";          body += b("automation_follows_edit");          body += ",";
    body += "\"markers_follow_edit\":";              body += b("markers_follow_edit");              body += ",";
    body += "\"mirrored_midi_editing\":";            body += b("mirrored_midi_editing");            body += ",";
    body += "\"layered_editing\":";                  body += b("layered_editing");
    body += "}}";
    const auto setRsp = SendPtsl(client, PTSLC_CPP::CommandId::CId_SetEditModeOptions, body,
                                 "SetEditModeOptionsLinkTrack", response);
    (void)setRsp; // Best-effort; ignore failure.
}

// M20: Restore the user's timeline selection (in_time / out_time / play_start_marker).
// Our SelectTracksByName + Import + Spot sequence has the side effect of clearing PT's
// edit selection to (0, 0). On the NEXT send from a persistent helper, GetTimelineSelection
// would then return 0 and clips would land at session start. Restoring at end of every
// send preserves the user's cursor across subsequent sends.
void RestoreTimelineSelectionPtsl(PTSLC_CPP::CppPTSLClient& client,
                                  const std::string& inTime,
                                  const std::string& outTime,
                                  const std::string& playStartMarker,
                                  HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    // If PT never gave us a value, don't send garbage.
    if (inTime.empty() && outTime.empty() && playStartMarker.empty()) return;
    // M22: PT 2024.10's SetTimelineSelectionRequestBody has NO `location_type`
    // field — it was added in 2025.06. Sending it caused old PT to reject the
    // whole restore request silently, and the next GetTimelineSelection returned
    // 0. Omit `location_type`; old PT defaults to samples for numeric strings.
    std::string body = "{";
    bool first = true;
    auto addStr = [&](const char* key, const std::string& v) {
        if (v.empty()) return;
        if (!first) body += ",";
        body += "\""; body += key; body += "\":\""; body += JsonEscape(v); body += "\"";
        first = false;
    };
    addStr("in_time",               inTime);
    addStr("out_time",              outTime);
    addStr("play_start_marker_time", playStartMarker);
    body += "}";
    const auto rsp = SendPtsl(client,
                              PTSLC_CPP::CommandId::CId_SetTimelineSelection,
                              body,
                              "RestoreTimelineSelection",
                              response);
    (void)rsp; // Best-effort; failure here doesn't break the completed send.
}

// M1: Programmatically select an audio track by name so subsequent Spot uses it.
// Uses CId_SelectTracksByName (73) — @since Pro Tools 2023.09.
// SelectionMode SMode_Replace clears prior selection so exactly one track is active.
bool SelectTracksByNamePtsl(PTSLC_CPP::CppPTSLClient& client,
                            const std::string& trackName,
                            HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    // M21: PT 2024.10's proto only knows the OLD enum name `SM_Replace`.
    // Sending the 2025.06+ name `SMode_Replace` is silently ignored (falls to
    // default enum value = "Unknown/Replace-nothing"), so the track never
    // gets selected. Use the old name for cross-version compatibility —
    // 2025.06+ proto has it as an alias (allow_alias=true) with the same value.
    std::string body = "{\"track_names\":[\"";
    body += JsonEscape(trackName);
    body += "\"],\"selection_mode\":\"SM_Replace\",";
    body += "\"pagination_request\":{\"limit\":0,\"offset\":0}}";
    const auto rsp = SendPtsl(client,
                              PTSLC_CPP::CommandId::CId_SelectTracksByName,
                              body,
                              "SelectTracksByName",
                              response);
    return EnsureCompleted(rsp, "SelectTracksByName", response);
}

// M2: Import a single audio file into the Pro Tools Clip List (bin).
// Uses old CId_Import (2) with MDestination_ClipList — @since Pro Tools 2022.12.
// Distinct from CId_ImportAudioToClipList (123) which is 2025.06+ only.
// Newly imported clip is auto-selected by Pro Tools, ready for CId_Spot.
//
// M12: Use the typed ImportRequest and populate spot_location_data with an empty
// SLType_Start@Samples marker. Hand-rolled JSON without location_data was rejected
// by PT 2024.10 with "PT_UnknownError: location_data" — the parser requires the
// field to be present even for ClipList destination (which ignores spot position).
bool ImportSingleFileToClipListMid(PTSLC_CPP::CppPTSLClient& client,
                                   const RequestedFile& file,
                                   HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    response.ptslCommandTrace.push_back("ImportToClipListMid");
    PTSLC_CPP::ImportRequest request{};
    request.importType = PTSLC_CPP::ImportType::IType_Audio;
    request.audioOptions = PTSLC_CPP::AudioMediaOptions::AMOptions_CopyFromSource;
    // M13: ConvertAudio (=3) instead of CopyAudio (=2) forces PT to sample-rate /
    // bit-depth convert the imported file to match session format. Without this,
    // a 44.1kHz file imported to a 48kHz session lands in the Clip List as a
    // 44.1kHz clip and PT rejects the subsequent Spot with:
    //   "Spot command cannot proceed due to incompatibilities between clip types
    //    and destination track types."
    // This error message is misleading — the real mismatch is SR/BD, not track
    // channel count. ConvertAudio guarantees the clip matches the session.
    request.audioData.audioOperations = PTSLC_CPP::AudioOperations::AOperations_ConvertAudio;
    // C++ AudioData struct only exposes the legacy `destination` field (auto-mapped
    // by PT to `audio_destination` on 2023.09+). ClipList destination = 4.
    request.audioData.destination = PTSLC_CPP::MediaDestination::MDestination_ClipList;
    // location_data is required by PT 2024.10's parser even for ClipList destination.
    // Provide a neutral SLType_Start @ samples marker; PT ignores it for ClipList.
    request.locationOptions = PTSLC_CPP::TrackOffsetOptions::TOOptions_Samples;
    request.locationType = PTSLC_CPP::SpotLocationType::SLType_Start;
    request.locationValue = "0";
    request.spotLocationData.locationOptions = PTSLC_CPP::TrackOffsetOptions::TOOptions_Samples;
    request.spotLocationData.locationType = PTSLC_CPP::SpotLocationType::SLType_Start;
    request.spotLocationData.locationValue = "0";
    request.audioData.filesList.push_back(Utf8PathForPtslImport(file.stagedPathUtf8));
    const auto rsp = client.Import(request);
    return EnsureTypedCompleted(rsp, "ImportToClipListMid", response);
}

// M3: Spot currently-selected clip(s) to the currently-selected track at a sample position.
// Uses CId_Spot (29) — @since Pro Tools 2022.12. Operates on the current selection state.
bool SpotSelectedClipsAt(PTSLC_CPP::CppPTSLClient& client,
                         const std::string& samplesLocation,
                         HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    // M14: PT 2024.10 (mid target) uses the OLD SpotLocationData schema:
    //   location_data.location_value  (string)      @field 2
    //   location_data.location_options (TrackOffset) @field 3
    // Nested TimelineLocation.location object (@field 4) is 2025.06+ only.
    // Sending only the new-schema shape triggers "PT_UnknownError: location_value".
    // Include BOTH shapes so 2023.09-2025.06 parsers accept the old fields and
    // future 2025.06+ parsers still see the new nested location — proto tolerates
    // unknown fields on deprecation boundaries.
    //
    // M21: use OLD enum NAMES:
    //   SpotLocationType.SLType_Start (2025.06+) → Start (2024.10)
    //   TrackOffsetOptions.TOOptions_Samples (2025.06+) → Samples (2024.10)
    //   TimeLocationType.TLType_Samples: only used inside the nested new-schema
    //   object which 2024.10 ignores. Kept as-is; 2025.06+ recognizes it.
    // Sending 2025.06+ names to PT 2024.10 causes silent defaulting — clips
    // land at position 0 instead of the intended sample offset.
    std::string body = "{\"location_data\":{";
    body += "\"location_type\":\"Start\",";
    body += "\"location_options\":\"Samples\",";
    body += "\"location_value\":\"";
    body += JsonEscape(samplesLocation);
    body += "\",";
    body += "\"location\":{\"time_type\":\"TLType_Samples\",\"location\":\"";
    body += JsonEscape(samplesLocation);
    body += "\"}}}";
    const auto rsp = SendPtsl(client,
                              PTSLC_CPP::CommandId::CId_Spot,
                              body,
                              "SpotSelectedClips",
                              response);
    return EnsureCompleted(rsp, "SpotSelectedClips", response);
}

// M4: Mid-tier spot path for Pro Tools 2023.09 ≤ v < 2025.06.
// Uses the trio SelectTracksByName + Import(ClipList) + Spot to place clips on the
// user's already-selected audio track WITHOUT creating a new track.
// Returns true on full success, false to trigger legacy fallback.
// Partial success (some files spotted, some failed): returns false but preserves
// insertedCount so the toast can surface "spotted X/N" via existing failure path.
// Append a labeled entry to the resolver diagnostic JSON in %TEMP%.
// so we can post-mortem which stage of the mid path failed. Writes fresh on every entry
// (last-writer-wins to keep the file small and current). Non-fatal.
void DumpMidPathDiagnostic(const std::string& stage,
                           const std::string& detail,
                           const TimelineTargetAudioTrack& target,
                           const HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    const char* tempDir = std::getenv("TEMP");
    if (!tempDir || !*tempDir) return;
    std::string path = tempDir;
    if (path.back() != '\\' && path.back() != '/') path += '\\';
    path += "auro-protools-resolver-dump.json";
    std::ostringstream out;
    out << "{\n";
    out << "  \"buildMarker\": \"M9 " << __DATE__ << " " << __TIME__ << "\",\n";
    out << "  \"ptVersionMajor\": " << response.ptVersionMajor << ",\n";
    out << "  \"ptVersionMinor\": " << response.ptVersionMinor << ",\n";
    out << "  \"pathTaken\": \"" << response.pathTaken << "\",\n";
    out << "  \"stageThatFailed\": \"" << stage << "\",\n";
    out << "  \"detail\": \"" << JsonEscape(detail) << "\",\n";
    out << "  \"resolverPickedTrackId\": \"" << JsonEscape(target.id) << "\",\n";
    out << "  \"resolverPickedTrackName\": \"" << JsonEscape(target.name) << "\",\n";
    out << "  \"resolverResolutionCode\": \"" << JsonEscape(target.resolutionCode) << "\",\n";
    out << "  \"responseErrorCode\": \"" << JsonEscape(response.errorCode) << "\",\n";
    out << "  \"responseErrorMessage\": \"" << JsonEscape(response.errorMessage) << "\",\n";
    out << "  \"ptslCommandTrace\": [";
    for (std::size_t i = 0; i < response.ptslCommandTrace.size(); ++i) {
        if (i) out << ", ";
        out << "\"" << JsonEscape(response.ptslCommandTrace[i]) << "\"";
    }
    out << "]\n}\n";
    Platform::FileSystem::WriteFileText(path, out.str());
}

// M10: unconditional per-send state dump. Called at the END of every send regardless
// Record the outcome and selected path in the last-send diagnostic JSON.
// so we can see actual ptVersionMajor/Minor, computed isModern/isMid flags, pathTaken,
// response.ok, errorCode, and full ptslCommandTrace. Answers "which branch did we take?"
// even when no diagnostic dump helper was invoked (e.g. straight to legacy new-track).
// Non-fatal.
void DumpLastSendState(const HarkenSearch::ProTools::LivePtslSpotResponse& response,
                       std::size_t requestedFileCount = 0) {
    const char* tempDir = std::getenv("TEMP");
    if (!tempDir || !*tempDir) return;
    std::string path = tempDir;
    if (path.back() != '\\' && path.back() != '/') path += '\\';
    path += "auro-protools-lastsend-dump.json";
    std::ostringstream out;
    // M12: mirror the M11 dual-scheme gate so dump flags match actual routing.
    const bool isYearScheme = response.ptVersionMajor >= 2020;
    const bool isModern = isYearScheme && (response.ptVersionMajor > 2025 ||
                          (response.ptVersionMajor == 2025 && response.ptVersionMinor >= 6));
    const bool isMid    = !isModern && (
                          (isYearScheme && (response.ptVersionMajor > 2023 ||
                              (response.ptVersionMajor == 2023 && response.ptVersionMinor >= 9))) ||
                          (!isYearScheme && response.ptVersionMajor >= 3));
    out << "{\n";
    out << "  \"buildMarker\": \"M12 " << __DATE__ << " " << __TIME__ << "\",\n";
    out << "  \"requestedFileCount\": " << requestedFileCount << ",\n";
    out << "  \"ptVersionMajor\": " << response.ptVersionMajor << ",\n";
    out << "  \"ptVersionMinor\": " << response.ptVersionMinor << ",\n";
    out << "  \"isModern\": " << (isModern ? "true" : "false") << ",\n";
    out << "  \"isMid\": " << (isMid ? "true" : "false") << ",\n";
    out << "  \"sessionSampleRate\": " << response.sessionSampleRate << ",\n";
    out << "  \"pathTaken\": \"" << JsonEscape(response.pathTaken) << "\",\n";
    out << "  \"ok\": " << (response.ok ? "true" : "false") << ",\n";
    out << "  \"insertedCount\": " << response.insertedCount << ",\n";
    out << "  \"trackWasCreated\": " << (response.trackWasCreated ? "true" : "false") << ",\n";
    out << "  \"resolvedTrackId\": \"" << JsonEscape(response.resolvedTrackId) << "\",\n";
    out << "  \"resolvedTrackName\": \"" << JsonEscape(response.resolvedTrackName) << "\",\n";
    // M16: raw GetTrackList entry for the resolved track. Embed as escaped string
    // (not a JSON sub-object) to keep the dump file simple and always valid.
    out << "  \"resolvedTrackRawJson\": \"" << JsonEscape(response.resolvedTrackRawJson) << "\",\n";
    // M19: per-clip absolute sample offsets actually used for Spot. Diagnoses whether
    // clips landed at the intended cursor position vs session start (0).
    out << "  \"perClipSampleOffsets\": [";
    for (std::size_t i = 0; i < response.perClipSampleOffsets.size(); ++i) {
        if (i) out << ", ";
        out << response.perClipSampleOffsets[i];
    }
    out << "],\n";
    out << "  \"timelineInTime\": \"" << JsonEscape(response.timelineInTime) << "\",\n";
    out << "  \"timelineOutTime\": \"" << JsonEscape(response.timelineOutTime) << "\",\n";
    out << "  \"timelinePlayStartMarker\": \"" << JsonEscape(response.timelinePlayStartMarker) << "\",\n";
    out << "  \"errorCode\": \"" << JsonEscape(response.errorCode) << "\",\n";
    out << "  \"errorMessage\": \"" << JsonEscape(response.errorMessage) << "\",\n";
    out << "  \"ptslCommandTrace\": [";
    for (std::size_t i = 0; i < response.ptslCommandTrace.size(); ++i) {
        if (i) out << ", ";
        out << "\"" << JsonEscape(response.ptslCommandTrace[i]) << "\"";
    }
    out << "]\n}\n";
    Platform::FileSystem::WriteFileText(path, out.str());
}

bool ExecuteMidSpotPath(PTSLC_CPP::CppPTSLClient& client,
                        const std::vector<RequestedFile>& files,
                        const std::vector<std::int64_t>& positions,
                        const TimelineTargetAudioTrack& target,
                        HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    response.ptslCommandTrace.push_back("MidPath");

    // Step 1: Force selection of the target track (resolver may have gone stale).
    if (!SelectTracksByNamePtsl(client, target.name, response)) {
        DumpMidPathDiagnostic("SelectTracksByName", "SelectTracksByName command failed", target, response);
        return false;
    }

    // Step 2: Per-file Import(ClipList) + Spot at position[i].
    // M15: After Import(ClipList), Pro Tools can drop the track selection and
    // Spot may then silently do nothing (returns Completed but places no clip).
    // We defensively re-select the target track right before every Spot call to
    // guarantee Spot has a target. Also re-select immediately after Import in
    // case Import itself shifted focus to the Clip List / imported clip.
    std::size_t successCount = 0;
    std::string failedAtStage;
    for (std::size_t i = 0; i < files.size(); ++i) {
        if (!ImportSingleFileToClipListMid(client, files[i], response)) {
            failedAtStage = "ImportToClipList(file " + std::to_string(i) + ": " + files[i].displayNameUtf8 + ")";
            break;
        }
        // M15: re-select track after Import — Import may have moved focus to bin.
        if (!SelectTracksByNamePtsl(client, target.name, response)) {
            failedAtStage = "ReSelectTracksByName(after Import file " + std::to_string(i) + ")";
            break;
        }
        const std::string posStr = std::to_string(positions[i]);
        if (!SpotSelectedClipsAt(client, posStr, response)) {
            failedAtStage = "SpotSelectedClips(file " + std::to_string(i) +
                            " at samples=" + posStr + ")";
            break;
        }
        ++successCount;
    }

    if (successCount < files.size()) {
        DumpMidPathDiagnostic(failedAtStage, response.errorMessage, target, response);
    }

    if (successCount == files.size()) {
        // Full success: no new track created, all clips on target track.
        response.ok = true;
        response.insertedCount = static_cast<int>(successCount);
        response.trackName = target.name;
        response.resolvedTrackId = target.id;
        response.resolvedTrackName = target.name;
        response.resolvedTrackRawJson = target.rawTrackJson; // M16
        response.trackWasCreated = false;
        response.pathTaken = "mid-2023.09+";
        response.perClipSampleOffsets = positions;
        response.errorCode.clear();
        response.errorMessage.clear();
        return true;
    }

    // Partial success: some clips landed on the target track before failure.
    // Surface via the failure path so user sees "spotted X/N" and can decide.
    if (successCount > 0) {
        response.ok = false;
        response.insertedCount = static_cast<int>(successCount);
        response.trackName = target.name;
        response.resolvedTrackId = target.id;
        response.resolvedTrackName = target.name;
        response.resolvedTrackRawJson = target.rawTrackJson; // M16
        response.trackWasCreated = false;
        response.pathTaken = "mid-2023.09+";
        response.perClipSampleOffsets.assign(positions.begin(), positions.begin() + successCount);
        response.errorCode = "midPathPartial";
        std::ostringstream msg;
        msg << "Spotted " << successCount << "/" << files.size()
            << " clips on \"" << target.name
            << "\" before an import or spot command failed. "
            << "The remaining clips were not placed.";
        response.errorMessage = msg.str();
        return true; // Do NOT fall through to legacy — partial state is preserved.
    }

    // Zero clips placed — clean failure, caller falls back to legacy new-track.
    return false;
}

// S7: Legacy per-file new-track import — each file lands at its own computed position.
// Sets response.pathTaken, trackWasCreated, perClipSampleOffsets on success.
bool ExecuteLegacyPerFileSpotPath(PTSLC_CPP::CppPTSLClient& client,
                                  const std::vector<RequestedFile>& files,
                                  const std::vector<std::int64_t>& positions,
                                  const std::string& reasonCode,
                                  const std::string& reasonMessage,
                                  HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    response.ptslCommandTrace.push_back("LegacyPerFileNewTrack");
    for (std::size_t i = 0; i < files.size(); ++i) {
        const std::string posStr = std::to_string(positions[i]);
        const auto rsp = ImportFilesToNewTrackAtSamples(client, { files[i] }, posStr, response);
        if (!EnsureTypedCompleted(rsp, "LegacyNewTrackImport", response)) {
            return false;
        }
    }
    response.ok = true;
    response.insertedCount = static_cast<int>(files.size());
    response.trackName = "New Track";
    response.trackWasCreated = true;
    response.pathTaken = "legacy-newtrack";
    response.perClipSampleOffsets = positions;
    response.errorCode = reasonCode.empty() ? "legacyNewTrack" : reasonCode;
    response.errorMessage = reasonMessage;
    return true;
}

// S6+S7: Main spot execution — version-adaptive with per-clip positions.
int ExecuteSpotRequestWithPreparedClient(PTSLC_CPP::CppPTSLClient& client,
                                         const std::vector<RequestedFile>& files,
                                         HarkenSearch::ProTools::SpotMode spotMode,
                                         HarkenSearch::ProTools::TrackFallback trackFallback,
                                         HarkenSearch::ProTools::LivePtslSpotResponse& response) {
    // S3: Version/SR detection — non-fatal; zeros → legacy routing.
    FetchPTSLVersion(client, response);
    FetchSessionSampleRate(client, response);
    // M8: turn on "Link Track and Edit Selection" preference so PT considers the
    // track that owns the edit cursor as selected. Non-fatal, best-effort.
    // Without this, PT <2025.06 does not expose the "cursor track" via GetTrackList and
    // clicks in a track's timeline area do not mark it as selected. Users would need to
    // Cmd+click track headers manually, so we enable the preference on their behalf.
    EnableLinkTrackAndEditSelection(client, response);

    // M11: PTSL SDK uses TWO version schemes returned by CId_GetPTSLVersion:
    //   • Old scheme (pre-2025.06 PT): version = small integer; e.g. PT 2024.06 → 3,
    //     PT 2024.10 → 5. `version_minor` field is not populated (defaults to 0).
    //   • New scheme (2025.06+ PT):    version = year (2025, 2026...); version_minor = month.
    // Distinguish by "is version a year?" — value ≥ 2020 means year-based.
    //
    // Routing gates:
    //   • Modern: needs ImportAudioToClipList(123) + SpotClipsByID(124) — PT ≥2025.06 only.
    //   • Mid:    needs SelectTracksByName(73) (@since 2023.09) + Import(2)/ClipList + Spot(29).
    //             In old scheme, PT 2024.06 = v3 supports these, so v3+ is the safe threshold.
    //   • Legacy: probe failed (version=0), or old scheme v<3 (pre-2023.09 approximately).
    const bool isYearScheme = response.ptVersionMajor >= 2020;
    const bool isModern = isYearScheme && (response.ptVersionMajor > 2025 ||
                          (response.ptVersionMajor == 2025 && response.ptVersionMinor >= 6));
    const bool isMid    = !isModern && (
                          // Year scheme but pre-2025.06 (shouldn't happen but be safe).
                          (isYearScheme && (response.ptVersionMajor > 2023 ||
                              (response.ptVersionMajor == 2023 && response.ptVersionMinor >= 9))) ||
                          // Old scheme: v3+ (PT 2024.06 = v3 supports SelectTracksByName @since 2023.09).
                          (!isYearScheme && response.ptVersionMajor >= 3));

    // GetTimelineSelection: baseline cursor for AtCursor/ToSelectionIn + BWF fallback.
    response.ptslCommandTrace.push_back("GetTimelineSelection");
    PTSLC_CPP::GetTimelineSelectionRequest timelineRequest{};
    timelineRequest.timeScale = PTSLC_CPP::TrackOffsetOptions::TOOptions_Samples;
    auto timelineRsp = client.GetTimelineSelection(timelineRequest);
    if (!EnsureTypedCompleted(timelineRsp, "GetTimelineSelection", response)) {
        return 7;
    }
    // M18: PT's GetTimelineSelection returns THREE positions:
    //   • in_time                 = edit selection start (0 if no explicit selection)
    //   • out_time                = edit selection end
    //   • play_start_marker_time  = playhead position (where playback would start)
    // When the user has only moved the transport playhead (no edit selection),
    // in_time returns "0" — NOT empty. Our old logic then used 0 and clips
    // landed at session start instead of the user's cursor.
    //
    // Semantics per spotMode:
    //   AtCursor      -> prefer non-zero in_time (current edit cursor/selection);
    //                    fall back to play_start_marker_time only if in_time is 0/missing.
    //   ToSelectionIn -> use in_time strictly (edit selection required).
    //   ToTimecode    -> base is per-file BWF, with this base only as missing-BWF fallback.
    // M19 diagnostic: stash raw PT-reported timeline positions for later analysis.
    response.timelineInTime = timelineRsp->inTime;
    response.timelineOutTime = timelineRsp->outTime;
    response.timelinePlayStartMarker = timelineRsp->playStartMarkerTime;
    const std::string baseSamplesStr = HarkenSearch::ProTools::SelectTimelineBaseSamples(
        spotMode,
        timelineRsp->inTime,
        timelineRsp->playStartMarkerTime);
    if (baseSamplesStr.empty()) {
        response.errorCode = "noTimelineSelection";
        response.errorMessage = "PTSL GetTimelineSelection did not return in_time or play_start_marker_time";
        return 8;
    }
    const std::int64_t basePosition = [&]() -> std::int64_t {
        try { return std::stoll(baseSamplesStr); } catch (...) { return 0; }
    }();

    // S4: Convert RequestedFile → LivePtslSpotFile for ComputeSpotPositions.
    std::vector<HarkenSearch::ProTools::LivePtslSpotFile> spotFiles;
    spotFiles.reserve(files.size());
    for (const auto& f : files) {
        HarkenSearch::ProTools::LivePtslSpotFile sf;
        sf.stagedPathUtf8 = f.stagedPathUtf8;
        sf.displayNameUtf8 = f.displayNameUtf8;
        sf.sourceSampleRate = f.sourceSampleRate;
        sf.channelCount = f.channelCount;
        sf.durationSamples = f.durationSamples;
        sf.bwfTimeReferenceSamples = f.bwfTimeReferenceSamples;
        spotFiles.push_back(sf);
    }
    const auto positions = HarkenSearch::ProTools::ComputeSpotPositions(
        spotFiles, spotMode, basePosition, response.sessionSampleRate);

// Modern path (≥2025.06)
    if (isModern) {
        response.pathTaken = "modern-2025.06+";
        TimelineTargetAudioTrack targetTrack;
        if (ResolveTimelineTargetAudioTrack(client, targetTrack, response)) {
            const auto clipIds = ImportAudioToClipList(client, files, response);
            if (!clipIds.empty()) {
                const std::size_t spotCount = std::min(clipIds.size(), positions.size());
                bool allOk = true;
                for (std::size_t i = 0; i < spotCount; ++i) {
                    if (!SpotClipsByIDToSelectedTrack(client, { clipIds[i] },
                            targetTrack.id, targetTrack.name,
                            std::to_string(positions[i]), response)) {
                        allOk = false;
                        break;
                    }
                }
                if (allOk) {
                    response.ok = true;
                    response.insertedCount = static_cast<int>(spotCount);
                    response.trackName = targetTrack.name;
                    response.resolvedTrackId = targetTrack.id;
                    response.resolvedTrackName = targetTrack.name;
                    response.resolvedTrackRawJson = targetTrack.rawTrackJson; // M16
                    response.trackWasCreated = false;
                    response.perClipSampleOffsets = positions;
                    response.errorCode.clear();
                    response.errorMessage = (targetTrack.resolutionCode == "singleAudioTrackFallback")
                        ? "Spotted to the only eligible Pro Tools audio track; no selected/edit-selection track was active."
                        : "";
                    return 0;
                }
            }
            response.ptslCommandTrace.push_back("ModernPathFailed");
        }

        // Resolver failed or spot failed → apply TrackFallback.
        if (trackFallback == HarkenSearch::ProTools::TrackFallback::AutoCreateNew) {
            const std::string code = response.errorCode.empty() ? "modernResolverFailed" : response.errorCode;
            const std::string msg  = response.errorMessage.empty()
                ? "No selected/edit-selection track found. Creating new track as fallback."
                : response.errorMessage;
            response.errorCode.clear();
            response.errorMessage.clear();
            if (ExecuteLegacyPerFileSpotPath(client, files, positions, code, msg, response)) {
                return 0;
            }
        }

        // All modern attempts failed → last-resort clip list.
        const std::string code = response.errorCode.empty() ? "modernPathExhausted" : response.errorCode;
        const std::string msg  = response.errorMessage.empty()
            ? "Modern Pro Tools spot path exhausted. Importing to Clip List."
            : response.errorMessage;
        if (ImportAudioToClipListFallback(client, files, "clipListFallbackNoTimelineTargetAudioTrack",
                code + ": " + msg, response)) {
            return 0;
        }
        return 9;
    }

// M5 Mid path (2023.09 ≤ v < 2025.06) — SelectTracksByName + Import(ClipList) + Spot
    // This is the compatibility path for PT versions that lack SpotClipsByID.
    // It places clips on the user's selected track WITHOUT creating a new one.
    if (isMid) {
        TimelineTargetAudioTrack midTarget;
        if (ResolveTimelineTargetAudioTrack(client, midTarget, response)) {
            // M16: expose the resolver's pick immediately so DumpLastSendState
            // sees the raw track JSON even when Spot silently no-ops.
            response.resolvedTrackId = midTarget.id;
            response.resolvedTrackName = midTarget.name;
            response.resolvedTrackRawJson = midTarget.rawTrackJson;
            if (ExecuteMidSpotPath(client, files, positions, midTarget, response)) {
                return 0; // Full or partial mid success — partial preserved in response.
            }
            response.ptslCommandTrace.push_back("MidPathFailed");
        }

        // Mid path resolver or execution failed → apply TrackFallback.
        if (trackFallback == HarkenSearch::ProTools::TrackFallback::AutoCreateNew) {
            const std::string code = response.errorCode.empty() ? "midResolverFailed" : response.errorCode;
            const std::string msg  = response.errorMessage.empty()
                ? "No selected/edit-selection track found. Creating new track as fallback."
                : response.errorMessage;
            response.errorCode.clear();
            response.errorMessage.clear();
            if (ExecuteLegacyPerFileSpotPath(client, files, positions, code, msg, response)) {
                return 0;
            }
        }
        // Ultimate fallback (mid): clip list.
        if (ImportAudioToClipListFallback(client, files, "clipListMidFallback",
                response.errorMessage, response)) {
            return 0;
        }
        return 12;
    }

// S7 Legacy path (<2023.09 or probe failed) — per-file new-track at computed positions
    response.pathTaken = "legacy-newtrack";
    if (ExecuteLegacyPerFileSpotPath(client, files, positions,
            "legacyPTVersion",
            "Pro Tools version is pre-2025.06 or version probe failed; using per-file new-track import.",
            response)) {
        return 0;
    }

    // Last resort: clip list.
    if (ImportAudioToClipListFallback(client, files, "clipListLegacyFallback",
            response.errorMessage, response)) {
        return 0;
    }
    return 10;
}

HarkenSearch::ProTools::LivePtslSpotResponse MakeBaseResponse() {
    HarkenSearch::ProTools::LivePtslSpotResponse response;
    response.parseOk = true;
    response.ok = false;
    response.path = HarkenSearch::ProTools::DeliveryPath::LivePtslSpot;
    response.insertedCount = 0;
    return response;
}

int RunPersistentQueueServer(const std::string& queueDirUtf8) {
    Platform::FileSystem::CreateDirectories(queueDirUtf8);

    // M9: startup marker so user can confirm which helper build is actually running.
    if (const char* tempDir = std::getenv("TEMP")) {
        std::string markerPath = tempDir;
        if (!markerPath.empty() && markerPath.back() != '\\' && markerPath.back() != '/')
            markerPath += '\\';
        markerPath += "auro-protools-helper-startup.txt";
        std::ostringstream marker;
        if (marker) {
            marker << "HarkenSearchProToolsBridge persistent helper started\n";
            marker << "Build: M9 " << __DATE__ << " " << __TIME__ << "\n";
            marker << "Compile flags: PTSL_ENABLED=1\n";
            Platform::FileSystem::WriteFileText(markerPath, marker.str());
        }
    }

    PTSLC_CPP::ClientConfig config{
        "localhost:31416",
        PTSLC_CPP::Mode::Mode_ProTools,
        PTSLC_CPP::SkipHostLaunch::No
    };
    PTSLC_CPP::CppPTSLClient client(config);
    bool registered = false;

    for (;;) {
        std::vector<std::filesystem::path> requests;
        Platform::FileSystem::DirectoryIterator iterator;
        Platform::FileSystem::DirectoryEntry entry;
        if (iterator.Open(queueDirUtf8, false, false)) {
            for (;;) {
                std::string iterationError;
                const auto status = iterator.Next(entry, &iterationError);
                if (status == Platform::FileSystem::DirectoryReadStatus::End) break;
                if (status == Platform::FileSystem::DirectoryReadStatus::Error)
                    throw std::runtime_error(iterationError);
                const auto path = std::filesystem::u8path(entry.pathUtf8);
                if (entry.isRegularFile &&
                    path.filename().u8string().find("-request.json") != std::string::npos) {
                    requests.push_back(path);
                }
            }
        }
        std::sort(requests.begin(), requests.end());
        for (const auto& requestFile : requests) {
            const std::string name = requestFile.filename().u8string();
            const std::string responseName = name.substr(0, name.find("-request.json")) + "-response.json";
            const auto responseFile = requestFile.parent_path() / std::filesystem::u8path(responseName);
            auto response = MakeBaseResponse();
            try {
                const std::string requestJson = ReadTextFile(requestFile.u8string());
                const std::vector<RequestedFile> files = ParseRequestedFiles(requestJson);
                if (files.empty()) {
                    response.errorCode = "emptyRequest";
                    response.errorMessage = "persistent Pro Tools bridge request did not contain staged files";
                    WriteResponse(responseFile.u8string(), response);
                    Platform::FileSystem::RemoveFile(requestFile.u8string());
                    continue;
                }
                const auto spotMode     = ParseSpotModeFromJson(requestJson);
                const auto trackFallback = ParseTrackFallbackFromJson(requestJson);
                if (!registered) {
                    response.ptslCommandTrace.push_back("CreatePersistentClient");
                    if (!RegisterPtslConnection(client, response, false)) {
                        WriteResponse(responseFile.u8string(), response);
                        Platform::FileSystem::RemoveFile(requestFile.u8string());
                        continue;
                    }
                    registered = true;
                } else {
                    response.sessionName = client.GetSessionId();
                    response.ptslCommandTrace.push_back("ReusePersistentClient");
                }
                auto readyRsp = SendPtsl(client, PTSLC_CPP::CommandId::CId_HostReadyCheck, "", "HostReadyCheck", response);
                if (!EnsureCompleted(readyRsp, "HostReadyCheck", response)) {
                    registered = false;
                    WriteResponse(responseFile.u8string(), response);
                    Platform::FileSystem::RemoveFile(requestFile.u8string());
                    continue;
                }
                ExecuteSpotRequestWithPreparedClient(client, files, spotMode, trackFallback, response);
                // M23: after a successful send, advance PT's edit cursor to the END
                // of the last-spotted clip, so the next send butt-joins onto the tail
                // instead of overlaying at the same start position. On failure or when
                // we can't compute an end position, fall back to M20 behavior:
                // restore the user's original timeline selection so at least the next
                // send reads a sensible cursor (not 0).
                std::int64_t advancedCursor = 0;
                bool advance = response.ok && !response.perClipSampleOffsets.empty() &&
                               !files.empty() && response.sessionSampleRate > 0 &&
                               files.back().sourceSampleRate > 0 &&
                               files.back().durationSamples > 0;
                if (advance) {
                    const std::int64_t lastStart = response.perClipSampleOffsets.back();
                    const std::int64_t scaled = static_cast<std::int64_t>(
                        (static_cast<std::uint64_t>(files.back().durationSamples) *
                         static_cast<std::uint64_t>(response.sessionSampleRate)) /
                        static_cast<std::uint64_t>(files.back().sourceSampleRate));
                    advancedCursor = lastStart + scaled;
                }
                if (advance) {
                    const std::string endStr = std::to_string(advancedCursor);
                    // Set both in_time/out_time and play_start_marker to the clip end,
                    // so AtCursor mode on the next send picks it up regardless of which
                    // field it prefers.
                    RestoreTimelineSelectionPtsl(client, endStr, endStr, endStr, response);
                    response.ptslCommandTrace.push_back("AdvanceCursorToClipEnd");
                } else {
                    // M20 fallback: restore original (do not leave PT at 0/0).
                    RestoreTimelineSelectionPtsl(client,
                        response.timelineInTime,
                        response.timelineOutTime,
                        response.timelinePlayStartMarker,
                        response);
                }
                DumpLastSendState(response, files.size()); // M10 (inside try — files in scope)
            } catch (const std::exception& ex) {
                registered = false;
                response.ok = false;
                response.errorCode = response.ptslCommandTrace.empty() ? "ptslPersistentServerException" : "ptslException";
                response.errorMessage = ex.what();
                DumpLastSendState(response); // M10 (exception path — file count unknown)
            }
            WriteResponse(responseFile.u8string(), response);
            Platform::FileSystem::RemoveFile(requestFile.u8string());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
#endif

} // namespace

int main(int argc, char** argv) {
    const bool smokeOnly = ArgPresent(argc, argv, "--smoke");
    const bool serverMode = ArgPresent(argc, argv, "--server");
    const std::string requestPath = ArgValue(argc, argv, "--request");
    const std::string responsePath = ArgValue(argc, argv, "--response");
    const std::string queueDir = ArgValue(argc, argv, "--queue");

    HarkenSearch::ProTools::LivePtslSpotResponse response;
    response.parseOk = true;
    response.ok = false;
    response.path = HarkenSearch::ProTools::DeliveryPath::LivePtslSpot;
    response.insertedCount = 0;

    if (serverMode && queueDir.empty()) {
        response.errorCode = "badArguments";
        response.errorMessage = "HarkenSearchProToolsBridge --server requires --queue";
        return 2;
    }
    if ((!smokeOnly && !serverMode && requestPath.empty()) || (!serverMode && responsePath.empty())) {
        response.errorCode = "badArguments";
        response.errorMessage = smokeOnly
            ? "HarkenSearchProToolsBridge --smoke requires --response"
            : "HarkenSearchProToolsBridge requires --request and --response";
        WriteResponse(responsePath, response);
        return 2;
    }

#if defined(AURO_PROTOOLS_PTSL_ENABLED)
    if (serverMode) {
        try {
            return RunPersistentQueueServer(queueDir);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "HarkenSearchProToolsBridge persistent server failed: %s\n", e.what());
            return 13;
        }
    }

    if (smokeOnly) {
        try {
            if (SmokePtslConnection(response)) {
                WriteResponse(responsePath, response);
                return 0;
            }
            WriteResponse(responsePath, response);
            return response.errorCode == "ptslRegisterConnectionFailed" ? 5 : 6;
        } catch (const std::exception& e) {
            response.ok = false;
            response.errorCode = "ptslSmokeException";
            response.errorMessage = e.what();
            WriteResponse(responsePath, response);
            return 11;
        }
    }

    const std::string requestJson = ReadTextFile(requestPath);
    const std::vector<RequestedFile> files = ParseRequestedFiles(requestJson);
    if (files.empty()) {
        response.errorCode = "emptyRequest";
        response.errorMessage = "live_ptsl_spot request did not contain staged files";
        WriteResponse(responsePath, response);
        return 4;
    }
    const auto spotMode      = ParseSpotModeFromJson(requestJson);
    const auto trackFallback = ParseTrackFallbackFromJson(requestJson);

    try {
        response.ptslCommandTrace.push_back("CreateClient");
        PTSLC_CPP::ClientConfig config{
            "localhost:31416",
            PTSLC_CPP::Mode::Mode_ProTools,
            PTSLC_CPP::SkipHostLaunch::No
        };
        PTSLC_CPP::CppPTSLClient client(config);

        // One-shot mode is retained for compatibility, but does not use the
        // cross-process cached session id: live evidence showed SetSessionId on
        // a new helper can be refused by Pro Tools after the first import.
        if (!PreparePtslSession(client, response, false, false)) {
            WriteResponse(responsePath, response);
            return response.errorCode == "ptslRegisterConnectionFailed" ? 5 : 6;
        }

        const int rc = ExecuteSpotRequestWithPreparedClient(client, files, spotMode, trackFallback, response);
        DumpLastSendState(response, files.size()); // M10
        WriteResponse(responsePath, response);
        return rc;
    } catch (const std::exception& ex) {
        response.ok = false;
        response.errorCode = response.ptslCommandTrace.size() == 1 ? "ptslClientCreateFailed" : "ptslException";
        response.errorMessage = ex.what();
        DumpLastSendState(response, files.size()); // M10
        WriteResponse(responsePath, response);
        return 12;
    }
#else
    response.errorCode = "ptslUnavailable";
    response.errorMessage = "HarkenSearchProToolsBridge was built without a local Avid PTSL SDK; live_ptsl_spot is unavailable";
    response.ptslCommandTrace = {"RegisterConnection", "HostReadyCheck"};
    WriteResponse(responsePath, response);
    return 3;
#endif
}
