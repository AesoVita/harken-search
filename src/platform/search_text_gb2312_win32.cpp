#include "platform/search_text_gb2312.h"

#include <windows.h>

namespace Platform::SearchText {

char Gb2312InitialForCodepoint(uint32_t codepoint) {
    if (codepoint > 0xFFFF) return '\0';

    wchar_t wc = static_cast<wchar_t>(codepoint);
    char gbk[3] = {0};
    BOOL defaultCharUsed = FALSE;
    const int len = WideCharToMultiByte(936, 0, &wc, 1, gbk, 2, nullptr, &defaultCharUsed);
    if (len != 2 || defaultCharUsed) return '\0';

    const unsigned char b1 = static_cast<unsigned char>(gbk[0]);
    const unsigned char b2 = static_cast<unsigned char>(gbk[1]);
    if (b1 < 0xB0 || b1 > 0xD7) return '\0';

    const int code = (b1 << 8) | b2;
    if (code >= 45217 && code <= 45252) return 'a';
    if (code >= 45253 && code <= 45760) return 'b';
    if (code >= 45761 && code <= 46317) return 'c';
    if (code >= 46318 && code <= 46825) return 'd';
    if (code >= 46826 && code <= 47009) return 'e';
    if (code >= 47010 && code <= 47296) return 'f';
    if (code >= 47297 && code <= 47613) return 'g';
    if (code >= 47614 && code <= 48118) return 'h';
    if (code >= 48119 && code <= 49061) return 'j';
    if (code >= 49062 && code <= 49323) return 'k';
    if (code >= 49324 && code <= 49895) return 'l';
    if (code >= 49896 && code <= 50370) return 'm';
    if (code >= 50371 && code <= 50613) return 'n';
    if (code >= 50614 && code <= 50621) return 'o';
    if (code >= 50622 && code <= 50905) return 'p';
    if (code >= 50906 && code <= 51386) return 'q';
    if (code >= 51387 && code <= 51445) return 'r';
    if (code >= 51446 && code <= 52217) return 's';
    if (code >= 52218 && code <= 52697) return 't';
    if (code >= 52698 && code <= 52979) return 'w';
    if (code >= 52980 && code <= 53688) return 'x';
    if (code >= 53689 && code <= 54480) return 'y';
    if (code >= 54481 && code <= 55289) return 'z';
    return '\0';
}

} // namespace Platform::SearchText
