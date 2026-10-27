#include "database_audio_probe_scripted_backend.h"

#include "platform/database_audio_probe_backend.h"

namespace DatabaseAudioProbeScripted {
namespace {
struct State {
    std::vector<bool> canDecode;
    std::vector<bool> isFlac;
    std::vector<Step> mediaFoundation;
    std::vector<Step> miniaudio;
    Snapshot snapshot;
};
State state;

template <typename T>
bool Push(std::vector<T>& queue, const T& value) {
    if (queue.size() >= kCapacity) return false;
    queue.push_back(value);
    return true;
}
template <typename T>
T Pop(std::vector<T>& queue) {
    if (queue.empty()) return {};
    T value = queue.front();
    queue.erase(queue.begin());
    return value;
}
void Record(Call call, const std::string& path) {
    if (state.snapshot.calls.size() >= kCapacity) return;
    state.snapshot.calls.push_back(call);
    state.snapshot.paths.push_back(path);
}
} // namespace

void Reset() { state = {}; }
bool QueueCanDecode(bool result) { return Push(state.canDecode, result); }
bool QueueIsFlac(bool result) { return Push(state.isFlac, result); }
bool QueueMediaFoundation(const Step& step) { return Push(state.mediaFoundation, step); }
bool QueueMiniaudio(const Step& step) { return Push(state.miniaudio, step); }
Snapshot Get() { return state.snapshot; }
} // namespace DatabaseAudioProbeScripted

namespace Platform::DatabaseAudioProbe::Detail {
bool CanDecodeExtension(const std::string& pathUtf8) {
    DatabaseAudioProbeScripted::Record(
        DatabaseAudioProbeScripted::Call::CanDecodeExtension, pathUtf8);
    return DatabaseAudioProbeScripted::Pop(DatabaseAudioProbeScripted::state.canDecode);
}
bool IsFlacExtension(const std::string& pathUtf8) {
    DatabaseAudioProbeScripted::Record(
        DatabaseAudioProbeScripted::Call::IsFlacExtension, pathUtf8);
    return DatabaseAudioProbeScripted::Pop(DatabaseAudioProbeScripted::state.isFlac);
}
bool ProbeMediaFoundation(const std::string& pathUtf8, Info& out, std::string& error) {
    DatabaseAudioProbeScripted::Record(
        DatabaseAudioProbeScripted::Call::MediaFoundation, pathUtf8);
    const auto step = DatabaseAudioProbeScripted::Pop(
        DatabaseAudioProbeScripted::state.mediaFoundation);
    out = step.info;
    error = step.error;
    return step.result;
}
bool ProbeMiniaudio(const std::filesystem::path& path, Info& out) {
    DatabaseAudioProbeScripted::Record(
        DatabaseAudioProbeScripted::Call::Miniaudio, path.string());
    const auto step = DatabaseAudioProbeScripted::Pop(
        DatabaseAudioProbeScripted::state.miniaudio);
    out = step.info;
    return step.result;
}
} // namespace Platform::DatabaseAudioProbe::Detail
