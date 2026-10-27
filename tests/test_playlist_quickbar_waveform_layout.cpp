#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string readText(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string text = ss.str();
    std::string normalized;
    normalized.reserve(text.size());
    for (char ch : text) {
        if (ch != '\r') normalized.push_back(ch);
    }
    return normalized;
}

bool requireContains(const std::string& haystack, const std::string& needle, const char* message) {
    if (haystack.find(needle) == std::string::npos) {
        std::cerr << "FAIL: " << message << "\nmissing: " << needle << "\n";
        return false;
    }
    return true;
}

bool requireNotContains(const std::string& haystack, const std::string& needle, const char* message) {
    if (haystack.find(needle) != std::string::npos) {
        std::cerr << "FAIL: " << message << "\nforbidden: " << needle << "\n";
        return false;
    }
    return true;
}

} // namespace

int main() {
    std::filesystem::path root = std::filesystem::current_path();
#ifdef AURA_SOURCE_DIR
    if (!std::filesystem::exists(root / "src" / "main.cpp")) {
        root = AURA_SOURCE_DIR;
    }
#endif

    const std::string mainCpp = readText(root / "src" / "main.cpp");
    if (mainCpp.empty()) {
        std::cerr << "FAIL: src/main.cpp is readable for playlist quick bar waveform layout contract\n";
        return 1;
    }

    bool ok = true;
    ok &= requireContains(
        mainCpp,
        "RenderPlaylistQuickBar(state, waveQuickBarH);\n            ImGui::Dummy(ImVec2(0.0f, waveQuickBarGap));\n        }\n        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + titleHeight);",
        "waveform transport/DSP layout must stay below the playlist quick bar");

    ok &= requireNotContains(
        mainCpp,
        "RenderPlaylistQuickBar(state, waveQuickBarH);\n            ImGui::Dummy(ImVec2(0.0f, waveQuickBarGap));\n        }\n        ImGui::SetCursorPosY(titleHeight);",
        "waveform transport/DSP layout must not rewind to the top after rendering the playlist quick bar");

    if (!ok) {
        return 1;
    }

    std::cout << "PASS: playlist quick bar does not overlay the waveform transport/DSP lane\n";
    return 0;
}
