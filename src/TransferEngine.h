#pragma once
// SoundManager - 文件传输/转换引擎模块
// TransferEngine.h - 格式转换、目录结构生成、批量传输
// File transfer naming and destination helpers for library delivery.

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <filesystem>

// Transfer File Format

enum class TransferFileType {
    WAV,        // Broadcast Wave
    AIFF,       // AIFF
    FLAC,       // FLAC (Lossless)
    MP3,        // MP3 (Lossy)
    SourceFormat // Keep source format
};

struct TransferDestinationResolution {
    std::string path;
    bool shouldCopy = true;
    bool skipped = false;
    bool wouldOverwrite = false;
    std::string errorMessage;
};

// Existing File Behavior

enum class ExistingFileBehavior {
    AlwaysCreateNew = 0,    // 总是新建
    SkipExisting = 2,       // 跳过已存在
    UpdateMetadata = 3,     // 仅更新元数据
    ReplaceExisting = 4     // 替换已存在
};

// Folder Naming Strategy

enum class FolderNamingStrategy {
    SingleFolder,                       // 单一文件夹
    ByCategory,                         // 按 Category
    ByCategorySubCategory,              // 按 Category/SubCategory
    ByCategoryOptionalSubCategory,      // 按 Category/[SubCategory]
    ByCategorySubCategoryCDTitle,        // 按 Category/SubCategory/CDTitle
    ByLibrary,                          // 按 Library (fallback: CDTitle)
    ByLibraryCategory,                  // 按 Library/Category
    ByLibraryCategorySubCategory,       // 按 Library/Category/SubCategory
    ByLibraryVolume,                    // 按 Library/Volume
    ByLibraryVolumeCDTitle,             // 按 Library/Volume/CDTitle
    ByLibraryCDTitle,                   // 按 Library/CDTitle
    ByShowCategory,                     // 按 Show/Category
    ByShowCategorySubCategory,          // 按 Show/Category/SubCategory
    ByShowCategoryOptionalSubCategory,  // 按 Show/Category/[SubCategory]
    ByDesignerCategorySubCategory,      // 按 Designer/Category/SubCategory
    ByComposer,                         // 按 Composer
    ByChannelLayout,                    // 按 ChannelLayout
    MirrorSource,                       // 镜像源文件夹结构
    COUNT
};

// Transfer Preferences

struct TransferPrefs {
    // Format conversion
    bool useSourceRateDepth = true;     // 使用源文件的采样率和位深度
    uint32_t sampleRate = 48000;        // 目标采样率
    int bitDepth = 24;                  // 目标位深度
    TransferFileType fileType = TransferFileType::WAV;
    bool interleaved = true;            // 交错文件
    int bitRate = 160;                  // MP3 比特率 (kbps)
    
    // Naming
    int filenameLimit = 255;            // 文件名最大长度
    std::string filenameScheme = "Filename:1"; // 命名方案
    bool useMonoExtension = true;       // 使用单声道扩展名
    bool stripIllegals = true;          // 移除非法字符
    
    // Behavior
    bool sumToMono = false;             // 混合为单声道
    bool decodeMS = false;              // 解码 M/S 立体声
    bool embedMetadataOnXfers = true;   // 传输时嵌入元数据
    bool markers = false;               // 包含标记
    bool copyDSP = true;               // 复制 DSP 设置
    bool writeTrackInfo = false;        // 写入 iXML 声道信息
    ExistingFileBehavior existingBehavior = ExistingFileBehavior::SkipExisting;
    
    // Folder structure
    FolderNamingStrategy folderStrategy = FolderNamingStrategy::ByCategory;
    
    // Threading
    int cpuCount = 3;                   // 最大并发线程数
    bool threadSafeTransfers = false;   // 线程安全传输
};

// Transfer Task

struct TransferTask {
    std::string sourceFilePath;         // 源文件路径
    std::string destinationPath;        // 目标文件路径
    std::unordered_map<std::string, std::string> metadata; // 元数据
    
    enum class Status {
        Pending,
        InProgress,
        Completed,
        Skipped,
        Error
    };
    Status status = Status::Pending;
    std::string errorMessage;
    float progress = 0.0f;              // 0.0 - 1.0
};

// Transfer Progress

struct TransferProgress {
    std::atomic<int> total{0};
    std::atomic<int> completed{0};
    std::atomic<int> skipped{0};
    std::atomic<int> errors{0};
    std::atomic<bool> cancelled{false};
    std::string currentFile;
};

// @stage7-contract
// API contract: TransferEngine helpers are deterministic, do not retain input
// strings or preferences, and return either a validated destination/result or
// an explicit error/false value. File operations and user-data mutation remain
// in the owning workflow; these declarations only resolve and validate policy.
// Transfer Engine

class TransferEngine {
public:
// Folder Path Generation
    
    // 根据策略生成目标文件夹路径
    // exportFolder: 导出根目录
    // metadata: 元数据字段
    // Returns: 目标文件夹路径, 空字符串表示错误
    static std::string generateDestFolderPath(
        FolderNamingStrategy strategy,
        const std::string& exportFolder,
        const std::unordered_map<std::string, std::string>& metadata,
        std::string& errorMessage);
    
    // 获取所有文件夹策略名称列表
    static std::vector<std::pair<FolderNamingStrategy, std::string>> getFolderStrategyList();
    
    // 获取策略的显示名称
    static std::string getFolderStrategyName(FolderNamingStrategy strategy);
    
// Format Choices
    
    // 获取采样率选项
    static std::vector<std::pair<std::string, uint32_t>> getSampleRateChoices();
    
    // 获取位深度选项
    static std::vector<std::pair<std::string, int>> getBitDepthChoices();
    
    // 获取 MP3 比特率选项
    static std::vector<std::pair<std::string, int>> getBitRateChoices();
    
    // 获取文件类型选项
    static std::vector<std::pair<std::string, TransferFileType>> getFileTypeChoices();
    
    // 获取已存在文件处理选项
    static std::vector<std::pair<std::string, ExistingFileBehavior>> getExistingBehaviorChoices();
    static TransferDestinationResolution resolveExistingDestinationPath(
        const std::string& requestedPath,
        ExistingFileBehavior behavior,
        int maxSuffix = 999);
    static std::string buildSafeDestinationFileName(
        const std::string& sourceFileName,
        const TransferPrefs& prefs);
    static bool canExtractSingleChannel(int totalChannels);
    static std::string getSingleChannelExportLabel(int channelIndex, int totalChannels);
    static std::string buildSingleChannelExportFileName(
        const std::string& sourceFileName,
        int channelIndex,
        int totalChannels,
        const TransferPrefs& prefs);
    
// File Name Limit
    
    static int getMinFilenameLimit() { return 8; }
    static int getMaxFilenameLimit() { return 255; }
    
// Transfer Validation
    
    // 验证传输参数
    static bool validatePrefs(const TransferPrefs& prefs, std::string& errorMessage);
    
    // 检查文件格式兼容性 (如 FLAC 只支持 16/24 bit)
    static bool checkFormatCompatibility(TransferFileType fileType,
                                          uint32_t sampleRate,
                                          int bitDepth,
                                          std::string& errorMessage);
    
// Default Prefs

    static TransferPrefs getDefaultPrefs();

// Metadata Integrity Validation

    struct MetadataValidationResult {
        bool passed = true;
        bool bwfPreserved = true;        // BWF bext chunk fields
        bool iXMLPreserved = true;       // iXML production fields
        bool timeReferenceValid = true;  // BWF timecode (adjusted for sample rate change)
        bool basicTagsPreserved = true;  // Title, artist, description, keywords
        bool albumArtPreserved = true;   // Cover art transferred
        std::vector<std::string> missingFields;  // List of fields lost in transfer
        std::vector<std::string> warnings;       // Non-critical issues
    };

    // Validate metadata integrity after file transfer/transcode.
    // Compares source and destination file metadata.
    // srcSampleRate/dstSampleRate: for BWF timecode recalculation (0 = same rate)
    static MetadataValidationResult validateMetadataAfterTransfer(
        const std::string& srcPath,
        const std::string& dstPath,
        uint32_t srcSampleRate = 0,
        uint32_t dstSampleRate = 0);

private:
    // 清理字段值中的路径非法字符
    static std::string cleanFieldForPath(const std::string& field);
    
    // 获取路径分隔符
    static char getPathDivider();
};

inline std::string TransferEngine::buildSafeDestinationFileName(
    const std::string& sourceFileName,
    const TransferPrefs& prefs) {
    std::string name = sourceFileName.empty() ? std::string("untitled") : sourceFileName;
    size_t dot = name.find_last_of('.');
    std::string stem = (dot != std::string::npos && dot > 0) ? name.substr(0, dot) : name;
    std::string ext = (dot != std::string::npos && dot > 0) ? name.substr(dot) : std::string();

    auto cleanNamePart = [](std::string value) {
        for (auto& c : value) {
            if (c == ':' || c == '/' || c == '\\' || c == '*' || c == '?' ||
                c == '"' || c == '<' || c == '>' || c == '|') {
                c = '_';
            }
        }
        return value;
    };
    if (prefs.stripIllegals) {
        stem = cleanNamePart(stem);
        ext = cleanNamePart(ext);
    }
    while (!stem.empty() && (stem.back() == ' ' || stem.back() == '.')) {
        stem.pop_back();
    }
    if (stem.empty()) stem = "untitled";

    int limit = prefs.filenameLimit;
    if (limit < getMinFilenameLimit()) limit = getMinFilenameLimit();
    if (limit > getMaxFilenameLimit()) limit = getMaxFilenameLimit();

    if ((int)(stem.size() + ext.size()) > limit) {
        size_t stemLimit = (limit > (int)ext.size())
            ? static_cast<size_t>(limit - (int)ext.size())
            : 1u;
        if (stem.size() > stemLimit) stem.resize(stemLimit);
        while (!stem.empty() && (stem.back() == ' ' || stem.back() == '.')) {
            stem.pop_back();
        }
        if (stem.empty()) stem = "u";
    }
    return stem + ext;
}

inline bool TransferEngine::canExtractSingleChannel(int totalChannels) {
    return totalChannels > 2;
}

inline std::string TransferEngine::getSingleChannelExportLabel(
    int channelIndex,
    int totalChannels) {
    if (channelIndex < 0 || totalChannels <= 0 || channelIndex >= totalChannels) {
        return "Ch?";
    }

    static const char* kStandardLabels[] = {
        "L", "R", "C", "LFE", "Ls", "Rs", "Lb", "Rb"
    };
    constexpr int kStandardLabelCount =
        static_cast<int>(sizeof(kStandardLabels) / sizeof(kStandardLabels[0]));
    if (channelIndex < kStandardLabelCount) {
        return kStandardLabels[channelIndex];
    }
    return "Ch" + std::to_string(channelIndex + 1);
}

inline std::string TransferEngine::buildSingleChannelExportFileName(
    const std::string& sourceFileName,
    int channelIndex,
    int totalChannels,
    const TransferPrefs& prefs) {
    std::string nameOnly = sourceFileName.empty()
        ? std::string("untitled")
        : std::filesystem::u8path(sourceFileName).filename().u8string();
    if (nameOnly.empty()) nameOnly = "untitled";

    size_t dot = nameOnly.find_last_of('.');
    std::string stem = (dot != std::string::npos && dot > 0) ? nameOnly.substr(0, dot) : nameOnly;
    while (!stem.empty() && (stem.back() == ' ' || stem.back() == '.')) {
        stem.pop_back();
    }
    if (stem.empty()) stem = "untitled";

    const std::string label = getSingleChannelExportLabel(channelIndex, totalChannels);
    const std::string suffix = "_" + label;
    const std::string ext = ".wav";

    int limit = prefs.filenameLimit;
    if (limit < getMinFilenameLimit()) limit = getMinFilenameLimit();
    if (limit > getMaxFilenameLimit()) limit = getMaxFilenameLimit();

    const size_t reserve = suffix.size() + ext.size();
    if (static_cast<size_t>(limit) <= reserve) {
        return buildSafeDestinationFileName(label + ext, prefs);
    }

    const size_t stemLimit = static_cast<size_t>(limit) - reserve;
    if (stem.size() > stemLimit) {
        stem.resize(stemLimit);
        while (!stem.empty() && (stem.back() == ' ' || stem.back() == '.')) {
            stem.pop_back();
        }
        if (stem.empty()) stem = "u";
    }

    return buildSafeDestinationFileName(stem + suffix + ext, prefs);
}
