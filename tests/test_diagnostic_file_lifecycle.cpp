#include "DiagnosticFileLifecycle.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

bool Check(bool condition, const char* message) {
    if (condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

std::uint16_t ReadUtf16Le(const std::string& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(static_cast<std::uint8_t>(bytes[offset])) |
        static_cast<std::uint16_t>(static_cast<std::uint8_t>(bytes[offset + 1]) << 8);
}

bool IsLowSurrogate(std::uint16_t codeUnit) {
    return codeUnit >= 0xDC00 && codeUnit <= 0xDFFF;
}

bool IsWellFormedUtf16Le(const std::string& bytes) {
    if (bytes.size() % 2 != 0) return false;
    for (std::size_t offset = 0; offset < bytes.size(); offset += 2) {
        const std::uint16_t codeUnit = ReadUtf16Le(bytes, offset);
        if (codeUnit >= 0xD800 && codeUnit <= 0xDBFF) {
            if (offset + 3 >= bytes.size() || !IsLowSurrogate(ReadUtf16Le(bytes, offset + 2))) {
                return false;
            }
            offset += 2;
        } else if (IsLowSurrogate(codeUnit)) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "aris_diagnostic_file_lifecycle_test";
    const fs::path logPath = root / "Aris_diag.log";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    HarkenSearch::DiagnosticFileLifecycle log(logPath.u8string(), 1024);

    log.setEnabled(false);
    if (!Check(!log.append("disabled\n"), "disabled logging should reject writes") ||
        !Check(!fs::exists(logPath), "disabled logging should not create the file")) {
        return 1;
    }

    log.setEnabled(true);
    if (!Check(log.append("old-marker\n"), "initial marker should write")) return 1;
    const std::string record(180, 'x');
    for (int i = 0; i < 20; ++i) {
        if (!Check(log.append(record + "\n"), "enabled logging should write")) return 1;
    }
    const auto sizeAfterOverflow = log.sizeBytes();
    if (!Check(sizeAfterOverflow > 0, "log size should be visible") ||
        !Check(sizeAfterOverflow <= log.maxBytes(), "log must stay within its capacity") ||
        !Check(sizeAfterOverflow < log.maxBytes(), "overflow should leave room for cheap appends") ||
        !Check(ReadAll(logPath).size() == sizeAfterOverflow, "reported size should match disk") ||
        !Check(ReadAll(logPath).find("old-marker") == std::string::npos, "overflow should retain the newest window") ) {
        return 1;
    }

    const auto sizeBeforeCheapAppend = log.sizeBytes();
    if (!Check(log.append("newest-marker\n"), "newest marker should write")) return 1;
    if (!Check(log.sizeBytes() == sizeBeforeCheapAppend + std::string("newest-marker\n").size(),
               "append after compaction should use the available headroom") ||
        !Check(ReadAll(logPath).find("newest-marker") != std::string::npos,
               "overflow window should include newest records")) return 1;

    log.setEnabled(false);
    const auto sizeWhileDisabled = log.sizeBytes();
    if (!Check(!log.append("disabled after enable\n"), "disabled logging should stop future writes") ||
        !Check(log.sizeBytes() == sizeWhileDisabled, "disabled logging should preserve existing file")) {
        return 1;
    }

    if (!Check(log.clear(), "clear should succeed") ||
        !Check(log.sizeBytes() == 0, "clear should remove all occupied bytes")) {
        return 1;
    }

    log.setEnabled(true);
    const auto queuedGeneration = log.generation();
    if (!Check(log.clear(), "second clear should succeed") ||
        !Check(!log.appendAtGeneration("queued before clear\n", queuedGeneration),
               "writes queued before clear must be discarded")) {
        return 1;
    }

    const fs::path externallyRemovedPath = root / "externally_removed.log";
    HarkenSearch::DiagnosticFileLifecycle externallyRemoved(externallyRemovedPath.u8string(), 64);
    externallyRemoved.setEnabled(true);
    if (!Check(externallyRemoved.append(std::string(64, 'a')), "full file should write") ||
        !Check(externallyRemoved.sizeBytes() == 64, "full file should report capacity")) {
        return 1;
    }
    fs::remove(externallyRemovedPath, ec);
    if (!Check(externallyRemoved.append("recreated\n"), "external deletion should recover") ||
        !Check(externallyRemoved.sizeBytes() == std::string("recreated\n").size(),
               "cache should match bytes actually recreated") ||
        !Check(ReadAll(externallyRemovedPath) == "recreated\n",
               "recreated file should contain only the new record")) {
        return 1;
    }

    const fs::path utf16Path = root / "utf16_aligned.log";
    {
        std::ofstream output(utf16Path, std::ios::binary);
        output.write("0123456789A", 11);
    }
    HarkenSearch::DiagnosticFileLifecycle utf16Log(utf16Path.u8string(), 16, 2);
    utf16Log.setEnabled(true);
    if (!Check(utf16Log.append("abcdefgh"),
               "aligned overflow should preserve complete UTF-16 elements")) {
        return 1;
    }
    const std::string alignedBytes = ReadAll(utf16Path);
    if (!Check(alignedBytes == "23456789abcdefgh",
               "overflow should discard the incomplete leading element, not shift it")) {
        return 1;
    }

    const fs::path utf16SurrogatePath = root / "utf16_surrogate.log";
    const std::string emojiExisting(
        "\x41\x00\x3D\xD8\x00\xDE\x42\x00\x43\x00\x44\x00", 12);
    const std::string emojiIncoming("\x45\x00\x46\x00\x47\x00", 6);
    {
        std::ofstream output(utf16SurrogatePath, std::ios::binary);
        output.write(emojiExisting.data(), static_cast<std::streamsize>(emojiExisting.size()));
    }
    HarkenSearch::DiagnosticFileLifecycle utf16Surrogate(utf16SurrogatePath.u8string(), 16, 2);
    utf16Surrogate.setEnabled(true);
    if (!Check(utf16Surrogate.append(emojiIncoming),
               "UTF-16 surrogate fixture should compact and append")) {
        return 1;
    }
    const std::string surrogateBytes = ReadAll(utf16SurrogatePath);
    if (!Check(surrogateBytes.size() <= utf16Surrogate.maxBytes(),
               "UTF-16 surrogate compaction must stay within capacity") ||
        !Check(IsWellFormedUtf16Le(surrogateBytes),
               "UTF-16 surrogate compaction must not split a surrogate pair") ||
        !Check(!IsLowSurrogate(ReadUtf16Le(surrogateBytes, 0)),
               "UTF-16 surrogate compaction must not start with a low surrogate")) {
        return 1;
    }

    const std::string emoji("\x3D\xD8\x00\xDE", 4);
    const fs::path utf16FitsPath = root / "utf16_fits.log";
    HarkenSearch::DiagnosticFileLifecycle utf16Fits(utf16FitsPath.u8string(), 2, 2);
    utf16Fits.setEnabled(true);
    if (!Check(utf16Fits.append(emoji), "UTF-16 fitting truncation should write safely")) {
        return 1;
    }
    const std::string fitsBytes = ReadAll(utf16FitsPath);
    if (!Check(fitsBytes.size() <= utf16Fits.maxBytes(),
               "UTF-16 fitting truncation must stay within capacity") ||
        !Check(IsWellFormedUtf16Le(fitsBytes),
               "UTF-16 fitting truncation must not split a surrogate pair") ||
        !Check(fitsBytes.empty() || !IsLowSurrogate(ReadUtf16Le(fitsBytes, 0)),
               "UTF-16 fitting truncation must not start with a low surrogate")) {
        return 1;
    }

    const fs::path utf16IncomingPath = root / "utf16_incoming_surrogate.log";
    {
        std::ofstream output(utf16IncomingPath, std::ios::binary);
        output.write("\x58\x00\x59\x00", 4);
    }
    const std::string oversizedIncoming(
        "\x41\x00\x3D\xD8\x00\xDE\x42\x00\x43\x00\x44\x00", 12);
    HarkenSearch::DiagnosticFileLifecycle utf16Incoming(utf16IncomingPath.u8string(), 8, 2);
    utf16Incoming.setEnabled(true);
    if (!Check(utf16Incoming.append(oversizedIncoming),
               "oversized UTF-16 input should compact and append safely")) {
        return 1;
    }
    const std::string incomingBytes = ReadAll(utf16IncomingPath);
    if (!Check(incomingBytes.size() <= utf16Incoming.maxBytes(),
               "oversized UTF-16 input must stay within capacity") ||
        !Check(IsWellFormedUtf16Le(incomingBytes),
               "oversized UTF-16 input must not split a surrogate pair") ||
        !Check(!IsLowSurrogate(ReadUtf16Le(incomingBytes, 0)),
               "oversized UTF-16 input must not start with a low surrogate")) {
        return 1;
    }

    const fs::path fitsRemovedPath = root / "fits_removed.log";
    HarkenSearch::DiagnosticFileLifecycle fitsRemoved(fitsRemovedPath.u8string(), 64);
    fitsRemoved.setEnabled(true);
    if (!Check(fitsRemoved.append("cached-bytes\n"),
               "cache drift fixture should write the initial record")) {
        return 1;
    }
    fs::remove(fitsRemovedPath, ec);
    if (!Check(fitsRemoved.append("fresh\n"),
               "append should recreate an externally removed file")) {
        return 1;
    }
    if (!Check(fitsRemoved.sizeBytes() == std::string("fresh\n").size(),
               "cache should recover from external deletion on a fitting append") ||
        !Check(ReadAll(fitsRemovedPath) == "fresh\n",
               "recreated fitting append should contain only the fresh record")) {
        return 1;
    }

    fs::remove_all(root, ec);
    std::cout << "PASS: diagnostic file switch, capacity and cleanup are bounded\n";
    return 0;
}
