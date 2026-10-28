#include "FileInfoPanel.h"
#include "imgui_internal.h"
#include <iostream>
int main() {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(640,480); io.IniFilename = nullptr;
    unsigned char* pixels; int width,height;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(400,300));
    ImGui::Begin("Info footer regression");
    const ImVec2 strip = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(200,18));
    FinishFileInfoArtworkFooter(strip,26);
    const auto* window = ImGui::GetCurrentWindowRead();
    const bool reserved = window->DC.CursorMaxPos.y >= strip.y+26;
    // Keep the failed test from opening a platform assertion dialog.
    ImGui::Dummy(ImVec2(0,0));
    ImGui::End(); ImGui::EndFrame(); ImGui::DestroyContext();
    std::cout << (reserved ? "PASS" : "FAIL") << ": footer space belongs to the parent layout\n";
    return reserved ? 0 : 1;
}
