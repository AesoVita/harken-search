import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CLIENT = (ROOT / "src" / "ReaperBridgeClient.cpp").read_text(encoding="utf-8")
SYSTEM_HEADER = (ROOT / "src" / "platform" / "system.h").read_text(encoding="utf-8")
SYSTEM_WIN32 = (ROOT / "src" / "platform" / "system_win32.cpp").read_text(
    encoding="utf-8"
)
SYSTEM_MACOS = (ROOT / "src" / "platform" / "system_macos.mm").read_text(
    encoding="utf-8"
)
NAMED_HEADER_PATH = ROOT / "src" / "platform" / "named_pipe.h"
NAMED_WIN32_PATH = ROOT / "src" / "platform" / "named_pipe_win32.cpp"
NAMED_MACOS_PATH = ROOT / "src" / "platform" / "named_pipe_macos.mm"
NAMED_HEADER = NAMED_HEADER_PATH.read_text(encoding="utf-8") if NAMED_HEADER_PATH.exists() else ""
NAMED_WIN32 = NAMED_WIN32_PATH.read_text(encoding="utf-8") if NAMED_WIN32_PATH.exists() else ""
NAMED_MACOS = NAMED_MACOS_PATH.read_text(encoding="utf-8") if NAMED_MACOS_PATH.exists() else ""


class ReaperBridgeClientPlatformContract(unittest.TestCase):
    def test_monotonic_clock_and_sleep_use_system_pal(self):
        self.assertNotIn('#include "platform/system.h"', CLIENT)
        self.assertNotIn("Platform::MonotonicMilliseconds()", CLIENT)
        legacy = NAMED_WIN32.split("MessageExchangeResult ExchangeMessage(", 1)[1].split(
            "MessageExchangeResult ExchangeMessageWithOptions", 1
        )[0]
        self.assertEqual(legacy.count("MonotonicMilliseconds()"), 4)
        self.assertIn("SleepMilliseconds(25);", legacy)
        self.assertIn("SleepMilliseconds(5);", legacy)
        self.assertNotIn("GetTickCount64", CLIENT)
        self.assertNotIn("Sleep(", CLIENT)

    def test_pipe_transport_calls_remain_native_in_this_slice(self):
        self.assertIn('#include "platform/named_pipe.h"', CLIENT)
        self.assertIn("Platform::NamedPipe::ExchangeMessage", CLIENT)
        self.assertIn("std::numeric_limits<uint32_t>::max()", CLIENT)
        for native_token in (
            "windows.h",
            "CreateFileW",
            "GetLastError",
            "WaitNamedPipeW",
            "SetNamedPipeHandleState",
            "WriteFile",
            "PeekNamedPipe",
            "ReadFile",
            "CloseHandle",
            "GetTickCount64",
            "Sleep(",
        ):
            self.assertNotIn(native_token, CLIENT)

    def test_named_pipe_header_is_native_free_and_statuses_map_in_client(self):
        self.assertIn("namespace Platform::NamedPipe", NAMED_HEADER)
        self.assertIn("enum class MessageExchangeStatus", NAMED_HEADER)
        self.assertIn("struct MessageExchangeResult", NAMED_HEADER)
        self.assertIn("MessageExchangeResult ExchangeMessage(", NAMED_HEADER)
        for native_token in ("windows.h", "HANDLE", "CreateFileW", "CloseHandle"):
            self.assertNotIn(native_token, NAMED_HEADER)
        for status in (
            "Unsupported",
            "Ok",
            "ConnectFailed",
            "MessageModeFailed",
            "WriteFailed",
            "ResponseWaitFailed",
            "ReadFailed",
            "TimedOut",
        ):
            self.assertIn(f"MessageExchangeStatus::{status}", CLIENT)

    def test_named_pipe_platform_ownership_and_cadence(self):
        for native_call in (
            "CreateFileW",
            "GetLastError",
            "WaitNamedPipeW",
            "SetNamedPipeHandleState",
            "WriteFile",
            "PeekNamedPipe",
            "ReadFile",
            "CloseHandle",
        ):
            self.assertIn(native_call, NAMED_WIN32)
        self.assertIn("class ScopedPipeHandle", NAMED_WIN32)
        self.assertIn("Platform::MonotonicMilliseconds", NAMED_WIN32)
        self.assertIn("Platform::SleepMilliseconds(25)", NAMED_WIN32)
        self.assertIn("Platform::SleepMilliseconds(5)", NAMED_WIN32)
        self.assertIn("std::min(timeoutMilliseconds, 200u)", NAMED_WIN32)
        self.assertIn("MessageExchangeStatus::Unsupported", NAMED_MACOS)

    def test_named_pipe_compatibility_options_contract(self):
        self.assertIn("struct MessageExchangeOptions", NAMED_HEADER)
        for field in (
            "connectAttempts", "busyWaitMs", "notFoundSleepMs", "responsePollMs",
            "responseCapacity", "ignoreMessageModeFailure",
            "continueAfterPeekOrReadFailure", "trimTrailingCrLf",
        ):
            self.assertIn(field, NAMED_HEADER)
        self.assertIn("bool retryExhausted", NAMED_HEADER)
        self.assertIn("ExchangeMessageWithOptions", NAMED_HEADER)
        self.assertIn("ExchangeMessageWithOptions", NAMED_WIN32)
        self.assertIn("AURO_NAMED_PIPE_CLIENT_TESTING", NAMED_WIN32)

    def test_named_pipe_client_seam_compiles_out_and_product_timing_uses_pal(self):
        table = NAMED_WIN32.split("struct ClientOpsTable", 1)
        self.assertEqual(2, len(table))
        self.assertTrue(table[0].rstrip().endswith("#ifdef AURO_NAMED_PIPE_CLIENT_TESTING"))
        self.assertIn("bool g_clientOpsEnabled = false;\n#endif", table[1])
        self.assertEqual(9, NAMED_WIN32.count("if (g_clientOpsEnabled)"))
        helpers = NAMED_WIN32.split("struct ClientOpsTable", 1)[1].split(
            "class OptionsPipeHandle", 1
        )[0]
        for name in ("ClientOpen", "ClientWait", "ClientSetMode", "ClientWrite",
                     "ClientPeek", "ClientRead", "ClientClose"):
            body = helpers.split(name, 1)[1].split("\n}", 1)[0]
            self.assertLess(body.index("#ifdef AURO_NAMED_PIPE_CLIENT_TESTING"),
                            body.index("if (g_clientOpsEnabled)"))
            self.assertLess(body.index("if (g_clientOpsEnabled)"), body.index("#endif"))
        for name, test_call, product_call in (
            ("ClientSleep", "::Sleep(milliseconds)", "Platform::SleepMilliseconds(milliseconds)"),
            ("ClientClock", "GetTickCount64()", "Platform::MonotonicMilliseconds()"),
        ):
            body = helpers.split(name, 1)[1].split("\n}", 1)[0]
            self.assertLess(body.index("if (g_clientOpsEnabled)"), body.index(test_call))
            self.assertLess(body.index(test_call), body.index("#else"))
            self.assertLess(body.index("#else"), body.index(product_call))
            self.assertLess(body.index(product_call), body.index("#endif"))
        options = NAMED_WIN32.split("MessageExchangeResult ExchangeMessageWithOptions", 1)[1]
        self.assertIn("fixedAttempts ? 0 : ClientClock()", options)

    def test_named_pipe_options_have_unsupported_macos_parity(self):
        self.assertIn("ExchangeMessageWithOptions", NAMED_MACOS)
        self.assertIn("MessageExchangeStatus::Unsupported", NAMED_MACOS)

    def test_named_pipe_server_unsupported_backend_has_no_ownership(self):
        for method in (
            "ServerSession::ServerSession()",
            "ServerSession::Open(",
            "ServerSession::Accept()",
            "ServerSession::Read(",
            "ServerSession::WriteAndFlush(",
            "ServerSession::DisconnectAndClose()",
            "ServerSession::IsOpen()",
        ):
            self.assertIn(method, NAMED_MACOS)
        self.assertIn("ServerSessionStatus::Unsupported", NAMED_MACOS)
        self.assertIn("return false;", NAMED_MACOS)
        self.assertNotIn("HANDLE", NAMED_MACOS)
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertEqual(1, cmake.count("src/platform/named_pipe_macos.mm"))

    def test_system_pal_owns_platform_specific_clock_and_sleep(self):
        self.assertIn("uint64_t MonotonicMilliseconds();", SYSTEM_HEADER)
        self.assertIn("void SleepMilliseconds(uint32_t milliseconds);", SYSTEM_HEADER)
        self.assertIn("GetTickCount64()", SYSTEM_WIN32)
        self.assertIn("Sleep(milliseconds)", SYSTEM_WIN32)
        self.assertIn("std::chrono::steady_clock", SYSTEM_MACOS)
        self.assertIn("std::this_thread::sleep_for", SYSTEM_MACOS)


if __name__ == "__main__":
    unittest.main()
