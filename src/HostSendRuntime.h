#pragma once

#include "HostSendModel.h"

#include <string>
#include <vector>

namespace HarkenSearch::HostSend {

// @stage7-contract
// API contract: HostSend runtime helpers synchronously build owned requests
// and dispatch-path vectors from caller-owned host/job/file inputs. They do
// not retain source references; MaterializeStagingPlan reports filesystem
// failures through bool/error and preserves the existing staging ownership
// contract.

Request BuildRequestFromFiles(HostId host,
                              const std::vector<std::string>& sourcePathsUtf8,
                              DeliveryMode mode,
                              SourceSelectionKind selectionKind = SourceSelectionKind::WholeFile,
                              bool copyToProject = false,
                              bool keepHarkenSearchProcessingSuffix = true);

bool MaterializeStagingPlan(const Job& job, std::string& outError);

std::vector<std::string> CollectDispatchPaths(const Job& job);

} // namespace HarkenSearch::HostSend
