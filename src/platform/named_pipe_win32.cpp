#ifdef _WIN32

#include "named_pipe.h"
#include "system.h"
#ifdef AURO_NAMED_PIPE_SERVER_TESTING
#include "named_pipe_server_test_seam.h"
#endif
#ifdef AURO_NAMED_PIPE_CLIENT_TESTING
#include "named_pipe_client_test_seam.h"
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace Platform::NamedPipe {
namespace {

class ScopedPipeHandle {
public:
    explicit ScopedPipeHandle(HANDLE handle) : handle_(handle) {}
    ~ScopedPipeHandle() {
        if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
    }
    HANDLE get() const { return handle_; }
    bool valid() const { return handle_ != INVALID_HANDLE_VALUE; }

private:
    HANDLE handle_;
};

uintptr_t RealCreateServer(const wchar_t* name, uint32_t inboundBytes,
                           uint32_t outboundBytes, uint32_t maxInstances,
                           unsigned long* error) {
    HANDLE handle = CreateNamedPipeW(
        name, PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        maxInstances, outboundBytes, inboundBytes, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        *error = GetLastError();
        return 0;
    }
    return reinterpret_cast<uintptr_t>(handle);
}
bool RealAccept(uintptr_t handle, unsigned long* error) {
    if (ConnectNamedPipe(reinterpret_cast<HANDLE>(handle), nullptr)) return true;
    *error = GetLastError();
    return false;
}
bool RealRead(uintptr_t handle, void* buffer, uint32_t capacity,
              uint32_t* bytesRead, unsigned long* error) {
    DWORD nativeRead = 0;
    if (!ReadFile(reinterpret_cast<HANDLE>(handle), buffer, capacity,
                  &nativeRead, nullptr)) {
        *error = GetLastError();
        return false;
    }
    *bytesRead = static_cast<uint32_t>(nativeRead);
    return true;
}
bool RealWrite(uintptr_t handle, const void* buffer, uint32_t size,
               uint32_t* bytesWritten, unsigned long* error) {
    DWORD nativeWritten = 0;
    if (!WriteFile(reinterpret_cast<HANDLE>(handle), buffer, size,
                   &nativeWritten, nullptr)) {
        *error = GetLastError();
        return false;
    }
    *bytesWritten = static_cast<uint32_t>(nativeWritten);
    return true;
}
bool RealFlush(uintptr_t handle, unsigned long* error) {
    if (FlushFileBuffers(reinterpret_cast<HANDLE>(handle))) return true;
    *error = GetLastError();
    return false;
}
bool RealDisconnect(uintptr_t handle, unsigned long* error) {
    if (DisconnectNamedPipe(reinterpret_cast<HANDLE>(handle))) return true;
    *error = GetLastError();
    return false;
}
bool RealClose(uintptr_t handle, unsigned long* error) {
    if (CloseHandle(reinterpret_cast<HANDLE>(handle))) return true;
    *error = GetLastError();
    return false;
}

#ifdef AURO_NAMED_PIPE_SERVER_TESTING
const Testing::ServerBackendOps* g_serverOps = nullptr;
#define SERVER_OP(name, realName, ...) \
    (g_serverOps ? g_serverOps->name(__VA_ARGS__) : realName(__VA_ARGS__))
#else
#define SERVER_OP(name, realName, ...) realName(__VA_ARGS__)
#endif

#ifdef AURO_NAMED_PIPE_CLIENT_TESTING
struct ClientOpsTable {
    uintptr_t (*open)(const wchar_t*, unsigned long*) = nullptr;
    bool (*wait)(const wchar_t*, uint32_t, unsigned long*) = nullptr;
    void (*sleep)(uint32_t) = nullptr;
    uint64_t (*clock)() = nullptr;
    bool (*setMode)(uintptr_t, unsigned long*) = nullptr;
    bool (*write)(uintptr_t, const void*, uint32_t, uint32_t*, unsigned long*) = nullptr;
    bool (*peek)(uintptr_t, uint32_t*, unsigned long*) = nullptr;
    bool (*read)(uintptr_t, void*, uint32_t, uint32_t*, unsigned long*) = nullptr;
    bool (*close)(uintptr_t, unsigned long*) = nullptr;
} g_clientOps;
bool g_clientOpsEnabled = false;
#endif

uintptr_t ClientOpen(const wchar_t* name, unsigned long* error) {
#ifdef AURO_NAMED_PIPE_CLIENT_TESTING
    if (g_clientOpsEnabled) return g_clientOps.open(name, error);
#endif
    HANDLE handle = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0,
                                nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) { *error = GetLastError(); return 0; }
    return reinterpret_cast<uintptr_t>(handle);
}
bool ClientWait(const wchar_t* name, uint32_t timeout, unsigned long* error) {
#ifdef AURO_NAMED_PIPE_CLIENT_TESTING
    if (g_clientOpsEnabled) return g_clientOps.wait(name, timeout, error);
#endif
    if (WaitNamedPipeW(name, timeout)) return true;
    *error = GetLastError(); return false;
}
void ClientSleep(uint32_t milliseconds) {
#ifdef AURO_NAMED_PIPE_CLIENT_TESTING
    if (g_clientOpsEnabled) { g_clientOps.sleep(milliseconds); return; }
    ::Sleep(milliseconds);
#else
    Platform::SleepMilliseconds(milliseconds);
#endif
}
uint64_t ClientClock() {
#ifdef AURO_NAMED_PIPE_CLIENT_TESTING
    if (g_clientOpsEnabled) return g_clientOps.clock();
    return static_cast<uint64_t>(GetTickCount64());
#else
    return Platform::MonotonicMilliseconds();
#endif
}
bool ClientSetMode(uintptr_t handle, unsigned long* error) {
#ifdef AURO_NAMED_PIPE_CLIENT_TESTING
    if (g_clientOpsEnabled) return g_clientOps.setMode(handle, error);
#endif
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (SetNamedPipeHandleState(reinterpret_cast<HANDLE>(handle), &mode, nullptr, nullptr)) return true;
    *error = GetLastError(); return false;
}
bool ClientWrite(uintptr_t handle, const void* data, uint32_t size,
                 uint32_t* written, unsigned long* error) {
#ifdef AURO_NAMED_PIPE_CLIENT_TESTING
    if (g_clientOpsEnabled) return g_clientOps.write(handle, data, size, written, error);
#endif
    DWORD nativeWritten = 0;
    if (WriteFile(reinterpret_cast<HANDLE>(handle), data, size, &nativeWritten, nullptr)) {
        *written = static_cast<uint32_t>(nativeWritten); return true;
    }
    *error = GetLastError(); return false;
}
bool ClientPeek(uintptr_t handle, uint32_t* available, unsigned long* error) {
#ifdef AURO_NAMED_PIPE_CLIENT_TESTING
    if (g_clientOpsEnabled) return g_clientOps.peek(handle, available, error);
#endif
    DWORD nativeAvailable = 0;
    if (PeekNamedPipe(reinterpret_cast<HANDLE>(handle), nullptr, 0, nullptr,
                      &nativeAvailable, nullptr)) {
        *available = static_cast<uint32_t>(nativeAvailable); return true;
    }
    *error = GetLastError(); return false;
}
bool ClientRead(uintptr_t handle, void* data, uint32_t capacity,
                uint32_t* read, unsigned long* error) {
#ifdef AURO_NAMED_PIPE_CLIENT_TESTING
    if (g_clientOpsEnabled) return g_clientOps.read(handle, data, capacity, read, error);
#endif
    DWORD nativeRead = 0;
    if (ReadFile(reinterpret_cast<HANDLE>(handle), data, capacity, &nativeRead, nullptr)) {
        *read = static_cast<uint32_t>(nativeRead); return true;
    }
    *error = GetLastError(); return false;
}
bool ClientClose(uintptr_t handle, unsigned long* error) {
#ifdef AURO_NAMED_PIPE_CLIENT_TESTING
    if (g_clientOpsEnabled) return g_clientOps.close(handle, error);
#endif
    if (CloseHandle(reinterpret_cast<HANDLE>(handle))) return true;
    *error = GetLastError(); return false;
}

class OptionsPipeHandle {
public:
    explicit OptionsPipeHandle(uintptr_t handle) : handle_(handle) {}
    ~OptionsPipeHandle() {
        if (handle_ != 0) { unsigned long ignored = 0; ClientClose(handle_, &ignored); }
    }
    uintptr_t get() const { return handle_; }
    bool valid() const { return handle_ != 0; }
private:
    uintptr_t handle_;
};
} // namespace

#ifdef AURO_NAMED_PIPE_SERVER_TESTING
namespace Testing {
void SetServerBackendOps(const ServerBackendOps* ops) noexcept { g_serverOps = ops; }
void ResetServerBackendOps() noexcept { g_serverOps = nullptr; }
} // namespace Testing
#endif

#ifdef AURO_NAMED_PIPE_CLIENT_TESTING
namespace Testing {
void SetClientBackendOps(const ClientBackendOps* ops) noexcept {
    if (!ops) { g_clientOpsEnabled = false; return; }
    g_clientOps.open = ops->open; g_clientOps.wait = ops->wait; g_clientOps.sleep = ops->sleep;
    g_clientOps.clock = ops->clock; g_clientOps.setMode = ops->setMode;
    g_clientOps.write = ops->write; g_clientOps.peek = ops->peek;
    g_clientOps.read = ops->read; g_clientOps.close = ops->close;
    g_clientOpsEnabled = true;
}
void ResetClientBackendOps() noexcept { g_clientOpsEnabled = false; }
} // namespace Testing
#endif

struct ServerSession::Impl {
    uintptr_t handle = 0;
    bool connected = false;
};

ServerSession::ServerSession() : impl_(std::make_unique<Impl>()) {}
ServerSession::~ServerSession() {
    if (!IsOpen()) return;
    unsigned long ignored = 0;
    if (impl_->connected) {
        SERVER_OP(disconnect, RealDisconnect, impl_->handle, &ignored);
    }
    SERVER_OP(close, RealClose, impl_->handle, &ignored);
}
ServerSession::ServerSession(ServerSession&&) noexcept = default;
ServerSession& ServerSession::operator=(ServerSession&& other) noexcept {
    if (this == &other) return *this;
    if (IsOpen()) DisconnectAndClose();
    impl_ = std::move(other.impl_);
    return *this;
}

bool ServerSession::IsOpen() const noexcept {
    return impl_ && impl_->handle != 0;
}

ServerOperationResult ServerSession::Open(std::wstring_view pipeName,
                                          uint32_t inboundBytes,
                                          uint32_t outboundBytes,
                                          uint32_t maxInstances) {
    if (pipeName.empty() || pipeName.find(L'\0') != std::wstring_view::npos ||
        inboundBytes == 0 || outboundBytes == 0 || maxInstances == 0) {
        return {ServerSessionStatus::InvalidArgument};
    }
    if (IsOpen()) DisconnectAndClose();
    if (!impl_) impl_ = std::make_unique<Impl>();
    const std::wstring name(pipeName);
    unsigned long error = 0;
    impl_->handle = SERVER_OP(create, RealCreateServer, name.c_str(), inboundBytes,
                              outboundBytes, maxInstances, &error);
    if (!impl_->handle) return {ServerSessionStatus::CreateFailed, error};
    return {ServerSessionStatus::Ok};
}

ServerOperationResult ServerSession::Accept() {
    if (!IsOpen()) return {ServerSessionStatus::InvalidArgument};
    unsigned long error = 0;
    const bool accepted = SERVER_OP(accept, RealAccept, impl_->handle, &error);
    if (!accepted && error != ERROR_PIPE_CONNECTED) {
        return {ServerSessionStatus::AcceptFailed, error};
    }
    impl_->connected = true;
    return {ServerSessionStatus::Ok};
}

ServerReadResult ServerSession::Read(uint32_t maxMessageBytes) {
    ServerReadResult result;
    if (!IsOpen() || maxMessageBytes == 0) {
        result.status = ServerSessionStatus::InvalidArgument;
        return result;
    }
    result.message.resize(maxMessageBytes);
    uint32_t bytesRead = 0;
    unsigned long error = 0;
    if (!SERVER_OP(read, RealRead, impl_->handle, result.message.data(),
                   maxMessageBytes, &bytesRead, &error)) {
        result.message.clear();
        result.status = ServerSessionStatus::ReadFailed;
        result.systemError = error;
        return result;
    }
    result.message.resize(bytesRead);
    result.status = bytesRead == 0 ? ServerSessionStatus::EmptyMessage
                                   : ServerSessionStatus::Ok;
    return result;
}

ServerOperationResult ServerSession::WriteAndFlush(std::string_view response) {
    if (!IsOpen() || response.size() > (std::numeric_limits<uint32_t>::max)()) {
        return {ServerSessionStatus::InvalidArgument};
    }
    uint32_t written = 0;
    unsigned long writeError = 0;
    const bool wrote = SERVER_OP(write, RealWrite, impl_->handle, response.data(),
                                 static_cast<uint32_t>(response.size()), &written,
                                 &writeError);
    unsigned long flushError = 0;
    const bool flushed = SERVER_OP(flush, RealFlush, impl_->handle, &flushError);
    if (!wrote || written != response.size()) {
        if (wrote) writeError = ERROR_WRITE_FAULT;
        return {ServerSessionStatus::WriteFailed, writeError,
                flushed ? 0 : flushError};
    }
    if (!flushed) return {ServerSessionStatus::FlushFailed, flushError};
    return {ServerSessionStatus::Ok};
}

ServerOperationResult ServerSession::DisconnectAndClose() {
    if (!IsOpen()) return {ServerSessionStatus::InvalidArgument};
    unsigned long disconnectError = 0;
    bool disconnected = true;
    if (impl_->connected) {
        disconnected = SERVER_OP(disconnect, RealDisconnect, impl_->handle,
                                 &disconnectError);
    }
    unsigned long closeError = 0;
    const bool closed = SERVER_OP(close, RealClose, impl_->handle, &closeError);
    impl_->handle = 0;
    impl_->connected = false;
    if (!disconnected) {
        return {ServerSessionStatus::DisconnectFailed, disconnectError,
                closed ? 0 : closeError};
    }
    if (!closed) return {ServerSessionStatus::CloseFailed, closeError};
    return {ServerSessionStatus::Ok};
}

MessageExchangeResult ExchangeMessage(std::wstring_view pipeName,
                                      std::string_view request,
                                      uint32_t timeoutMilliseconds) {
    MessageExchangeResult result;
    const std::wstring path(pipeName);
    const uint64_t connectDeadline =
        Platform::MonotonicMilliseconds() + static_cast<uint64_t>(timeoutMilliseconds);
    HANDLE opened = INVALID_HANDLE_VALUE;

    while (Platform::MonotonicMilliseconds() < connectDeadline) {
        opened = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                             nullptr, OPEN_EXISTING, 0, nullptr);
        if (opened != INVALID_HANDLE_VALUE) break;

        result.systemError = GetLastError();
        if (result.systemError == ERROR_PIPE_BUSY) {
            WaitNamedPipeW(path.c_str(), std::min(timeoutMilliseconds, 200u));
        } else if (result.systemError == ERROR_FILE_NOT_FOUND) {
            Platform::SleepMilliseconds(25);
        } else {
            result.status = MessageExchangeStatus::ConnectFailed;
            return result;
        }
    }

    ScopedPipeHandle pipe(opened);
    if (!pipe.valid()) {
        result.status = MessageExchangeStatus::ConnectFailed;
        return result;
    }

    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe.get(), &mode, nullptr, nullptr)) {
        result.systemError = GetLastError();
        result.status = MessageExchangeStatus::MessageModeFailed;
        return result;
    }
    if (request.size() > static_cast<size_t>((std::numeric_limits<DWORD>::max)())) {
        result.systemError = ERROR_INSUFFICIENT_BUFFER;
        result.status = MessageExchangeStatus::WriteFailed;
        return result;
    }

    DWORD written = 0;
    if (!WriteFile(pipe.get(), request.data(), static_cast<DWORD>(request.size()),
                   &written, nullptr) || written != request.size()) {
        result.systemError = GetLastError();
        result.status = MessageExchangeStatus::WriteFailed;
        return result;
    }

    const uint64_t responseDeadline =
        Platform::MonotonicMilliseconds() + static_cast<uint64_t>(timeoutMilliseconds);
    while (Platform::MonotonicMilliseconds() < responseDeadline) {
        DWORD available = 0;
        if (!PeekNamedPipe(pipe.get(), nullptr, 0, nullptr, &available, nullptr)) {
            result.systemError = GetLastError();
            result.status = MessageExchangeStatus::ResponseWaitFailed;
            return result;
        }
        if (available == 0) {
            Platform::SleepMilliseconds(5);
            continue;
        }

        std::string response((std::max<DWORD>)(available, 256), '\0');
        DWORD bytesRead = 0;
        if (!ReadFile(pipe.get(), response.data(), static_cast<DWORD>(response.size()),
                      &bytesRead, nullptr)) {
            result.systemError = GetLastError();
            result.status = MessageExchangeStatus::ReadFailed;
            return result;
        }
        response.resize(bytesRead);
        result.response = std::move(response);
        result.status = MessageExchangeStatus::Ok;
        return result;
    }

    result.status = MessageExchangeStatus::TimedOut;
    return result;
}

MessageExchangeResult ExchangeMessageWithOptions(
    std::wstring_view pipeName, std::string_view request,
    uint32_t timeoutMilliseconds, const MessageExchangeOptions& options) {
    MessageExchangeResult result;
    const std::wstring path(pipeName);
    const bool fixedAttempts = options.connectAttempts != 0;
    const uint64_t connectDeadline =
        fixedAttempts ? 0 : ClientClock() + static_cast<uint64_t>(timeoutMilliseconds);
    uintptr_t opened = 0;
    uint32_t attempts = 0;
    while (fixedAttempts ? attempts < options.connectAttempts
                         : ClientClock() < connectDeadline) {
        unsigned long error = 0;
        opened = ClientOpen(path.c_str(), &error);
        ++attempts;
        if (opened != 0) break;
        result.systemError = error;
        if (error == ERROR_PIPE_BUSY) {
            const uint32_t waitMs = options.busyWaitMs != 0
                ? options.busyWaitMs
                : (std::min)(timeoutMilliseconds, 200u);
            ClientWait(path.c_str(), waitMs, &error);
        } else if (error == ERROR_FILE_NOT_FOUND) {
            const uint32_t sleepMs = options.notFoundSleepMs != 0
                ? options.notFoundSleepMs : 25u;
            ClientSleep(sleepMs);
        } else {
            result.status = MessageExchangeStatus::ConnectFailed;
            return result;
        }
    }
    if (opened == 0) {
        result.status = MessageExchangeStatus::ConnectFailed;
        result.retryExhausted = fixedAttempts;
        return result;
    }

    OptionsPipeHandle pipe(opened);
    unsigned long error = 0;
    if (!ClientSetMode(pipe.get(), &error)) {
        if (!options.ignoreMessageModeFailure) {
            result.systemError = error;
            result.status = MessageExchangeStatus::MessageModeFailed;
            return result;
        }
        error = 0;
    }
    if (request.size() > static_cast<size_t>((std::numeric_limits<DWORD>::max)())) {
        result.systemError = ERROR_INSUFFICIENT_BUFFER;
        result.status = MessageExchangeStatus::WriteFailed;
        return result;
    }
    uint32_t written = 0;
    const bool writeOk = ClientWrite(pipe.get(), request.data(),
                                     static_cast<uint32_t>(request.size()), &written, &error);
    if (!writeOk || written != request.size()) {
        result.systemError = writeOk ? ERROR_WRITE_FAULT : error;
        result.status = MessageExchangeStatus::WriteFailed;
        return result;
    }

    const uint64_t responseDeadline =
        ClientClock() + static_cast<uint64_t>(timeoutMilliseconds);
    while (ClientClock() < responseDeadline) {
        uint32_t available = 0;
        if (!ClientPeek(pipe.get(), &available, &error)) {
            result.systemError = error;
            if (!options.continueAfterPeekOrReadFailure) {
                result.status = MessageExchangeStatus::ResponseWaitFailed;
                return result;
            }
            ClientSleep(options.responsePollMs);
            continue;
        }
        if (available == 0) {
            ClientSleep(options.responsePollMs);
            continue;
        }
        const uint32_t capacity = options.responseCapacity != 0
            ? options.responseCapacity
            : (std::max<DWORD>)(available, 256);
        std::string response(capacity, '\0');
        uint32_t bytesRead = 0;
        if (!ClientRead(pipe.get(), response.data(), capacity, &bytesRead, &error)) {
            result.systemError = error;
            if (!options.continueAfterPeekOrReadFailure) {
                result.status = MessageExchangeStatus::ReadFailed;
                return result;
            }
            ClientSleep(options.responsePollMs);
            continue;
        }
        if (bytesRead == 0) {
            if (!options.continueAfterPeekOrReadFailure) {
                result.systemError = ERROR_BROKEN_PIPE;
                result.status = MessageExchangeStatus::ReadFailed;
                return result;
            }
            ClientSleep(options.responsePollMs);
            continue;
        }
        response.resize((std::min)(bytesRead, capacity));
        if (options.trimTrailingCrLf) {
            while (!response.empty() &&
                   (response.back() == '\n' || response.back() == '\r')) {
                response.pop_back();
            }
        }
        result.response = std::move(response);
        result.status = MessageExchangeStatus::Ok;
        return result;
    }
    result.status = MessageExchangeStatus::TimedOut;
    return result;
}

} // namespace Platform::NamedPipe

#endif // _WIN32
