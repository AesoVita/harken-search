#include "ui/TransportBlendSlider.h"
#include "ui/TransportVisualAudit.h"
#include <cmath>
#include <cstdio>

namespace {
constexpr ImU32 backing = IM_COL32(239, 1, 254, 255);
constexpr ImU32 ink = IM_COL32(1, 254, 23, 255);
float percent = 42;
ImVec2 center;
bool editing = false, overlay = false;
int failures = 0;
void Check(bool good, const char* label) {
    std::printf("%s %s\n", good ? "PASS" : "FAIL", label);
    failures += !good;
}
void Frame() {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20));
    ImGui::SetNextWindowSize(ImVec2(400, 120));
    ImGui::Begin("Transport input", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextItemWidth(240);
    const int start = ImGui::GetWindowDrawList()->VtxBuffer.Size;
    TransportBlendSlider::Draw("##blend", &percent, ink, backing);
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    center = ImVec2((a.x + b.x) / 2, (a.y + b.y) / 2);
    editing = ImGui::TempInputIsActive(ImGui::GetItemID());
    overlay = false;
    const auto& vertices = ImGui::GetWindowDrawList()->VtxBuffer;
    for (int i = start; i < vertices.Size; ++i)
        overlay |= vertices[i].col == backing; // Native editor text may use the readable ink.
    ImGui::End();
    ImGui::Render();
}
}
int main() {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800, 600);
    io.DeltaTime = 1.f / 60;
    unsigned char* pixels; int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    Frame(); Frame();
    Check(!editing && overlay, "ordinary value keeps its readable backing");
    io.AddMousePosEvent(center.x, center.y); Frame();
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true); Frame();
    Check(editing, "Ctrl+click enters the real ImGui number editor");
    Check(!overlay, "number editor is not painted over by the display label");
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false); Frame();
    io.AddInputCharactersUTF8("75"); Frame();
    Check(!overlay, "typed digits stay unobscured");
    io.AddKeyEvent(ImGuiKey_Enter, true); Frame();
    io.AddKeyEvent(ImGuiKey_Enter, false); Frame(); Frame();
    Check(std::fabs(percent - 75.f) < .001f, "typing 75 and Enter commits 75 percent");
    Check(!editing && overlay, "readable display returns after editing");
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20));
    ImGui::SetNextWindowSize(ImVec2(400, 120));
    ImGui::Begin("Glyph audit", nullptr, ImGuiWindowFlags_NoSavedSettings);
    TransportVisualAudit::Request();
    TransportVisualAudit::Begin(0, ImVec2(20, 20), ImVec2(420, 140), IM_COL32_BLACK);
    ImGui::GetWindowDrawList()->PushClipRect(ImVec2(60, 50), ImVec2(100, 80), true);
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), 20, ImVec2(40, 55), ink, "OVERFLOW");
    TransportVisualAudit::Text("test", ink, IM_COL32_BLACK, ImVec2(40, 55), "OVERFLOW", ImGui::GetFont(), 20);
    ImGui::GetWindowDrawList()->PopClipRect();
    const auto& measured = TransportVisualAudit::checks.at(0);
    Check(measured.glyphBounds && measured.min.x < 60 && measured.max.x > 100,
        "audit preserves uncut glyph bounds from the actual font");
    Check(measured.clipMin.x == 60 && measured.clipMax.x == 100,
        "audit preserves the active inner clip after it is popped");
    ImGui::End(); ImGui::Render();
    ImGui::DestroyContext();
    return failures ? 1 : 0;
}
