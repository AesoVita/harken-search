#include "platform/search_text_gb2312.h"

namespace Platform::SearchText {

char Gb2312InitialForCodepoint(uint32_t codepoint) {
    (void)codepoint;
    return '\0';
}

} // namespace Platform::SearchText
