#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include <string>
#include <utility>
#include <array>
#include <map>

#include "HarkenSearchUpdateComposition.h"
#include "HarkenSearchUpdateNoticeModel.h"

namespace aris::update {

#if AURA_BUILD_DIAGNOSTICS
// Real renderer geometry for bounded CLI interaction, never OS mouse control.
struct HarkenSearchUpdateNoticeQaSnapshot {
    bool visible = false;
    bool history = false;
    std::string version;
    std::size_t itemCount = 0;
    float scrollY = 0;
    float scrollMaxY = 0;
    std::map<std::string, std::array<float, 4>> controls;
};
HarkenSearchUpdateNoticeQaSnapshot GetHarkenSearchUpdateNoticeQaSnapshot();
#endif

enum class HarkenSearchUpdateNoticeVisibleState {
    Checking,
    Latest,
    Available,
    Downloading,
    Restarting,
    Error,
    History,
};

enum class HarkenSearchUpdateHistoryState { Empty, LoadFailed, Populated };

struct HarkenSearchUpdateNoticeProgress {
    bool determinate = false;
    float fraction = 0.0f;
};

inline HarkenSearchUpdateNoticeVisibleState MapHarkenSearchUpdateNoticeState(
    HarkenSearchUpdateUiPhase phase, bool historyVisible) {
    if (historyVisible) return HarkenSearchUpdateNoticeVisibleState::History;
    switch (phase) {
        case HarkenSearchUpdateUiPhase::Checking: return HarkenSearchUpdateNoticeVisibleState::Checking;
        case HarkenSearchUpdateUiPhase::Idle: return HarkenSearchUpdateNoticeVisibleState::Latest;
        case HarkenSearchUpdateUiPhase::Ready: return HarkenSearchUpdateNoticeVisibleState::Available;
        case HarkenSearchUpdateUiPhase::Downloading: return HarkenSearchUpdateNoticeVisibleState::Downloading;
        case HarkenSearchUpdateUiPhase::Restarting: return HarkenSearchUpdateNoticeVisibleState::Restarting;
        case HarkenSearchUpdateUiPhase::Error: return HarkenSearchUpdateNoticeVisibleState::Error;
        case HarkenSearchUpdateUiPhase::Disabled: return HarkenSearchUpdateNoticeVisibleState::Latest;
    }
    return HarkenSearchUpdateNoticeVisibleState::Error;
}

inline HarkenSearchUpdateNoticeProgress BuildHarkenSearchUpdateProgress(
    std::uint64_t downloaded, std::uint64_t total) {
    if (total == 0) return {};
    const float raw = static_cast<float>(downloaded) / static_cast<float>(total);
    return {true, raw < 0.0f ? 0.0f : (raw > 1.0f ? 1.0f : raw)};
}

inline HarkenSearchUpdateHistoryState ClassifyHarkenSearchUpdateHistory(bool loadFailed, bool hasData) {
    if (loadFailed) return HarkenSearchUpdateHistoryState::LoadFailed;
    return hasData ? HarkenSearchUpdateHistoryState::Populated : HarkenSearchUpdateHistoryState::Empty;
}

// Build a font-safe display copy without changing the source bytes used by
// update comparison, signature verification, persistence, or diagnostics.
inline std::string SanitizeHarkenSearchUpdateDisplayText(std::string_view source) {
    std::string display;
    display.reserve(source.size());
    std::size_t offset = 0;
    while (offset < source.size()) {
        const auto lead = static_cast<unsigned char>(source[offset]);
        if (lead < 0x80U) {
            display.push_back(static_cast<char>(lead));
            ++offset;
            continue;
        }

        std::size_t length = 0;
        std::uint32_t codePoint = 0;
        std::uint32_t minimum = 0;
        if (lead >= 0xC2U && lead <= 0xDFU) {
            length = 2; codePoint = lead & 0x1FU; minimum = 0x80U;
        } else if (lead >= 0xE0U && lead <= 0xEFU) {
            length = 3; codePoint = lead & 0x0FU; minimum = 0x800U;
        } else if (lead >= 0xF0U && lead <= 0xF4U) {
            length = 4; codePoint = lead & 0x07U; minimum = 0x10000U;
        } else {
            display.append("[invalid]");
            ++offset;
            continue;
        }

        if (source.size() - offset < length) {
            display.append("[invalid]");
            break;
        }
        bool continuationValid = true;
        for (std::size_t index = 1; index < length; ++index) {
            const auto byte = static_cast<unsigned char>(source[offset + index]);
            if ((byte & 0xC0U) != 0x80U) {
                continuationValid = false;
                break;
            }
            codePoint = (codePoint << 6U) | (byte & 0x3FU);
        }
        if (!continuationValid) {
            display.append("[invalid]");
            ++offset;
            continue;
        }
        const bool scalarValid = codePoint >= minimum && codePoint <= 0x10FFFFU &&
                                 !(codePoint >= 0xD800U && codePoint <= 0xDFFFU);
        if (!scalarValid) {
            display.append("[invalid]");
        } else if (codePoint >= 0x10000U) {
            display.append("[emoji]");
        } else {
            display.append(source.data() + offset, length);
        }
        offset += length;
    }
    return display;
}

inline std::string BuildHarkenSearchUpdateReadyCopy(std::string_view version) {
    std::string copy = u8"更新已下载完成";
    if (!version.empty()) copy += " v" + SanitizeHarkenSearchUpdateDisplayText(version);
    copy += u8"，重启后应用";
    return copy;
}

inline void FinalizeHarkenSearchUpdateNoticeClose(bool& open, ReleaseNotesUiState& notes) {
    open = false;
    notes.showHistory = false;
    notes.summaryScrollKey.clear();
    notes.errorScrollKey.clear();
    notes.expandedReleaseVersion.clear();
}

class HarkenSearchUpdateNoticeActionGate {
public:
    void Enter(std::string key, std::uint64_t operationGeneration = 0) {
        if (key_ != key || operationGeneration_ != operationGeneration) {
            key_ = std::move(key);
            operationGeneration_ = operationGeneration;
            consumed_ = false;
        }
    }
    bool TryConsume() {
        if (consumed_) return false;
        consumed_ = true;
        return true;
    }
    void ReleaseRejectedAction() {
        consumed_ = false;
    }
private:
    std::string key_;
    std::uint64_t operationGeneration_ = 0;
    bool consumed_ = false;
};

struct HarkenSearchUpdateNoticeActions {
    std::function<bool()> requestCheck;
    std::function<bool()> confirmRestart;
};

struct HarkenSearchUpdateNoticePresentation {
    const char* windowId = "##HarkenSearchUpdateCenter";
    const char* checkLabel = u8"检查更新";
    const char* retryLabel = u8"重试";
    const char* restartLabel = u8"立即重启更新";
    const char* laterLabel = u8"稍后";
    std::uint64_t downloadedBytes = 0;
    std::uint64_t totalBytes = 0;
    std::string_view lastError;
    std::uint64_t operationGeneration = 0;
};

// Renders the native Aris What's New/update surface. The caller owns the
// window-open flag and release-note loading; this module owns all visible UI.
void RenderHarkenSearchUpdateNoticeUi(
    bool& open,
    ReleaseNotesUiState& notes,
    const HarkenSearchUpdateUiState& updateState,
    const HarkenSearchUpdateNoticeActions& actions,
    const HarkenSearchUpdateNoticePresentation& presentation);

}  // namespace aris::update
