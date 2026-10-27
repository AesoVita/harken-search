#pragma once

#include <string>

namespace Platform::DatabaseDialogs {

// @stage7-contract
// API contract: Both operations run on the UI thread, borrow the optional
// native owner window only for the modal call, and return an owned UTF-8 path.
// The initial-directory variant also borrows that directory only until the
// modal call returns. An empty string is the existing cancel/failure result;
// no window handle or caller memory is retained after return.

std::string OpenDatabaseFileDialogUtf8(void* ownerWindow);
std::string OpenDatabaseFileDialogWithInitialDirectoryUtf8(
    void* ownerWindow,
    const std::wstring& initialDirectory);

} // namespace Platform::DatabaseDialogs
