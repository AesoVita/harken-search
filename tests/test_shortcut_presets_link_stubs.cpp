#include "../src/AppState.h"

Database::Database() = default;
Database::~Database() = default;

void AppState::performGlobalMemoryGC()
{
}

namespace CoverModule {

bool IsCoverTextureHandleCurrent(const CoverItem&, uint32_t)
{
    return true;
}

} // namespace CoverModule
