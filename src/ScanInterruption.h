#pragma once

#include <cstdint>

// @stage7-contract
// API contract: scan-stop disposition helpers are pure, synchronous enum
// predicates. They borrow no state, mutate nothing, and preserve the existing
// pause-versus-rollback meaning used by scan/database callers.

namespace HarkenSearch {

enum class ScanStopDisposition : std::uint8_t {
    None = 0,
    PauseKeepCommitted,
    CancelRollback,
};

inline bool IsScanPause(ScanStopDisposition disposition) {
    return disposition == ScanStopDisposition::PauseKeepCommitted;
}

inline bool IsScanRollbackCancel(ScanStopDisposition disposition) {
    return disposition == ScanStopDisposition::CancelRollback;
}

} // namespace HarkenSearch
