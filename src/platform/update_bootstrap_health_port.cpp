#include "platform/update_bootstrap_health_port.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "nlohmann_json.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Platform::UpdateBootstrapHealth {
namespace {

// The updated process may spend several seconds loading WebView2, the
// database, and startup modules before it can read the inherited channel.
// Keep this admission window aligned with the bootstrap health wait instead
// of treating slow startup as a failed replacement.
constexpr DWORD kChallengeWaitMs = 120'000U;

bool IsPipe(HANDLE handle) {
    return handle != nullptr && handle != INVALID_HANDLE_VALUE &&
           GetFileType(handle) == FILE_TYPE_PIPE;
}

Status ReadAvailable(HANDLE pipe, std::string& bytes) {
    const ULONGLONG start = GetTickCount64();
    DWORD available = 0U;
    for (;;) {
        if (!PeekNamedPipe(pipe, nullptr, 0U, nullptr, &available, nullptr)) {
            return Status::IoFailure;
        }
        if (available != 0U) break;
        if (GetTickCount64() - start >= kChallengeWaitMs) {
            return Status::Timeout;
        }
        Sleep(5U);
    }
    if (available > 4096U) return Status::InvalidChannel;
    std::vector<char> buffer(available);
    DWORD read = 0U;
    if (!ReadFile(pipe, buffer.data(), available, &read, nullptr) ||
        read != available) {
        return Status::IoFailure;
    }
    bytes.assign(buffer.data(), buffer.size());
    return Status::Accepted;
}

class WindowsPort final : public Port {
public:
    Result CaptureChallenge() override {
        if (captureAttempted_) {
            return {Status::InvalidChannel, "bootstrap health challenge already consumed"};
        }
        captureAttempted_ = true;
        const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!IsPipe(input) || !IsPipe(output)) {
            return {Status::InvalidChannel, "bootstrap health channel unavailable"};
        }

        std::string challenge;
        const Status readStatus = ReadAvailable(input, challenge);
        if (readStatus != Status::Accepted) {
            return {readStatus, "bootstrap health challenge read failed"};
        }
        const auto value = nlohmann::json::parse(challenge, nullptr, false);
        if (value.is_discarded() || !value.is_object() || value.size() != 3U ||
            !value.contains("build_id") || !value.contains("version") ||
            !value.contains("token") || !value["build_id"].is_string() ||
            !value["version"].is_string() || !value["token"].is_string()) {
            return {Status::InvalidChannel, "bootstrap health challenge invalid"};
        }
        output_ = output;
        buildId_ = value["build_id"].get<std::string>();
        version_ = value["version"].get<std::string>();
        token_ = value["token"].get<std::string>();
        captured_ = true;
        return {Status::Accepted, {}};
    }

    Result AcknowledgeReady() override {
        if (!captured_ || acknowledged_ || !IsPipe(output_)) {
            return {Status::InvalidChannel, "bootstrap health challenge unavailable"};
        }
        acknowledged_ = true;
        const auto acknowledgement = nlohmann::json{
            {"build_id", buildId_},
            {"process_id", static_cast<std::uint64_t>(GetCurrentProcessId())},
            {"token", token_},
            {"ui_ready", true},
            {"version", version_},
        };
        const std::string bytes = acknowledgement.dump() + "\n";
        DWORD written = 0U;
        if (WriteFile(output_, bytes.data(), static_cast<DWORD>(bytes.size()),
                      &written, nullptr) == FALSE ||
            written != bytes.size()) {
            return {Status::IoFailure, "bootstrap health acknowledgement failed"};
        }
        return {Status::Accepted, {}};
    }

private:
    bool captureAttempted_ = false;
    bool captured_ = false;
    bool acknowledged_ = false;
    HANDLE output_ = INVALID_HANDLE_VALUE;
    std::string buildId_;
    std::string version_;
    std::string token_;
};

}  // namespace

CreateResult CreateUpdateBootstrapHealthPort() {
    auto port = std::make_unique<WindowsPort>();
    return {Status::Accepted, std::move(port), 0U};
}

}  // namespace Platform::UpdateBootstrapHealth

#else

namespace Platform::UpdateBootstrapHealth {

CreateResult CreateUpdateBootstrapHealthPort() {
    return {Status::Unsupported, nullptr, 0U};
}

}  // namespace Platform::UpdateBootstrapHealth

#endif
