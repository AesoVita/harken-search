#include "bootstrap/HarkenSearchBootstrapExitExplanation.h"

#include <cstdio>

// Independent process fixtures must never write the installed product's
// diagnostic directory. The real persistence adapter has its own isolated tests.
namespace aris::bootstrap {
void RecordBootstrapStageFailure(const char* stageName, int exitCode,
                                 const char* extra) {
    if (exitCode == 0) return;
    std::fprintf(stderr, "stage=%s code=%d detail=%s\n",
                 stageName ? stageName : "", exitCode, extra ? extra : "");
}
}  // namespace aris::bootstrap
