#pragma once
#include "TransportContrast.h"
#include "TransportVisualAudit.h"
#include <cmath>

namespace TransportValueText {
// Text only: no rectangle, badge, outline or shadow. Clip the same glyphs
// into pixel columns so a moving fill boundary can use a different ink on
// each underlying surface without putting a plate over the slider.
template<class SurfaceAt>
inline void Draw(ImDrawList* draw, ImVec2 position, const char* text,
                 ImVec4 preferred, SurfaceAt surfaceAt, const char* auditId) {
    const auto size=ImGui::CalcTextSize(text);
    for (float x=std::floor(position.x); x<position.x+size.x; x+=1.f) {
        const auto bg=TransportContrast::Quantize(surfaceAt(x+.5f));
        const auto ink=ImGui::GetColorU32(TransportContrast::ForSurface(preferred,bg,4.5));
        const ImVec2 a(x,position.y), b(std::min(x+1.f,position.x+size.x),position.y+size.y);
        draw->PushClipRect(a,b,true);
        draw->AddText(position,ink,text);
        draw->PopClipRect();
        TransportVisualAudit::Record(auditId,true,ink,ImGui::GetColorU32(bg),a,b);
    }
}
} // namespace TransportValueText
