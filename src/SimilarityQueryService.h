#pragma once

#include "SimilarityWorkflow.h"

#include <memory>
#include <string>

class Database;

namespace HarkenSearch {

// @stage7-contract
// API contract: SimilarityQueryService owns its database connection and borrows
// the supplied SimilaritySearch cache, which must outlive the service and any
// handler returned by MakeSimilarityQueryHandler. Execute is called serially by
// the SimilarityWorkflow worker; the service is not a concurrent entry point.
// Invalid database/reference/request data is reported in SimilarityCompletion
// with empty rows and the established error enum. The returned handler owns the
// service instance, observes cancellation, and retains no request references.
class SimilarityQueryService final {
public:
    explicit SimilarityQueryService(SimilaritySearch& cache);
    ~SimilarityQueryService();

    SimilarityCompletion Execute(
        const SimilarityRequest& request,
        const SimilarityWorkflow::CancelCheck& cancelled);

private:
    bool EnsureDatabase(const std::string& path);

    SimilaritySearch& cache_;
    std::unique_ptr<Database> database_;
    std::string databasePath_;
    int cachedFeatureCount_ = -1;
};

SimilarityWorkflow::Handler MakeSimilarityQueryHandler(SimilaritySearch& cache);

} // namespace HarkenSearch
