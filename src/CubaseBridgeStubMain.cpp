#ifdef _WIN32

#include "platform/named_pipe.h"
#include "platform/system.h"
#include "platform/text_encoding.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::map<std::string, std::string> ParseArgs(int argc, wchar_t* argv[]) {
    std::map<std::string, std::string> args;
    for (int i = 1; i < argc; ++i) {
        const std::string key = Platform::TextEncoding::WideToUtf8Permissive(argv[i]);
        if (key.rfind("--", 0) != 0) continue;
        std::string value;
        if (i + 1 < argc) {
            const std::string next = Platform::TextEncoding::WideToUtf8Permissive(argv[i + 1]);
            if (next.rfind("--", 0) != 0) {
                value = next;
                ++i;
            }
        }
        args[key] = value;
    }
    return args;
}

std::string TrimLine(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

std::vector<std::string> Split(const std::string& value, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, delim)) {
        parts.push_back(item);
    }
    return parts;
}

std::map<std::string, std::string> ParseKeyValueTokens(const std::vector<std::string>& tokens, size_t startIndex) {
    std::map<std::string, std::string> fields;
    for (size_t i = startIndex; i < tokens.size(); ++i) {
        const auto pos = tokens[i].find('=');
        if (pos == std::string::npos || pos == 0 || pos + 1 >= tokens[i].size()) continue;
        fields[tokens[i].substr(0, pos)] = tokens[i].substr(pos + 1);
    }
    return fields;
}

std::vector<std::string> CollectInsertFiles(const std::vector<std::string>& tokens) {
    std::vector<std::string> files;
    for (size_t i = 2; i < tokens.size(); ++i) {
        if (tokens[i].find('=') == std::string::npos && !tokens[i].empty()) {
            files.push_back(tokens[i]);
        }
    }
    return files;
}

int CountInsertFiles(const std::vector<std::string>& tokens) {
    return static_cast<int>(CollectInsertFiles(tokens).size());
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    const auto args = ParseArgs(argc, argv);
    const std::string pipeName = args.count("--pipe") ? args.at("--pipe") : R"(\\.\pipe\auro_cubase_direct_insert_v1)";
    const std::string handshakeToken = args.count("--token") ? args.at("--token") : std::string();
    const bool stayAlive = args.count("--stay-alive") != 0;
    const std::string preferredTrack = "Selected Track";
    const std::string sessionId = "auro-helper-session-" +
        std::to_string(Platform::CurrentProcessId());
    const std::string snapshotToken = "snap-demo-001";
    const std::string projectName = "Harken Search Bridge Demo Project";
    const std::string selectedTrackName = "Selected Track";
    const std::string cursorLabel = "Bar 17 Beat 1";

    if (pipeName.empty()) {
        return 2;
    }

    const std::wstring widePipe(pipeName.begin(), pipeName.end());
    bool shouldExit = false;
    while (!shouldExit) {
        Platform::NamedPipe::ServerSession pipe;
        if (pipe.Open(widePipe, 4096, 4096, 1).status !=
            Platform::NamedPipe::ServerSessionStatus::Ok) {
            return 3;
        }

        if (pipe.Accept().status != Platform::NamedPipe::ServerSessionStatus::Ok) {
            pipe.DisconnectAndClose();
            continue;
        }

        const auto read = pipe.Read(4095);
        if (read.status == Platform::NamedPipe::ServerSessionStatus::Ok) {
            const std::string message = TrimLine(read.message);
            const auto tokens = Split(message, '|');

            std::string response = "ERR|BRIDGE_UNAVAILABLE|Unknown command\n";
            if (!tokens.empty() && tokens[0] == "HELLO") {
                const std::string helloPayload = message.size() > 6 ? message.substr(6) : std::string();
                if (!helloPayload.empty() && !handshakeToken.empty() &&
                    helloPayload != handshakeToken && helloPayload != "shutdown") {
                    response = "ERR|SESSION_MISMATCH|handshake-token-mismatch\n";
                } else {
                    response = "OK|HELLO|session=" + sessionId +
                               "|snapshotToken=" + snapshotToken +
                               "|projectName=" + projectName +
                               "|selectedTrack=" + selectedTrackName +
                               "|cursorLabel=" + cursorLabel +
                               "|projectState=ready|trackState=selected-ready|supportsSelectedTrack=1|supportsCursorTrack=1|supportsAppendToTrack=1|supportsSpreadAcrossSelection=1\n";
                    if (helloPayload == "shutdown") {
                        shouldExit = true;
                    }
                }
            } else if (!tokens.empty() && tokens[0] == "INSERT") {
                const auto fields = ParseKeyValueTokens(tokens, 2);
                const auto files = CollectInsertFiles(tokens);
                const int fileCount = CountInsertFiles(tokens);
                const int safeCount = std::max(1, fileCount);
                const std::string requestedSession = fields.count("session") ? fields.at("session") : "current-session";
                const std::string requestedTrack = fields.count("preferredTrack") ? fields.at("preferredTrack") : preferredTrack;
                const std::string requestedSnapshot = fields.count("snapshot") ? fields.at("snapshot") : std::string();
                const std::string requestedProject = fields.count("projectName") ? fields.at("projectName") : std::string();
                const std::string requestedSelectedTrack = fields.count("selectedTrack") ? fields.at("selectedTrack") : std::string();
                const std::string requestedCursor = fields.count("cursorLabel") ? fields.at("cursorLabel") : std::string();
                const std::string spotMode = fields.count("spot") ? fields.at("spot") : "edit-cursor";
                const std::string insertPolicy = fields.count("policy") ? fields.at("policy") : "single-track";

                if (requestedSession == "missing-session" || requestedSession == "project-session-changed") {
                    response = "ERR|SESSION_MISMATCH|project-session-changed\n";
                } else if (!requestedSnapshot.empty() && requestedSnapshot != snapshotToken) {
                    response = "ERR|BRIDGE_NOT_READY|snapshot-mismatch\n";
                } else if (!requestedProject.empty() && requestedProject != projectName) {
                    response = "ERR|BRIDGE_NOT_READY|project-mismatch\n";
                } else if (!requestedSelectedTrack.empty() && requestedSelectedTrack != selectedTrackName) {
                    response = "ERR|TRACK_UNAVAILABLE|selected-track-changed\n";
                } else if (!requestedCursor.empty() && requestedCursor != cursorLabel) {
                    response = "ERR|BRIDGE_NOT_READY|cursor-mismatch\n";
                } else if (requestedTrack == "Missing Track") {
                    response = "ERR|TRACK_UNAVAILABLE|selected-track-not-found\n";
                } else if (insertPolicy == "spread-selection" && !files.empty()) {
                    response = "OK|INSERT|session=" + sessionId +
                               "|snapshotToken=" + snapshotToken +
                               "|projectName=" + projectName +
                               "|selectedTrack=" + selectedTrackName +
                               "|cursorLabel=" + cursorLabel +
                               "|projectState=ready|trackState=remapped" +
                               "|track=" + requestedTrack +
                               "|remappedTrack=FX ALT|position=next-grid|adjusted=1|count=1|clips=clip-stub-001|failed=" +
                               files.back() + "\n";
                } else {
                    response = "OK|INSERT|session=" + sessionId +
                               "|snapshotToken=" + snapshotToken +
                               "|projectName=" + projectName +
                               "|selectedTrack=" + selectedTrackName +
                               "|cursorLabel=" + cursorLabel +
                               "|projectState=ready|trackState=selected-ready" +
                               "|track=" + requestedTrack +
                               "|position=" + spotMode + "|count=" + std::to_string(safeCount) +
                               "|clips=clip-stub-001\n";
                }
                shouldExit = !stayAlive;
            }

            pipe.WriteAndFlush(response);
        }

        pipe.DisconnectAndClose();
    }

    return 0;
}

#else
int main() { return 0; }
#endif
