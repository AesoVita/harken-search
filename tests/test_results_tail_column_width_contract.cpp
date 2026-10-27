#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string readText(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream stream;
    stream << in.rdbuf();
    return stream.str();
}

bool requireContains(const std::string& text, const std::string& needle, const char* message) {
    if (text.find(needle) != std::string::npos) return true;
    std::cerr << "FAIL: " << message << "\nmissing: " << needle << "\n";
    return false;
}

bool requireNotContains(const std::string& text, const std::string& needle, const char* message) {
    if (text.find(needle) == std::string::npos) return true;
    std::cerr << "FAIL: " << message << "\nforbidden: " << needle << "\n";
    return false;
}

} // namespace

int main() {
    const std::string mainCpp = readText(std::filesystem::current_path() / "src" / "main.cpp");
    if (mainCpp.empty()) {
        std::cerr << "FAIL: src/main.cpp is readable\n";
        return 1;
    }

    bool ok = true;
    ok &= requireContains(mainCpp, "constexpr float kResultsTailColumnAnomalyRatio = 0.45f",
                          "results table defines a narrow legacy tail-width anomaly threshold");
    ok &= requireContains(mainCpp, "const ImGuiID compactTailColumnIds[] = {",
                          "legacy width repair is limited to compact meter and rating columns");
    ok &= requireContains(mainCpp, "column.WidthRequest > innerWidth * kResultsTailColumnAnomalyRatio",
                          "only compact columns consuming an anomalous table share are repaired");
    ok &= requireContains(mainCpp, "ImGui::TableSetColumnWidth(columnIndex, compactTailColumnWidths[compactIndex])",
                          "only the anomalous compact column is restored to its intended width");
    ok &= requireContains(mainCpp,
                          "ImGui::TableSetupColumn(L(Col_FilePath), ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultHide",
                          "FilePath remains the only explicit trailing stretch column");
    ok &= requireContains(mainCpp,
                          "ImGui::TableSetupColumn(u8\"TruePeak\", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultHide",
                          "TruePeak remains a compact fixed-width column");
    ok &= requireNotContains(mainCpp, "ImGui::TableResetSettings(tbl)",
                             "targeted repair must preserve saved order, visibility, sorting, and unrelated widths");

    if (!ok) return 1;
    std::cout << "PASS: results tail column width contract\n";
    return 0;
}
