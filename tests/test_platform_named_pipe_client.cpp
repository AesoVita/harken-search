#include "platform/named_pipe.h"
#include "platform/named_pipe_client_test_seam.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {
using namespace Platform::NamedPipe;
using Platform::NamedPipe::Testing::ClientBackendOps;

struct State {
    std::vector<std::pair<std::uintptr_t, unsigned long>> opens;
    std::vector<bool> peeks, reads;
    std::vector<std::uint32_t> available, readBytes, ticks, waits, sleeps, capacities;
    std::size_t openAt = 0, peekAt = 0, readAt = 0, tickAt = 0;
    unsigned long lastError = 0, modeError = 0, writeError = 0;
    bool advanceClockAfterSleep = false;
    bool waitOk = false, modeOk = true, writeOk = true;
    std::uint32_t writeBytes = UINT32_MAX;
    int modeCalls = 0, writeCalls = 0, closes = 0;
} g;
int failures = 0;

void Check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
void Reset() {
    g = {};
    g.opens = {{42, 0}};
    g.peeks = {true}; g.available = {1};
    g.reads = {true}; g.readBytes = {1};
    g.ticks = {100, 100};
    g.modeOk = g.writeOk = true; g.writeBytes = UINT32_MAX;
}
std::uintptr_t Open(const wchar_t*, unsigned long* error) {
    if (g.openAt >= g.opens.size()) return 0;
    const auto step = g.opens[g.openAt++]; *error = step.second; return step.first;
}
bool Wait(const wchar_t*, std::uint32_t ms, unsigned long*) {
    g.waits.push_back(ms); return g.waitOk;
}
void Sleep(std::uint32_t ms) { g.sleeps.push_back(ms); }
std::uint64_t Clock() {
    if (g.advanceClockAfterSleep) return g.sleeps.empty() ? 100 : 110;
    if (g.ticks.empty()) return 0;
    return g.tickAt < g.ticks.size() ? g.ticks[g.tickAt++] : g.ticks.back();
}
bool SetMode(std::uintptr_t, unsigned long* error) {
    ++g.modeCalls; *error = g.modeError; return g.modeOk;
}
bool Write(std::uintptr_t, const void*, std::uint32_t size, std::uint32_t* written,
           unsigned long* error) {
    ++g.writeCalls; *error = g.writeError;
    *written = g.writeBytes == UINT32_MAX ? size : g.writeBytes; return g.writeOk;
}
bool Peek(std::uintptr_t, std::uint32_t* count, unsigned long*) {
    if (g.peekAt >= g.peeks.size()) { *count = 0; return true; }
    *count = g.available[g.peekAt]; return g.peeks[g.peekAt++];
}
bool Read(std::uintptr_t, void* data, std::uint32_t capacity, std::uint32_t* bytes,
          unsigned long*) {
    g.capacities.push_back(capacity);
    if (g.readAt >= g.reads.size()) { *bytes = 0; return false; }
    *bytes = g.readBytes[g.readAt];
    if (g.reads[g.readAt] && *bytes) std::memset(data, 'x', (std::min)(capacity, *bytes));
    return g.reads[g.readAt++];
}
bool Close(std::uintptr_t, unsigned long*) { ++g.closes; return true; }
const ClientBackendOps kOps{Open, Wait, Sleep, Clock, SetMode, Write, Peek, Read, Close};

MessageExchangeOptions Compat() {
    MessageExchangeOptions options;
    options.connectAttempts = 5; options.busyWaitMs = 250; options.notFoundSleepMs = 25;
    options.responsePollMs = 10; options.responseCapacity = 2047;
    options.ignoreMessageModeFailure = true;
    options.continueAfterPeekOrReadFailure = true; options.trimTrailingCrLf = true;
    return options;
}
MessageExchangeResult Exchange(const MessageExchangeOptions& options, std::uint32_t timeout = 10) {
    return ExchangeMessageWithOptions(LR"(\\.\pipe\auro_client_options_test)", "HELLO", timeout, options);
}
}

int main() {
    Testing::SetClientBackendOps(&kOps);
    {
        Reset(); g.opens.assign(5, {0, 2});
        const auto result = Exchange(Compat());
        Check(result.status == MessageExchangeStatus::ConnectFailed && result.retryExhausted &&
                  g.sleeps == std::vector<std::uint32_t>(5, 25) && g.closes == 0,
              "options distinguish exhausted missing retries");
    }
    {
        Reset(); g.opens = {{0, 231}, {0, 2}, {42, 0}};
        const auto result = Exchange(Compat());
        Check(result.status == MessageExchangeStatus::Ok && !result.retryExhausted &&
                  g.waits == std::vector<std::uint32_t>{250} &&
                  g.sleeps == std::vector<std::uint32_t>{25} && g.closes == 1,
              "options preserve busy/missing cadence and ownership");
    }
    {
        Reset(); g.opens = {{0, 5}};
        const auto result = Exchange(Compat());
        Check(result.status == MessageExchangeStatus::ConnectFailed && !result.retryExhausted &&
                  result.systemError == 5 && g.openAt == 1,
              "options distinguish immediate connect failure");
    }
    {
        Reset(); g.modeOk = false;
        const auto result = Exchange(Compat());
        Check(result.status == MessageExchangeStatus::Ok && g.modeCalls == 1 && g.closes == 1,
              "options can ignore message-mode failure");
    }
    {
        Reset(); g.writeOk = false; g.writeBytes = 0; g.writeError = 55;
        const auto result = Exchange(Compat());
        Check(result.status == MessageExchangeStatus::WriteFailed && result.systemError == 55 &&
                  g.writeCalls == 1 && g.closes == 1,
              "options preserve write failure and close");
        Reset(); g.modeOk = false; g.modeError = 77; g.writeBytes = 1;
        const auto shortResult = Exchange(Compat());
        Check(shortResult.status == MessageExchangeStatus::WriteFailed &&
                  shortResult.systemError == 29 && g.closes == 1,
              "options reject short write and close");
    }
    {
        Reset(); g.peeks = {false}; g.ticks = {100, 100, 110};
        auto result = Exchange(Compat());
        Check(result.status == MessageExchangeStatus::TimedOut && g.sleeps == std::vector<std::uint32_t>{10} &&
                  g.closes == 1, "options continue after peek failure to timeout");
        Reset(); g.reads = {false}; g.ticks = {100, 100, 110};
        result = Exchange(Compat());
        Check(result.status == MessageExchangeStatus::TimedOut && g.sleeps == std::vector<std::uint32_t>{10} &&
                  g.closes == 1, "options continue after read failure to timeout");
        Reset(); g.advanceClockAfterSleep = true; g.readBytes = {0};
        result = Exchange(Compat());
        Check(result.status == MessageExchangeStatus::TimedOut &&
                  g.sleeps == std::vector<std::uint32_t>{10} && g.closes == 1,
              "options continue after successful zero-byte read to timeout");
        Reset(); g.readBytes = {0};
        auto strict = Compat(); strict.continueAfterPeekOrReadFailure = false;
        result = Exchange(strict);
        Check(result.status == MessageExchangeStatus::ReadFailed && result.systemError == 109 &&
                  g.closes == 1, "options reject zero-byte read deterministically");
    }
    {
        Reset(); g.available = {2048}; g.readBytes = {2047};
        const auto result = Exchange(Compat());
        Check(result.status == MessageExchangeStatus::Ok && g.capacities == std::vector<std::uint32_t>{2047} &&
                  g.closes == 1, "options enforce 2047 response capacity");
    }
    Testing::ResetClientBackendOps();
    return failures == 0 ? 0 : 1;
}
