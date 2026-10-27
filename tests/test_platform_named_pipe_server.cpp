#include "platform/named_pipe.h"
#include "platform/named_pipe_server_test_seam.h"

#include <cstring>
#include <iostream>
#include <string>
#include <utility>

namespace Platform {
uint64_t MonotonicMilliseconds() { return 0; }
void SleepMilliseconds(uint32_t) {}
}

namespace {
using namespace Platform::NamedPipe;

struct State {
    bool acceptOk = true, readOk = true, writeOk = true;
    bool flushOk = true, disconnectOk = true, closeOk = true;
    unsigned long acceptError = 0, readError = 0, writeError = 0;
    unsigned long flushError = 0, disconnectError = 0, closeError = 0;
    uint32_t bytesRead = 1, bytesWritten = 1;
    int creates = 0, accepts = 0, reads = 0, writes = 0;
    int flushes = 0, disconnects = 0, closes = 0;
} g;

uintptr_t Create(const wchar_t*, uint32_t, uint32_t, uint32_t, unsigned long*) {
    ++g.creates;
    return 7;
}
bool Accept(uintptr_t, unsigned long* error) {
    ++g.accepts; *error = g.acceptError; return g.acceptOk;
}
bool Read(uintptr_t, void* buffer, uint32_t capacity, uint32_t* read,
          unsigned long* error) {
    ++g.reads; *error = g.readError; *read = g.bytesRead;
    if (g.readOk && g.bytesRead && capacity) std::memcpy(buffer, "x", 1);
    return g.readOk;
}
bool Write(uintptr_t, const void*, uint32_t size, uint32_t* written,
           unsigned long* error) {
    ++g.writes; *error = g.writeError;
    *written = g.bytesWritten == UINT32_MAX ? size : g.bytesWritten;
    return g.writeOk;
}
bool Flush(uintptr_t, unsigned long* error) {
    ++g.flushes; *error = g.flushError; return g.flushOk;
}
bool Disconnect(uintptr_t, unsigned long* error) {
    ++g.disconnects; *error = g.disconnectError; return g.disconnectOk;
}
bool Close(uintptr_t, unsigned long* error) {
    ++g.closes; *error = g.closeError; return g.closeOk;
}

const Testing::ServerBackendOps kOps{
    Create, Accept, Read, Write, Flush, Disconnect, Close};
int failures = 0;
void Check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
void Reset() { g = {}; g.bytesWritten = UINT32_MAX; }
ServerSession OpenSession() {
    ServerSession session;
    Check(session.Open(LR"(\\.\pipe\auro_test)", 4096, 4096).status ==
              ServerSessionStatus::Ok,
          "open succeeds");
    return session;
}
}

int main() {
    Testing::SetServerBackendOps(&kOps);
    {
        Reset(); auto session = OpenSession();
        g.acceptOk = false; g.acceptError = 535;
        Check(session.Accept().status == ServerSessionStatus::Ok,
              "ERROR_PIPE_CONNECTED accepts");
        g.readOk = false; g.readError = 23;
        auto failedRead = session.Read(4095);
        Check(failedRead.status == ServerSessionStatus::ReadFailed &&
                  failedRead.systemError == 23 && failedRead.message.empty(),
              "read failure maps error and no message");
        g.readOk = true; g.bytesRead = 0;
        Check(session.Read(4095).status == ServerSessionStatus::EmptyMessage,
              "zero-byte read is empty message");
    }
    {
        Reset(); auto session = OpenSession(); session.Accept();
        g.bytesWritten = 1; g.flushOk = false; g.flushError = 55;
        auto partial = session.WriteAndFlush("abc");
        Check(partial.status == ServerSessionStatus::WriteFailed &&
                  partial.systemError == 29 && partial.secondarySystemError == 55,
              "short write uses deterministic primary and flush secondary");
        g.writeOk = false; g.writeError = 5; g.flushError = 6;
        auto both = session.WriteAndFlush("abc");
        Check(both.status == ServerSessionStatus::WriteFailed &&
                  both.systemError == 5 && both.secondarySystemError == 6,
              "write failure wins over flush failure");
    }
    {
        Reset(); auto source = OpenSession(); source.Accept();
        { ServerSession moved(std::move(source)); Check(!source.IsOpen(), "move empties source"); }
        Check(g.disconnects == 1 && g.closes == 1,
              "moved session destructor disconnects and closes once");
    }
    {
        Reset(); auto source = OpenSession(); source.Accept();
        auto destination = OpenSession(); destination.Accept();
        destination = std::move(source);
        Check(g.disconnects == 1 && g.closes == 1,
              "move assignment releases the destination exactly once");
    }
    {
        Reset(); auto session = OpenSession(); session.Accept();
        g.disconnectOk = false; g.disconnectError = 8;
        g.closeOk = false; g.closeError = 9;
        auto result = session.DisconnectAndClose();
        Check(result.status == ServerSessionStatus::DisconnectFailed &&
                  result.systemError == 8 && result.secondarySystemError == 9 &&
                  !session.IsOpen() && g.disconnects == 1 && g.closes == 1,
              "disconnect wins, close is secondary, ownership clears");
    }
    Testing::ResetServerBackendOps();
    return failures == 0 ? 0 : 1;
}
