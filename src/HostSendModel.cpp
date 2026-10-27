#include "HostSendModel.h"
#include "platform/filesystem.h"
#include "platform/system.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace HarkenSearch::HostSend {

namespace {

std::filesystem::path Utf8Path(const std::string& value) {
    return std::filesystem::u8path(value);
}

std::string ToUtf8(const std::filesystem::path& path) {
    return path.u8string();
}

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch - 'A' + 'a');
        }
        return static_cast<char>(ch);
    });
    return value;
}

std::string SanitizeAsciiLabel(std::string value) {
    if (value.empty()) {
        return "audio";
    }
    for (char& ch : value) {
        const bool isAsciiAlphaNum =
            (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9');
        if (!isAsciiAlphaNum) {
            ch = '_';
        }
    }
    while (!value.empty() && value.front() == '_') {
        value.erase(value.begin());
    }
    while (!value.empty() && value.back() == '_') {
        value.pop_back();
    }
    if (value.empty()) {
        return "audio";
    }
    return value;
}

std::string SanitizeFileNamePreservingUtf8(std::string value) {
    if (value.empty()) {
        return "audio";
    }
    for (char& ch : value) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        const bool isControl = uch < 0x20;
        const bool isWindowsInvalid =
            ch == '<' || ch == '>' || ch == ':' || ch == '"' ||
            ch == '/' || ch == '\\' || ch == '|' || ch == '?' || ch == '*';
        if (isControl || isWindowsInvalid) {
            ch = '_';
        }
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '.')) {
        value.pop_back();
    }
    while (!value.empty() && (value.front() == ' ')) {
        value.erase(value.begin());
    }
    return value.empty() ? "audio" : value;
}

bool EndsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool StripSuffixOnce(std::string& value, const std::string& suffix) {
    if (!EndsWith(value, suffix) || value.size() <= suffix.size()) {
        return false;
    }
    value.resize(value.size() - suffix.size());
    return true;
}

bool StripChannelSoloSuffixOnce(std::string& value) {
    const auto underscore = value.rfind('_');
    if (underscore == std::string::npos || underscore + 2 >= value.size()) {
        return false;
    }
    if (value[underscore + 1] != 'C') {
        return false;
    }
    for (std::size_t i = underscore + 2; i < value.size(); ++i) {
        if (value[i] < '0' || value[i] > '9') {
            return false;
        }
    }
    value.resize(underscore);
    return !value.empty();
}

std::string StripHarkenSearchProcessingSuffixesFromStem(const std::string& stem) {
    std::string stripped = stem;
    bool changed = true;
    while (changed && !stripped.empty()) {
        changed = false;
        for (const std::string& suffix : {
                 std::string("_region"),
                 std::string("_pitch"),
                 std::string("_vol"),
                 std::string("_pan"),
                 std::string("_rev"),
                 std::string("_mono"),
                 std::string("_center"),
                 std::string("_Org"),
                 std::string("_org"),
                 std::string("_processed"),
                 std::string("_proc"),
                 std::string("_L"),
                 std::string("_R"),
                 std::string("_C"),
                 std::string("_LFE"),
                 std::string("_LS"),
                 std::string("_RS"),
                 std::string("_BL"),
                 std::string("_BR"),
             }) {
            if (StripSuffixOnce(stripped, suffix)) {
                changed = true;
                break;
            }
        }
        if (!changed && StripChannelSoloSuffixOnce(stripped)) {
            changed = true;
        }
    }
    return stripped.empty() ? stem : stripped;
}

std::string MaybeStripHarkenSearchProcessingSuffixesFromFileName(std::string fileName, bool keepHarkenSearchProcessingSuffix) {
    if (keepHarkenSearchProcessingSuffix || fileName.empty()) {
        return fileName;
    }
    std::filesystem::path path = Utf8Path(fileName);
    const std::string stem = path.stem().u8string();
    const std::string ext = path.extension().u8string();
    const std::string strippedStem = StripHarkenSearchProcessingSuffixesFromStem(stem);
    if (strippedStem == stem) {
        return fileName;
    }
    return strippedStem + ext;
}

std::string JsonEscape(const std::string& input) {
    std::ostringstream oss;
    for (unsigned char ch : input) {
        switch (ch) {
        case '\\': oss << "\\\\"; break;
        case '"':  oss << "\\\""; break;
        case '\b': oss << "\\b"; break;
        case '\f': oss << "\\f"; break;
        case '\n': oss << "\\n"; break;
        case '\r': oss << "\\r"; break;
        case '\t': oss << "\\t"; break;
        default:
            if (ch < 0x20) {
                static const char* hex = "0123456789abcdef";
                oss << "\\u00"
                    << hex[(ch >> 4) & 0x0f]
                    << hex[ch & 0x0f];
            } else {
                oss << static_cast<char>(ch);
            }
            break;
        }
    }
    return oss.str();
}

std::string BuildStagedFileName(const SourceItem& item,
                                std::size_t index,
                                std::size_t totalCount,
                                HostId host,
                                bool keepHarkenSearchProcessingSuffix) {
    std::filesystem::path source = Utf8Path(item.sourcePathUtf8);
    const std::string stem = source.stem().u8string();
    const std::string ext = LowerAscii(source.extension().u8string());

    if (host == HostId::ProTools) {
        std::string base = source.filename().u8string();
        if (base.empty()) {
            base = item.displayLabel.empty() ? (std::string("audio") + ext) : (item.displayLabel + ext);
        }
        base = MaybeStripHarkenSearchProcessingSuffixesFromFileName(base, keepHarkenSearchProcessingSuffix);
        base = SanitizeFileNamePreservingUtf8(base);
        if (totalCount <= 1 || base.empty()) {
            return base;
        }
        std::filesystem::path basePath = Utf8Path(base);
        const std::string baseStem = basePath.stem().u8string();
        const std::string baseExt = basePath.extension().u8string();
        std::ostringstream pt;
        pt << (baseStem.empty() ? "audio" : baseStem) << "_" << index;
        if (!baseExt.empty()) {
            pt << baseExt;
        }
        return pt.str();
    }

    std::ostringstream oss;
    const std::string labelStem = StripHarkenSearchProcessingSuffixesFromStem(item.displayLabel.empty() ? stem : item.displayLabel);
    const std::string stagedStem = keepHarkenSearchProcessingSuffix
        ? (item.displayLabel.empty() ? stem : item.displayLabel)
        : labelStem;
    oss << index << "_" << SanitizeAsciiLabel(stagedStem);
    if (!ext.empty()) {
        oss << ext;
    }
    return oss.str();
}

} // namespace

const char* HostKey(HostId host) {
    switch (host) {
    case HostId::Nuendo: return "nuendo";
    case HostId::ProTools: return "protools";
    }
    return "unknown";
}

const char* AdapterPathKey(AdapterPath path) {
    switch (path) {
    case AdapterPath::SteinbergBridge: return "steinberg_bridge";
    case AdapterPath::ProToolsImportAutomation: return "protools_import_automation";
    case AdapterPath::ProToolsPtslBridge: return "protools_ptsl_bridge";
    }
    return "unknown";
}

const char* DeliveryModeKey(DeliveryMode mode) {
    switch (mode) {
    case DeliveryMode::BringIntoHost: return "bring_into_host";
    case DeliveryMode::SpotToHost: return "spot_to_host";
    case DeliveryMode::ImportToClipsList: return "import_to_clips_list";
    }
    return "unknown";
}

const char* SourceSelectionKey(SourceSelectionKind kind) {
    switch (kind) {
    case SourceSelectionKind::WholeFile: return "whole_file";
    case SourceSelectionKind::RegionRender: return "region_render";
    }
    return "unknown";
}

AdapterPath DefaultAdapterPathForHost(HostId host) {
    switch (host) {
    case HostId::Nuendo:
        return AdapterPath::SteinbergBridge;
    case HostId::ProTools:
        return AdapterPath::ProToolsPtslBridge;
    }
    return AdapterPath::SteinbergBridge;
}

DeliveryMode DefaultDeliveryModeForHost(HostId host) {
    switch (host) {
    case HostId::Nuendo:
        return DeliveryMode::BringIntoHost;
    case HostId::ProTools:
        return DeliveryMode::SpotToHost;
    }
    return DeliveryMode::BringIntoHost;
}

std::string DefaultStagingRootUtf8() {
    const auto localAppData = Platform::ReadEnvironmentVariable("LOCALAPPDATA");
    std::filesystem::path root;
    if (localAppData.status == Platform::EnvironmentStringStatus::Success &&
        !localAppData.valueUtf8.empty()) {
        root = Utf8Path(localAppData.valueUtf8);
    } else {
        const auto temporary = Platform::FileSystem::QueryTemporaryDirectory();
        if (temporary.status != Platform::FileSystem::TemporaryDirectoryStatus::Success) {
            throw std::filesystem::filesystem_error(
                "temp_directory_path", temporary.error);
        }
        root = Utf8Path(temporary.pathUtf8);
    }
    root /= "Auro";
    root /= "HostSend";
    return ToUtf8(root);
}

StagingPlan BuildStagingPlan(const Request& request, const std::string& rootDirectoryUtf8) {
    StagingPlan plan;
    plan.rootDirectoryUtf8 = rootDirectoryUtf8.empty()
        ? DefaultStagingRootUtf8()
        : rootDirectoryUtf8;

    std::filesystem::path baseRoot = Utf8Path(plan.rootDirectoryUtf8);
    baseRoot /= HostKey(request.host);

    plan.files.reserve(request.items.size());
    for (std::size_t i = 0; i < request.items.size(); ++i) {
        const auto& item = request.items[i];
        std::filesystem::path source = Utf8Path(item.sourcePathUtf8);
        std::filesystem::path target = baseRoot / BuildStagedFileName(
            item,
            i,
            request.items.size(),
            request.host,
            request.keepHarkenSearchProcessingSuffix);

        StagingFilePlan file;
        file.sourcePathUtf8 = item.sourcePathUtf8;
        file.stagedPathUtf8 = ToUtf8(target);
        file.stagedFileName = target.filename().u8string();
        file.extensionLower = LowerAscii(source.extension().u8string());
        plan.files.push_back(std::move(file));
    }

    return plan;
}

std::string BuildPayloadJson(const Job& job) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"cmd\":\"insert_audio\",";
    oss << "\"host\":\"" << HostKey(job.host) << "\",";
    oss << "\"adapter\":\"" << AdapterPathKey(job.adapterPath) << "\",";
    oss << "\"mode\":\"" << DeliveryModeKey(job.mode) << "\",";
    oss << "\"copyToProject\":" << (job.copyToProject ? "true" : "false") << ",";
    oss << "\"stagingPolicy\":\"copy_source\",";
    oss << "\"files\":[";
    for (std::size_t i = 0; i < job.items.size() && i < job.stagingPlan.files.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        const auto& item = job.items[i];
        const auto& staged = job.stagingPlan.files[i];
        oss << "{"
            << "\"sourcePath\":\"" << JsonEscape(item.sourcePathUtf8) << "\","
            << "\"stagedPath\":\"" << JsonEscape(staged.stagedPathUtf8) << "\","
            << "\"label\":\"" << JsonEscape(item.displayLabel) << "\","
            << "\"selection\":\"" << SourceSelectionKey(item.selectionKind) << "\""
            << "}";
    }
    oss << "]";
    oss << "}";
    return oss.str();
}

Job BuildJob(const Request& request, const std::string& rootDirectoryUtf8) {
    Job job;
    job.host = request.host;
    job.adapterPath = DefaultAdapterPathForHost(request.host);
    job.mode = request.mode;
    job.copyToProject = request.copyToProject;
    job.stagingPolicy = request.stagingPolicy;
    job.items = request.items;
    job.stagingPlan = BuildStagingPlan(request, rootDirectoryUtf8);
    job.payloadJson = BuildPayloadJson(job);
    return job;
}

} // namespace HarkenSearch::HostSend
