// SoundManager - 文件传输/转换引擎模块
// TransferEngine.cpp - 目录结构生成和传输逻辑
// File transfer naming and destination helpers for library delivery.

#include "TransferEngine.h"
#include "MetadataReader.h"
#include "platform/filesystem.h"
#include <algorithm>
#include <sstream>
#include <cmath>

TransferDestinationResolution TransferEngine::resolveExistingDestinationPath(
    const std::string& requestedPath,
    ExistingFileBehavior behavior,
    int maxSuffix) {
    namespace fs = std::filesystem;
    TransferDestinationResolution result;
    result.path = requestedPath;
    if (requestedPath.empty()) {
        result.shouldCopy = false;
        result.errorMessage = "Destination path is empty";
        return result;
    }

    std::string probeError;
    const auto requestedStatus = Platform::FileSystem::ProbePath(requestedPath, &probeError);
    if (requestedStatus == Platform::FileSystem::PathStatus::Error) {
        result.shouldCopy = false;
        result.errorMessage = probeError;
        return result;
    }
    if (requestedStatus == Platform::FileSystem::PathStatus::Missing) return result;

    if (behavior == ExistingFileBehavior::SkipExisting ||
        behavior == ExistingFileBehavior::UpdateMetadata) {
        result.shouldCopy = false;
        result.skipped = true;
        return result;
    }
    if (behavior == ExistingFileBehavior::ReplaceExisting) {
        result.wouldOverwrite = true;
        return result;
    }

    const fs::path requested = fs::u8path(requestedPath);
    const fs::path parent = requested.parent_path();
    const std::string stem = requested.stem().u8string();
    const std::string ext = requested.extension().u8string();
    for (int i = 1; i <= maxSuffix; ++i) {
        const fs::path candidate = parent / (stem + "_" + std::to_string(i) + ext);
        probeError.clear();
        const auto candidateStatus = Platform::FileSystem::ProbePath(
            candidate.u8string(), &probeError);
        if (candidateStatus == Platform::FileSystem::PathStatus::Missing) {
            result.path = candidate.u8string();
            return result;
        }
        if (candidateStatus == Platform::FileSystem::PathStatus::Error) {
            result.shouldCopy = false;
            result.errorMessage = probeError;
            return result;
        }
    }

    result.shouldCopy = false;
    result.errorMessage = "No free destination suffix";
    return result;
}

// Helpers

char TransferEngine::getPathDivider() {
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

std::string TransferEngine::cleanFieldForPath(const std::string& field) {
    if (field.empty()) return "";
    std::string result = field;
    // Normalize separators for destination-friendly mirror paths.
    for (auto& c : result) {
        if (c == ':') c = ' ';
        else if (c == '/') c = '_';
        else if (c == '\\') c = '_';
        else if (c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
    }
    return result;
}

static std::string getFieldOrError(const std::unordered_map<std::string, std::string>& metadata,
                                    const std::string& field, std::string& errorMessage) {
    auto it = metadata.find(field);
    if (it == metadata.end() || it->second.empty()) {
        errorMessage = "Missing " + field + " field";
        return "";
    }
    return it->second;
}

// Generate Destination Folder Path

std::string TransferEngine::generateDestFolderPath(
    FolderNamingStrategy strategy,
    const std::string& exportFolder,
    const std::unordered_map<std::string, std::string>& metadata,
    std::string& errorMessage) {
    
    char pd = getPathDivider();
    std::string base = exportFolder;
    if (!base.empty() && base.back() != pd && base.back() != '/' && base.back() != '\\') {
        base += pd;
    }
    
    auto getField = [&](const std::string& name) -> std::string {
        return getFieldOrError(metadata, name, errorMessage);
    };
    
    auto clean = [](const std::string& f) { return cleanFieldForPath(f); };
    
    switch (strategy) {
        case FolderNamingStrategy::SingleFolder:
            return base;
            
        case FolderNamingStrategy::ByCategory: {
            std::string cat = getField("Category");
            if (cat.empty()) return "";
            return base + clean(cat) + pd;
        }
        
        case FolderNamingStrategy::ByCategorySubCategory: {
            std::string cat = getField("Category");
            if (cat.empty()) return "";
            std::string sub = getField("SubCategory");
            if (sub.empty()) return "";
            return base + clean(cat) + pd + clean(sub) + pd;
        }
        
        case FolderNamingStrategy::ByCategoryOptionalSubCategory: {
            std::string cat = getField("Category");
            if (cat.empty()) return "";
            std::string result = base + clean(cat) + pd;
            auto subIt = metadata.find("SubCategory");
            if (subIt != metadata.end() && !subIt->second.empty()) {
                result += clean(subIt->second) + pd;
            }
            return result;
        }
        
        case FolderNamingStrategy::ByCategorySubCategoryCDTitle: {
            std::string cat = getField("Category");
            if (cat.empty()) return "";
            std::string sub = getField("SubCategory");
            if (sub.empty()) return "";
            std::string cd = getField("CDTitle");
            if (cd.empty()) return "";
            return base + clean(cat) + pd + clean(sub) + pd + clean(cd) + pd;
        }
        
        case FolderNamingStrategy::ByLibrary: {
            auto cdIt = metadata.find("CDTitle");
            if (cdIt != metadata.end() && !cdIt->second.empty()) {
                return base + clean(cdIt->second) + pd;
            }
            auto libIt = metadata.find("Library");
            if (libIt != metadata.end() && !libIt->second.empty()) {
                return base + clean(libIt->second) + pd;
            }
            errorMessage = "Missing Library/CDTitle field";
            return "";
        }
        
        case FolderNamingStrategy::ByLibraryCategory: {
            std::string lib = getField("Library");
            if (lib.empty()) return "";
            std::string cat = getField("Category");
            if (cat.empty()) return "";
            return base + clean(lib) + pd + clean(cat) + pd;
        }
        
        case FolderNamingStrategy::ByLibraryCategorySubCategory: {
            std::string lib = getField("Library");
            if (lib.empty()) return "";
            std::string cat = getField("Category");
            if (cat.empty()) return "";
            std::string sub = getField("SubCategory");
            if (sub.empty()) return "";
            return base + clean(lib) + pd + clean(cat) + pd + clean(sub) + pd;
        }
        
        case FolderNamingStrategy::ByLibraryVolume: {
            std::string lib = getField("Library");
            if (lib.empty()) return "";
            std::string vol = getField("Volume");
            if (vol.empty()) return "";
            return base + clean(lib) + pd + clean(vol) + pd;
        }
        
        case FolderNamingStrategy::ByLibraryVolumeCDTitle: {
            std::string lib = getField("Library");
            if (lib.empty()) return "";
            std::string vol = getField("Volume");
            if (vol.empty()) return "";
            std::string cd = getField("CDTitle");
            if (cd.empty()) return "";
            return base + clean(lib) + pd + clean(vol) + pd + clean(cd) + pd;
        }
        
        case FolderNamingStrategy::ByLibraryCDTitle: {
            std::string lib = getField("Library");
            if (lib.empty()) return "";
            std::string cd = getField("CDTitle");
            if (cd.empty()) return "";
            return base + clean(lib) + pd + clean(cd) + pd;
        }
        
        case FolderNamingStrategy::ByShowCategory: {
            std::string show = getField("Show");
            if (show.empty()) return "";
            std::string cat = getField("Category");
            if (cat.empty()) return "";
            return base + clean(show) + pd + clean(cat) + pd;
        }
        
        case FolderNamingStrategy::ByShowCategorySubCategory: {
            std::string show = getField("Show");
            if (show.empty()) return "";
            std::string cat = getField("Category");
            if (cat.empty()) return "";
            std::string sub = getField("SubCategory");
            if (sub.empty()) return "";
            return base + clean(show) + pd + clean(cat) + pd + clean(sub) + pd;
        }
        
        case FolderNamingStrategy::ByShowCategoryOptionalSubCategory: {
            std::string show = getField("Show");
            if (show.empty()) return "";
            std::string cat = getField("Category");
            if (cat.empty()) return "";
            std::string result = base + clean(show) + pd + clean(cat) + pd;
            auto subIt = metadata.find("SubCategory");
            if (subIt != metadata.end() && !subIt->second.empty()) {
                result += clean(subIt->second) + pd;
            }
            return result;
        }
        
        case FolderNamingStrategy::ByDesignerCategorySubCategory: {
            std::string des = getField("Designer");
            if (des.empty()) return "";
            std::string cat = getField("Category");
            if (cat.empty()) return "";
            std::string sub = getField("SubCategory");
            if (sub.empty()) return "";
            return base + clean(des) + pd + clean(cat) + pd + clean(sub) + pd;
        }
        
        case FolderNamingStrategy::ByComposer: {
            std::string comp = getField("Composer");
            if (comp.empty()) return "";
            return base + clean(comp) + pd;
        }
        
        case FolderNamingStrategy::ByChannelLayout: {
            std::string ch = getField("ChannelLayout");
            if (ch.empty()) return "";
            std::string cleaned = clean(ch);
            // Remove slashes from channel layout
            cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '/'), cleaned.end());
            return base + cleaned + pd;
        }
        
        case FolderNamingStrategy::MirrorSource: {
            auto pathIt = metadata.find("Pathname");
            if (pathIt == metadata.end() || pathIt->second.empty()) {
                errorMessage = "Missing Pathname field";
                return "";
            }
            std::string originalPath = pathIt->second;
            
#ifdef _WIN32
            // On Windows, strip drive letter prefix for mirroring
            if (originalPath.length() >= 3 && originalPath[1] == ':') {
                originalPath = originalPath.substr(2);
            }
            // Convert forward slashes to backslashes
            for (auto& c : originalPath) {
                if (c == '/') c = '\\';
            }
#else
            // On macOS, strip /Volumes/DriveName or /Users/Username prefix
            if (originalPath.find("/Volumes/") == 0) {
                size_t secondSlash = originalPath.find('/', 9);
                if (secondSlash != std::string::npos)
                    originalPath = originalPath.substr(secondSlash);
            } else if (originalPath.find("/Users/") == 0) {
                size_t secondSlash = originalPath.find('/', 7);
                if (secondSlash != std::string::npos)
                    originalPath = originalPath.substr(secondSlash);
            }
#endif
            return base + originalPath;
        }
        
        default:
            errorMessage = "Unknown folder naming strategy";
            return "";
    }
}

// Strategy List

std::vector<std::pair<FolderNamingStrategy, std::string>> TransferEngine::getFolderStrategyList() {
    return {
        {FolderNamingStrategy::SingleFolder,                      "Build Using Single Folder"},
        {FolderNamingStrategy::ByCategory,                        "Build Using Category"},
        {FolderNamingStrategy::ByCategorySubCategory,             "Build Using Category then SubCategory"},
        {FolderNamingStrategy::ByCategoryOptionalSubCategory,     "Build Using Category then [optional] SubCategory"},
        {FolderNamingStrategy::ByCategorySubCategoryCDTitle,      "Build Using Category then SubCategory then CDTitle"},
        {FolderNamingStrategy::ByLibraryCategory,                 "Build Using Library then Category"},
        {FolderNamingStrategy::ByLibraryCategorySubCategory,      "Build Using Library then Category then SubCategory"},
        {FolderNamingStrategy::ByLibraryVolume,                   "Build Using Library then Volume"},
        {FolderNamingStrategy::ByLibraryVolumeCDTitle,            "Build Using Library then Volume then CDTitle"},
        {FolderNamingStrategy::ByLibraryCDTitle,                  "Build Using Library then CDTitle"},
        {FolderNamingStrategy::ByShowCategory,                    "Build Using Show then Category"},
        {FolderNamingStrategy::ByShowCategorySubCategory,         "Build Using Show then Category then SubCategory"},
        {FolderNamingStrategy::ByShowCategoryOptionalSubCategory, "Build Using Show then Category then [optional] SubCategory"},
        {FolderNamingStrategy::ByDesignerCategorySubCategory,     "Build Using Designer then Category then SubCategory"},
        {FolderNamingStrategy::ByComposer,                        "Build Using Composer"},
        {FolderNamingStrategy::ByChannelLayout,                   "Build Using ChannelLayout"},
        {FolderNamingStrategy::MirrorSource,                      "Mirror Source Folder Structure"}
    };
}

std::string TransferEngine::getFolderStrategyName(FolderNamingStrategy strategy) {
    auto list = getFolderStrategyList();
    for (const auto& p : list) {
        if (p.first == strategy) return p.second;
    }
    return "Unknown";
}

// Format Choices

std::vector<std::pair<std::string, uint32_t>> TransferEngine::getSampleRateChoices() {
    return {
        {"44.1 kHz", 44100},
        {"48 kHz",   48000},
        {"88.2 kHz", 88200},
        {"96 kHz",   96000},
        {"192 kHz",  192000}
    };
}

std::vector<std::pair<std::string, int>> TransferEngine::getBitDepthChoices() {
    return {
        {"16 bit", 16},
        {"24 bit", 24}
    };
}

std::vector<std::pair<std::string, int>> TransferEngine::getBitRateChoices() {
    return {
        {"112 kbps", 112},
        {"128 kbps", 128},
        {"160 kbps", 160},
        {"192 kbps", 192},
        {"224 kbps", 224},
        {"256 kbps", 256},
        {"320 kbps", 320}
    };
}

std::vector<std::pair<std::string, TransferFileType>> TransferEngine::getFileTypeChoices() {
    return {
        {"Broadcast Wave", TransferFileType::WAV},
        {"AIFF",           TransferFileType::AIFF},
        {"FLAC (Lossless)", TransferFileType::FLAC},
        {"MP3",            TransferFileType::MP3},
        {"Source Format",  TransferFileType::SourceFormat}
    };
}

std::vector<std::pair<std::string, ExistingFileBehavior>> TransferEngine::getExistingBehaviorChoices() {
    return {
        {"Always Create New", ExistingFileBehavior::AlwaysCreateNew},
        {"Skip Existing",     ExistingFileBehavior::SkipExisting},
        {"Update Metadata",   ExistingFileBehavior::UpdateMetadata},
        {"Replace Existing",  ExistingFileBehavior::ReplaceExisting}
    };
}

// Validation

bool TransferEngine::validatePrefs(const TransferPrefs& prefs, std::string& errorMessage) {
    if (prefs.filenameLimit < 8 || prefs.filenameLimit > 255) {
        errorMessage = "Filename limit must be between 8 and 255";
        return false;
    }
    if (prefs.cpuCount < 1 || prefs.cpuCount > 32) {
        errorMessage = "CPU count must be between 1 and 32";
        return false;
    }
    
    if (!prefs.useSourceRateDepth) {
        std::string fmtErr;
        if (!checkFormatCompatibility(prefs.fileType, prefs.sampleRate, prefs.bitDepth, fmtErr)) {
            errorMessage = fmtErr;
            return false;
        }
    }
    
    return true;
}

bool TransferEngine::checkFormatCompatibility(TransferFileType fileType,
                                               uint32_t sampleRate,
                                               int bitDepth,
                                               std::string& errorMessage) {
    if (fileType == TransferFileType::FLAC) {
        if (bitDepth != 16 && bitDepth != 24) {
            errorMessage = "FLAC only supports 16 or 24 bit files";
            return false;
        }
    }
    if (fileType == TransferFileType::MP3) {
        if (sampleRate > 48000) {
            errorMessage = "MP3 files have a maximum sample rate of 48kHz";
            return false;
        }
    }
    return true;
}

// Default Prefs

TransferPrefs TransferEngine::getDefaultPrefs() {
    return TransferPrefs(); // Use struct defaults
}

// Metadata Integrity Validation

TransferEngine::MetadataValidationResult TransferEngine::validateMetadataAfterTransfer(
    const std::string& srcPath,
    const std::string& dstPath,
    uint32_t srcSampleRate,
    uint32_t dstSampleRate) {

    MetadataValidationResult result;

    AudioMetadata srcMeta, dstMeta;
    bool srcOk = MetadataReader::readMetadata(srcPath, srcMeta);
    bool dstOk = MetadataReader::readMetadata(dstPath, dstMeta);

    if (!srcOk) {
        result.warnings.push_back("Could not read source metadata");
        return result;
    }
    if (!dstOk) {
        result.passed = false;
        result.missingFields.push_back("(entire destination metadata unreadable)");
        return result;
    }

    // Helper: compare string fields
    auto checkField = [&](const char* name, const std::string& src, const std::string& dst, bool critical) {
        if (!src.empty() && dst.empty()) {
            result.missingFields.push_back(name);
            if (critical) result.passed = false;
        } else if (!src.empty() && src != dst) {
            result.warnings.push_back(std::string(name) + ": value changed");
        }
    };

    // Basic tags
    checkField("title", srcMeta.title, dstMeta.title, false);
    checkField("artist", srcMeta.artist, dstMeta.artist, false);
    checkField("description", srcMeta.description, dstMeta.description, true);
    checkField("keywords", srcMeta.keywords, dstMeta.keywords, true);
    checkField("category", srcMeta.category, dstMeta.category, true);
    if (!srcMeta.description.empty() && dstMeta.description.empty())
        result.basicTagsPreserved = false;

    // BWF fields
    checkField("bw_description", srcMeta.bwDescription, dstMeta.bwDescription, true);
    checkField("bw_originator", srcMeta.originator, dstMeta.originator, true);
    checkField("bw_originator_ref", srcMeta.originatorRef, dstMeta.originatorRef, false);
    checkField("bw_date", srcMeta.originationDate, dstMeta.originationDate, true);
    checkField("bw_time", srcMeta.originationTime, dstMeta.originationTime, false);
    checkField("bw_umid", srcMeta.umid, dstMeta.umid, true);
    checkField("bw_coding_history", srcMeta.codingHistory, dstMeta.codingHistory, false);

    // Check if any BWF field was lost
    if (!srcMeta.bwDescription.empty() && dstMeta.bwDescription.empty())
        result.bwfPreserved = false;
    if (!srcMeta.originator.empty() && dstMeta.originator.empty())
        result.bwfPreserved = false;
    if (!srcMeta.umid.empty() && dstMeta.umid.empty())
        result.bwfPreserved = false;

    // BWF TimeReference validation (adjusted for sample rate change)
    if (srcMeta.bwTimeStamp > 0) {
        if (dstMeta.bwTimeStamp == 0) {
            result.timeReferenceValid = false;
            result.missingFields.push_back("bw_timestamp");
            result.passed = false;
        } else if (srcSampleRate > 0 && dstSampleRate > 0 && srcSampleRate != dstSampleRate) {
            // Recalculate expected timestamp for new sample rate
            double expectedTimestamp = (double)srcMeta.bwTimeStamp * (double)dstSampleRate / (double)srcSampleRate;
            double actualTimestamp = (double)dstMeta.bwTimeStamp;
            // Allow 1 sample tolerance for rounding
            if (std::abs(expectedTimestamp - actualTimestamp) > 1.5) {
                result.timeReferenceValid = false;
                result.warnings.push_back("BWF TimeReference not correctly recalculated for new sample rate");
            }
        } else if (srcMeta.bwTimeStamp != dstMeta.bwTimeStamp) {
            result.timeReferenceValid = false;
            result.warnings.push_back("BWF TimeReference changed unexpectedly");
        }
    }

    // iXML fields
    checkField("ixml_project", srcMeta.project, dstMeta.project, true);
    checkField("ixml_scene", srcMeta.scene, dstMeta.scene, false);
    checkField("ixml_take", srcMeta.take, dstMeta.take, false);
    checkField("ixml_tape", srcMeta.tape, dstMeta.tape, false);
    checkField("ixml_file_uid", srcMeta.fileUID, dstMeta.fileUID, true);
    checkField("ixml_track_list", srcMeta.trackList, dstMeta.trackList, true);

    if (!srcMeta.project.empty() && dstMeta.project.empty())
        result.iXMLPreserved = false;
    if (!srcMeta.fileUID.empty() && dstMeta.fileUID.empty())
        result.iXMLPreserved = false;
    if (!srcMeta.trackList.empty() && dstMeta.trackList.empty())
        result.iXMLPreserved = false;

    // Album art
    if (srcMeta.albumArt.hasData() && !dstMeta.albumArt.hasData()) {
        result.albumArtPreserved = false;
        result.warnings.push_back("Album art not transferred");
    }

    // Set overall pass/fail
    if (!result.bwfPreserved || !result.iXMLPreserved || !result.timeReferenceValid)
        result.passed = false;

    return result;
}
