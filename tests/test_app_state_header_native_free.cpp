#include "src/AppState.h"

#include <type_traits>

static_assert(std::is_same_v<GLuint, unsigned int>);
static_assert(std::is_same_v<decltype(AppState::spectrogramTexture), GLuint>);
static_assert(std::is_same_v<Aura::AppState, ::AppState>);
static_assert(std::is_same_v<decltype(Aura::g_appState), AppState*>);

#ifdef SetWindowPos
#error AppState.h must neutralize SetWindowPos
#endif

#ifdef GetWindowPos
#error AppState.h must neutralize GetWindowPos
#endif

int main() {
    return 0;
}
