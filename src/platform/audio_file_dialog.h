#pragma once

#include <string>

namespace Platform::AudioDialogs {

// @stage7-contract
// API contract: borrows ownerWindow for the synchronous modal call and returns
// an owned UTF-8 path. Empty means cancel or an unsupported platform.
std::string OpenAudioFileDialogUtf8(void* ownerWindow);

} // namespace Platform::AudioDialogs
