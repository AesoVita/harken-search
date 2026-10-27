#pragma once

#include "platform/database_audio_probe.h"

#include <string>

namespace Platform::DatabaseAudioProbe::Detail {

// @stage7-contract
// API contract: backend helpers are synchronous, implementation-facing
// dispatch functions. They borrow UTF-8/path inputs and caller-owned Info and
// error outputs for the duration of the call, retain no references, and
// report unsupported/invalid media through bool plus the existing error text.

bool CanDecodeExtension(const std::string& pathUtf8);
bool IsFlacExtension(const std::string& pathUtf8);
bool ProbeMediaFoundation(const std::string& pathUtf8, Info& out, std::string& error);
bool ProbeMiniaudio(const std::filesystem::path& path, Info& out);

} // namespace Platform::DatabaseAudioProbe::Detail
