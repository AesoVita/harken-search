#pragma once

#include <string>

namespace Platform::ThemeDialogs {

// @stage7-contract
// API contract: the dialog helpers are UI-thread-only modal operations. They
// borrow the owner handle and suggested UTF-8 path for the call and return an
// owned selected path; empty means cancel/failure under the existing API.
// No native handle or caller memory is retained after return.

std::string OpenThemeJsonFileDialogUtf8(void* ownerWindow, const std::string& suggestedPathUtf8);
std::string SaveThemeJsonFileDialogUtf8(void* ownerWindow, const std::string& suggestedPathUtf8);

} // namespace Platform::ThemeDialogs
