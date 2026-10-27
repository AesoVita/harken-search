#pragma once

#include <string>

namespace Platform::ThesaurusDialogs {

// @stage7-contract
// API contract: Runs synchronously on the UI thread, borrows the optional
// native owner only for the modal call, and returns an owned UTF-8 path. Empty
// remains cancel/failure; the owner is not retained after return.
std::string OpenThesaurusJsonFileDialogUtf8(void* ownerWindow);

} // namespace Platform::ThesaurusDialogs
