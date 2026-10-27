#include "platform/text_encoding.inl"

#include <filesystem>

std::string PathToUtf8(const std::filesystem::path& path) {
#ifdef _WIN32
    return Platform::TextEncoding::WideToUtf8(path.wstring());
#else
    return path.string();
#endif
}
