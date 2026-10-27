#include "platform/emergency_file_writer.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<Platform::EmergencyFile::Writer>);
static_assert(!std::is_move_constructible_v<Platform::EmergencyFile::Writer>);

int main() {
    Platform::EmergencyFile::Writer writer;
#ifdef _WIN32
    const auto path = std::filesystem::temp_directory_path() /
        "aris_stage6_emergency_writer.jsonl";
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (!writer.OpenOverwriteWide(path.c_str())) return 1;
    if (!writer.WriteAll(nullptr, 0)) return 2;
    if (writer.WriteAll(nullptr, 1)) return 9;
    const std::string first = "alpha";
    const std::string second = "-beta";
    if (!writer.WriteAll(first.data(), first.size())) return 3;
    if (!writer.WriteAll(second.data(), second.size())) return 4;
    if (!writer.Close() || !writer.Close()) return 5;
    std::ifstream input(path, std::ios::binary);
    const std::string actual{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    std::filesystem::remove(path, ec);
    if (actual != "alpha-beta") return 6;
    const auto missing = std::filesystem::temp_directory_path() /
        "aris_stage6_emergency_missing" / "crash.jsonl";
    std::filesystem::remove_all(missing.parent_path(), ec);
    if (writer.OpenOverwriteWide(missing.c_str())) return 7;
#else
    if (writer.OpenOverwriteWide(L"unsupported")) return 8;
#endif
    return 0;
}
