import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "WaapiClient.h"
SOURCE = ROOT / "src" / "WaapiClient.cpp"


HARNESS = r'''
#include "WaapiClient.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAIL: " << message << "\n";
    std::exit(1);
}

void allow(const std::string& host, int port, const std::string& path,
           const std::string& normalizedHost) {
    aura::waapi::ClientConfig cfg;
    cfg.host = host;
    cfg.port = port;
    cfg.path = path;
    const auto result = aura::waapi::detail::validatePlaintextLoopbackEndpoint(cfg);
    require(result.allowed, "allowed endpoint rejected");
    require(result.normalizedHost == normalizedHost, "host normalization changed");
}

void deny(const std::string& host, int port, const std::string& path) {
    aura::waapi::ClientConfig cfg;
    cfg.host = host;
    cfg.port = port;
    cfg.path = path;
    const auto result = aura::waapi::detail::validatePlaintextLoopbackEndpoint(cfg);
    require(!result.allowed, "unsafe endpoint accepted");
}
} // namespace

int main() {
    allow("127.0.0.1", 1, "/waapi", "127.0.0.1");
    allow("127.0.0.1", 65535, "/waapi", "127.0.0.1");
    allow("::1", 8765, "/waapi", "[::1]");
    allow("[::1]", 8765, "/waapi", "[::1]");
    allow("localhost", 8765, "/waapi", "127.0.0.1");
    allow("LOCALHOST", 8765, "/waapi", "127.0.0.1");
    allow("localhost.", 8765, "/waapi", "127.0.0.1");
    allow("LOCALHOST.", 8765, "/waapi", "127.0.0.1");

    for (const auto& host : {
             "", "127.0.0.2", "127.0.0.01", "192.168.1.10", "8.8.8.8",
             "localhost..", "localhost.evil", "127.0.0.1.evil", "2130706433",
             "0x7f000001", "::01", "::ffff:127.0.0.1", "[::2]",
             "user@localhost", "localhost/waapi", "ws://127.0.0.1",
             "127.0.0.1:8765", "[::1]:8765", "localhost ", "localhost%00"
         }) {
        deny(host, 8765, "/waapi");
    }
    deny(u8"localhost\u00E9", 8765, "/waapi");
    deny("127.0.0.1", 0, "/waapi");
    deny("127.0.0.1", 65536, "/waapi");
    deny("127.0.0.1", -1, "/waapi");

    for (const auto& path : {
             "", "/", "//waapi", "/WAAPI", "/waapi/extra", "/waapi?x=1",
             "/waapi#fragment", "/waapi\\x", "/waapi%2F", "/waapi ",
             "ws://127.0.0.1/waapi"
         }) {
        deny("127.0.0.1", 8765, path);
    }
    deny("127.0.0.1", 8765, "/waapi\t");
    deny("127.0.0.1", 8765, "/waapi\n");
    return 0;
}
'''


CONNECT_HARNESS = r'''
#include "WaapiClient.h"

#include <cstdlib>
#include <iostream>

int main() {
    aura::waapi::WaapiClient client;
    aura::waapi::ClientConfig cfg;
    // This is deliberately a malformed host-plus-port form.  A live
    // connect() must reject it through the policy before touching IXWebSocket.
    cfg.host = "127.0.0.1:8765";
    if (client.connect(cfg)) {
        std::cerr << "unsafe endpoint connected\n";
        return 1;
    }
    if (client.state() != aura::waapi::ConnectionState::Disconnected) {
        std::cerr << "unsafe endpoint changed transport-owned state\n";
        return 1;
    }
    if (client.lastError() != "WAAPI endpoint policy rejected") {
        std::cerr << "unsafe endpoint was not rejected by policy\n";
        return 1;
    }
    return 0;
}
'''


def _run_policy_harness(tmp_path: Path) -> subprocess.CompletedProcess[str]:
    fixture = tmp_path / "waapi-policy-fixture"
    fixture.mkdir()
    (fixture / "policy_harness.cpp").write_text(HARNESS, encoding="utf-8")
    (fixture / "CMakeLists.txt").write_text(
        "\n".join(
            [
                "cmake_minimum_required(VERSION 3.20)",
                "project(waapi_policy_harness LANGUAGES CXX)",
                "add_executable(waapi_policy_harness policy_harness.cpp)",
                "target_compile_features(waapi_policy_harness PRIVATE cxx_std_17)",
                f'target_include_directories(waapi_policy_harness PRIVATE "{(ROOT / "src").as_posix()}")',
            ]
        )
        + "\n",
        encoding="utf-8",
    )
    build = fixture / "build"
    configured = subprocess.run(
        ["cmake", "-S", str(fixture), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"],
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        check=False,
    )
    assert configured.returncode == 0, configured.stdout + configured.stderr
    built = subprocess.run(
        ["cmake", "--build", str(build), "--config", "Release", "--target", "waapi_policy_harness"],
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        check=False,
    )
    assert built.returncode == 0, built.stdout + built.stderr
    candidates = list(build.rglob("waapi_policy_harness.exe")) + list(build.rglob("waapi_policy_harness"))
    assert candidates, "CMake did not produce the policy harness"
    return subprocess.run(
        candidates[0], text=True, encoding="utf-8", errors="replace",
        capture_output=True, check=False, timeout=5,
    )


def _run_connect_harness(tmp_path: Path) -> subprocess.CompletedProcess[str]:
    fixture = tmp_path / "waapi-connect-fixture"
    fixture.mkdir()
    (fixture / "connect_harness.cpp").write_text(CONNECT_HARNESS, encoding="utf-8")
    (fixture / "CMakeLists.txt").write_text(
        "\n".join(
            [
                "cmake_minimum_required(VERSION 3.20)",
                "project(waapi_connect_harness LANGUAGES CXX)",
                "set(USE_TLS OFF CACHE BOOL \"\" FORCE)",
                "set(USE_MBED_TLS OFF CACHE BOOL \"\" FORCE)",
                "set(USE_OPEN_SSL OFF CACHE BOOL \"\" FORCE)",
                "set(USE_ZLIB OFF CACHE BOOL \"\" FORCE)",
                "set(BUILD_DEMO OFF CACHE BOOL \"\" FORCE)",
                "set(BUILD_SHARED_LIBS OFF CACHE BOOL \"\" FORCE)",
                f'add_subdirectory("{(ROOT / "thirdparty" / "ixwebsocket").as_posix()}" ixwebsocket-build EXCLUDE_FROM_ALL)',
                "add_executable(waapi_connect_harness",
                "  connect_harness.cpp",
                f'  "{(ROOT / "src" / "WaapiClient.cpp").as_posix()}"',
                f'  "{(ROOT / "src" / "WaapiResponseParse.cpp").as_posix()}"',
                f'  "{(ROOT / "src" / "platform" / "filesystem_std.cpp").as_posix()}"',
                f'  "{(ROOT / "src" / "platform" / "filesystem_win32.cpp").as_posix()}"',
                f'  "{(ROOT / "src" / "platform" / "text_encoding_win32.cpp").as_posix()}"',
                ")",
                "target_compile_features(waapi_connect_harness PRIVATE cxx_std_17)",
                f'target_include_directories(waapi_connect_harness PRIVATE "{(ROOT / "src").as_posix()}" "{(ROOT / "thirdparty").as_posix()}")',
                "target_link_libraries(waapi_connect_harness PRIVATE ixwebsocket)",
                "if(WIN32)",
                "  target_link_libraries(waapi_connect_harness PRIVATE ws2_32)",
                "endif()",
            ]
        )
        + "\n",
        encoding="utf-8",
    )
    build = fixture / "build"
    configured = subprocess.run(
        ["cmake", "-S", str(fixture), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"],
        text=True, encoding="utf-8", errors="replace", capture_output=True, check=False,
    )
    assert configured.returncode == 0, configured.stdout + configured.stderr
    built = subprocess.run(
        ["cmake", "--build", str(build), "--config", "Release", "--target", "waapi_connect_harness"],
        text=True, encoding="utf-8", errors="replace", capture_output=True, check=False,
    )
    assert built.returncode == 0, built.stdout + built.stderr
    candidates = list(build.rglob("waapi_connect_harness.exe")) + list(build.rglob("waapi_connect_harness"))
    assert candidates, "CMake did not produce the connect harness"
    return subprocess.run(
        candidates[0], text=True, encoding="utf-8", errors="replace",
        capture_output=True, check=False, timeout=5,
    )


def test_plaintext_loopback_policy_accepts_and_rejects_real_endpoint_table(tmp_path: Path):
    result = _run_policy_harness(tmp_path)
    assert result.returncode == 0, result.stderr


def test_connect_executes_the_policy_before_transport_side_effects(tmp_path: Path):
    result = _run_connect_harness(tmp_path)
    assert result.returncode == 0, result.stderr


def test_connect_rejects_before_network_or_trace_side_effects():
    source = SOURCE.read_text(encoding="utf-8")
    connect = source.split("bool WaapiClient::connect", 1)[1]
    policy = connect.index("validatePlaintextLoopbackEndpoint")
    # The mandatory guard itself must be executable, not a disabled source
    # fragment retained only to satisfy textual checks.
    assert "#if" not in connect[:policy]
    for side_effect in (
        "ensureNetSystemInit",
        'impl_->trace("CONNECT"',
        "impl_->ws.setUrl",
        "impl_->ws.start",
    ):
        assert policy < connect.index(side_effect)
    rejected = connect[policy:connect.index("ensureNetSystemInit")]
    assert '"WAAPI endpoint policy rejected"' in rejected
    assert "impl_->state.store" not in rejected
    assert "return false;" in rejected
    assert "cfg.host" not in rejected
    assert "cfg.path" not in rejected
    assert 'path = "/waapi"' in HEADER.read_text(encoding="utf-8")


def test_connect_harness_links_windows_socket_only_on_windows():
    source = Path(__file__).read_text(encoding="utf-8")
    link = source.index('"target_link_libraries(waapi_connect_harness PRIVATE ws2_32)"')
    guard = source.rfind('"if(WIN32)"', 0, link)
    end = source.index('"endif()"', link)
    assert guard != -1
    assert guard < link < end
