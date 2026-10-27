#include "src/FileTypeVisibilityPolicy.h"

#include <cassert>

int main() {
    using namespace aura::file_types;
    FileTypeVisibility visibility;
    visibility.showWav = false;
    visibility.showOther = false;
    const auto policy = BuildPolicy(visibility);
    assert(!policy.allVisible);
    assert(policy.includeOnlyKnownExtensions);
    assert(policy.excludedExtensions.size() == 2);
    assert(!isVisible(visibility, ".wav"));
    assert(!isVisible(visibility, "WAVE"));
    assert(isVisible(visibility, ".mp3"));
    assert(!isVisible(visibility, ".unknown"));
    return 0;
}
