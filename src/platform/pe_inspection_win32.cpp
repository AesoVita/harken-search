#ifdef _WIN32

#include "pe_inspection.h"

#include "text_encoding.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string_view>
#include <vector>

namespace Platform::PeInspection {
namespace {

class ScopedPeFile {
public:
    explicit ScopedPeFile(HANDLE handle) : handle_(handle) {}
    ~ScopedPeFile() {
        if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
    }
    HANDLE get() const { return handle_; }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

class ScopedPeModule {
public:
    explicit ScopedPeModule(HMODULE module) : module_(module) {}
    ~ScopedPeModule() {
        if (module_) FreeLibrary(module_);
    }
    HMODULE get() const { return module_; }

private:
    HMODULE module_ = nullptr;
};

StringResult QueryFailed(unsigned long error) {
    StringResult result;
    result.status = Status::QueryFailed;
    result.systemError = error;
    return result;
}

StringResult ReadVersionBlock(const std::wstring& path,
                              std::vector<unsigned char>& bytes) {
    StringResult result;
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (size == 0) {
        result.status = Status::OpenFailed;
        result.systemError = GetLastError();
        return result;
    }
    bytes.resize(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, bytes.data())) {
        result.status = Status::QueryFailed;
        result.systemError = GetLastError();
        bytes.clear();
        return result;
    }
    result.status = Status::Success;
    return result;
}

StringResult InvalidPathResult(const std::string& pathUtf8,
                               std::wstring& path) {
    StringResult result;
    if (pathUtf8.empty()) {
        result.status = Status::InvalidArgument;
        return result;
    }
    path = TextEncoding::Utf8ToWideStrict(pathUtf8);
    if (path.empty()) result.status = Status::EncodingFailed;
    return result;
}

} // namespace

StringResult ReadArchitecture(const std::string& pathUtf8) {
    StringResult result;
    if (pathUtf8.empty()) {
        result.status = Status::InvalidArgument;
        return result;
    }
    const std::wstring path = TextEncoding::Utf8ToWideStrict(pathUtf8);
    if (path.empty()) {
        result.status = Status::EncodingFailed;
        return result;
    }
    ScopedPeFile file(CreateFileW(
        path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (file.get() == INVALID_HANDLE_VALUE) {
        result.status = Status::OpenFailed;
        result.systemError = GetLastError();
        return result;
    }

    IMAGE_DOS_HEADER dos = {};
    DWORD read = 0;
    if (!ReadFile(file.get(), &dos, sizeof(dos), &read, nullptr) ||
        read != sizeof(dos)) {
        return QueryFailed(GetLastError());
    }
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0) {
        result.status = Status::InvalidFormat;
        return result;
    }

    LARGE_INTEGER offset = {};
    offset.QuadPart = dos.e_lfanew;
    if (!SetFilePointerEx(file.get(), offset, nullptr, FILE_BEGIN)) {
        return QueryFailed(GetLastError());
    }
    DWORD signature = 0;
    if (!ReadFile(file.get(), &signature, sizeof(signature), &read, nullptr) ||
        read != sizeof(signature)) {
        return QueryFailed(GetLastError());
    }
    if (signature != IMAGE_NT_SIGNATURE) {
        result.status = Status::InvalidFormat;
        return result;
    }
    IMAGE_FILE_HEADER header = {};
    if (!ReadFile(file.get(), &header, sizeof(header), &read, nullptr) ||
        read != sizeof(header)) {
        return QueryFailed(GetLastError());
    }
    result.status = Status::Success;
    switch (header.Machine) {
    case IMAGE_FILE_MACHINE_I386: result.value = "x86"; break;
    case IMAGE_FILE_MACHINE_AMD64: result.value = "x64"; break;
    case IMAGE_FILE_MACHINE_ARM64: result.value = "arm64"; break;
    default: result.value = "unknown"; break;
    }
    return result;
}

StringResult ReadFileVersion(const std::string& pathUtf8) {
    std::wstring path;
    StringResult result = InvalidPathResult(pathUtf8, path);
    if (result.status != Status::Unsupported) return result;
    std::vector<unsigned char> bytes;
    result = ReadVersionBlock(path, bytes);
    if (result.status != Status::Success) return result;
    VS_FIXEDFILEINFO* info = nullptr;
    UINT infoSize = 0;
    if (!VerQueryValueW(bytes.data(), L"\\",
                        reinterpret_cast<void**>(&info), &infoSize) ||
        !info || infoSize < sizeof(VS_FIXEDFILEINFO) ||
        info->dwSignature != 0xFEEF04BD) {
        result.status = Status::InvalidFormat;
        return result;
    }
    result.value = std::to_string(HIWORD(info->dwFileVersionMS)) + "." +
                   std::to_string(LOWORD(info->dwFileVersionMS)) + "." +
                   std::to_string(HIWORD(info->dwFileVersionLS)) + "." +
                   std::to_string(LOWORD(info->dwFileVersionLS));
    return result;
}

StringResult ReadVersionString(const std::string& pathUtf8,
                               const std::string& keyUtf8) {
    std::wstring path;
    StringResult result = InvalidPathResult(pathUtf8, path);
    if (result.status != Status::Unsupported) return result;
    if (keyUtf8.empty()) {
        result.status = Status::InvalidArgument;
        return result;
    }
    const std::wstring key = TextEncoding::Utf8ToWideStrict(keyUtf8);
    if (key.empty()) {
        result.status = Status::EncodingFailed;
        return result;
    }
    std::vector<unsigned char> bytes;
    result = ReadVersionBlock(path, bytes);
    if (result.status != Status::Success) return result;
    struct Translation { WORD language; WORD codePage; };
    Translation* translations = nullptr;
    UINT translationBytes = 0;
    if (!VerQueryValueW(bytes.data(), L"\\VarFileInfo\\Translation",
                        reinterpret_cast<void**>(&translations), &translationBytes) ||
        !translations || translationBytes < sizeof(Translation)) {
        result.status = Status::ValueUnavailable;
        return result;
    }
    wchar_t query[256] = {};
    if (swprintf_s(query, L"\\StringFileInfo\\%04x%04x\\%s",
                   translations[0].language, translations[0].codePage,
                   key.c_str()) < 0) {
        result.status = Status::ValueUnavailable;
        return result;
    }
    wchar_t* value = nullptr;
    UINT chars = 0;
    if (!VerQueryValueW(bytes.data(), query,
                        reinterpret_cast<void**>(&value), &chars) ||
        !value || chars == 0) {
        result.status = Status::ValueUnavailable;
        return result;
    }
    const std::wstring_view wideValue(value, chars > 0 && value[chars - 1] == L'\0'
        ? chars - 1 : chars);
    result.value = TextEncoding::WideToUtf8Strict(wideValue);
    if (result.value.empty()) {
        result.status = Status::EncodingFailed;
        return result;
    }
    result.status = Status::Success;
    return result;
}

ExportResult ProbeExport(const std::string& pathUtf8,
                         const std::string& exportName) {
    ExportResult result;
    if (pathUtf8.empty() || exportName.empty()) {
        result.status = Status::InvalidArgument;
        return result;
    }
    const std::wstring path = TextEncoding::Utf8ToWideStrict(pathUtf8);
    const std::wstring name = TextEncoding::Utf8ToWideStrict(exportName);
    if (path.empty() || name.empty()) {
        result.status = Status::EncodingFailed;
        return result;
    }
    ScopedPeModule module(LoadLibraryExW(
        path.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES));
    if (!module.get()) {
        result.status = Status::OpenFailed;
        result.systemError = GetLastError();
        return result;
    }
    result.status = Status::Success;
    result.present = GetProcAddress(module.get(),
                                    TextEncoding::WideToUtf8Strict(name).c_str()) != nullptr;
    return result;
}

} // namespace Platform::PeInspection

#endif // _WIN32
