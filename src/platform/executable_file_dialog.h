#pragma once

#include <string>

namespace Platform::ExecutableDialogs {

// @stage7-contract
// API contract: Runs synchronously on the UI thread, borrows the optional
// native owner, title, and initial path only for the modal call, and returns an
// owned UTF-8 path. Empty remains the existing cancel/failure result; no input
// is retained after return.
std::string OpenExecutableFileDialogUtf8(
    void* ownerWindow,
    const std::wstring& title,
    const std::wstring& initialPath);

} // namespace Platform::ExecutableDialogs
