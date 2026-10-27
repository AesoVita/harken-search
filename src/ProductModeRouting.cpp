#include "ProductModeRouting.h"

namespace aura {

bool TryGetBuiltInPresetProductMode(const ColumnViewPreset& preset, ProductMode& outMode) {
    if (!preset.builtIn) {
        return false;
    }

    outMode = (preset.name == "Music") ? ProductMode::Music : ProductMode::SFX;
    return true;
}

}
