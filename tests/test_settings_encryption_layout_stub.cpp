#include "platform/harken_user_data_layout.h"

namespace Platform::HarkenUserData {

bool PreviewIsolationActive() {
    return true;
}

Layout EnsureLayout() {
    return Layout{};
}

std::wstring LegacyAuroDbFilePath() {
    return std::wstring();
}

}
