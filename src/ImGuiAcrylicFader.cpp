#include "ImGuiAcrylicFader.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>

namespace ui::synth {

    enum class FaderSize {
        Small = 48,
        Medium = 84, // Reference Size
        Large = 140
    };

    // ---------------------------------------------------------------------------------
    // 助手函数：构建带“收腰”设计的亚克力切割路径 (SVG Bezier Equivalent)
    // 对应 SVG: M 0,16 Q 0,0 16,0 C 35,8 65,8 84,0 Q 100,0 100,16 L ...
    // ---------------------------------------------------------------------------------
    void PathAcrylicWaist(ImDrawList* draw_list, ImVec2 center, float size) {
        ImVec2 tl = ImVec2(center.x - size * 0.5f, center.y - size * 0.5f);
        float s = size;

        draw_list->PathClear();
        // 左上角出发
        draw_list->PathLineTo(ImVec2(tl.x, tl.y + s * 0.16f)); 
        // 左上到顶边(圆角)
        draw_list->PathBezierQuadraticCurveTo(ImVec2(tl.x, tl.y), ImVec2(tl.x + s * 0.16f, tl.y), 10);
        // 顶部“收腰”下沉曲线 (Cubic)
        draw_list->PathBezierCubicCurveTo(
            ImVec2(tl.x + s * 0.35f, tl.y + s * 0.08f),
            ImVec2(tl.x + s * 0.65f, tl.y + s * 0.08f),
            ImVec2(tl.x + s * 0.84f, tl.y), 15);
        // 右上圆角
        draw_list->PathBezierQuadraticCurveTo(ImVec2(tl.x + s, tl.y), ImVec2(tl.x + s, tl.y + s * 0.16f), 10);
        // 右直线
        draw_list->PathLineTo(ImVec2(tl.x + s, tl.y + s * 0.84f));
        // 右下圆角
        draw_list->PathBezierQuadraticCurveTo(ImVec2(tl.x + s, tl.y + s), ImVec2(tl.x + s * 0.84f, tl.y + s), 10);
        // 底部“收腰”上拱曲线 (Cubic)
        draw_list->PathBezierCubicCurveTo(
            ImVec2(tl.x + s * 0.65f, tl.y + s * 0.92f),
            ImVec2(tl.x + s * 0.35f, tl.y + s * 0.92f),
            ImVec2(tl.x + s * 0.16f, tl.y + s), 15);
        // 左下圆角
        draw_list->PathBezierQuadraticCurveTo(ImVec2(tl.x, tl.y + s), ImVec2(tl.x, tl.y + s * 0.84f), 10);
    }

    // ---------------------------------------------------------------------------------
    // 助手函数：绘制半透明的玻璃防滑纤维柱 (Glass Rods)
    // ---------------------------------------------------------------------------------
    void DrawGlassRod(ImDrawList* draw_list, ImVec2 center, float width, float height) {
        ImVec2 tl = ImVec2(center.x - width * 0.5f, center.y - height * 0.5f);
        ImVec2 br = ImVec2(center.x + width * 0.5f, center.y + height * 0.5f);
        float rounding = width * 0.5f;

        // 内部白透纤维底色
        draw_list->AddRectFilled(tl, br, IM_COL32(255, 255, 255, 25), rounding);

        // 左上黑死角（内部折射遮挡）
        draw_list->AddRect(tl, br, IM_COL32(0, 0, 0, 50), rounding, 0, 1.0f);
        
        // 右下纯白高光（外部玻璃边缘聚焦）
        draw_list->AddRect(
            ImVec2(tl.x + 1.0f, tl.y + 1.0f), 
            ImVec2(br.x + 1.0f, br.y + 1.0f), 
            IM_COL32(255, 255, 255, 100), rounding, 0, 1.0f);
    }

    // ---------------------------------------------------------------------------------
    // 核心函数：艺术装置版透明亚克力推子 (Art Installation Acrylic Fader)
    // 支持按三种预设 Size 等比缩放渲染
    // ---------------------------------------------------------------------------------
    void DrawArtAcrylicFader(ImVec2 center, FaderSize size_type, bool is_hovered) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
        // 获取实际物理尺寸，并计算与基准(84px)的缩放比例
        float size = static_cast<float>(size_type);
        float scl = size / 84.0f; 

        // 1. >>> 巨型悬浮阴影 (Deep Drop Shadows) <<<
        float shadow_offset = (is_hovered ? 16.0f : 10.0f) * scl;
        
        for (int i = 0; i < 4; i++) {
            PathAcrylicWaist(draw_list, ImVec2(center.x, center.y + shadow_offset - (i * 1.5f * scl)), size + (i * 2.5f * scl));
            // 注：此处填充由于形状略带微收腰凹进，ImGui 的 PathFillConvex 对于这种星形多边形可正常运作
            draw_list->PathFillConvex(IM_COL32(0, 0, 0, 40 - i*10)); 
        }

        // 2. >>> 亚克力玻璃基底 (Acrylic Base Fill) <<<
        // ※在 ImGui 中要实现完美的模糊 Blur 需使用 Shader FBO，此处使用高配黑白渐变模拟光透感※
        ImU32 base_col_bot = IM_COL32(0, 0, 0, 60);

        PathAcrylicWaist(draw_list, center, size);
        draw_list->PathFillConvex(base_col_bot); // 垫底黑色
        
        // 软光箱顶部高光 (Soft-box Top Glare Bloom)
        float glare_h = size * 0.3f;
        ImVec2 glare_tl = ImVec2(center.x - size * 0.45f, center.y - size * 0.45f);
        ImVec2 glare_br = ImVec2(center.x + size * 0.45f, center.y - size * 0.5f + glare_h);
        draw_list->AddRectFilledMultiColor(glare_tl, glare_br, 
            IM_COL32(255, 255, 255, 180), IM_COL32(255, 255, 255, 180),
            IM_COL32(255, 255, 255, 0),   IM_COL32(255, 255, 255, 0));

        // 3. >>> 晶体坚硬倒角折射光 (Crystal Hard Bevels) <<<
        
        // --- 内部偏暗模具分界线  (Opacity 80%)
        PathAcrylicWaist(draw_list, center, size);
        draw_list->PathStroke(IM_COL32(255, 255, 255, 100), ImDrawFlags_Closed, 2.0f * scl);

        // --- 外侧高强度纯白混色 (Mix-blend Overlay)
        PathAcrylicWaist(draw_list, ImVec2(center.x, center.y + 0.5f * scl), size); // 稍微沉降表现边缘受光面
        draw_list->PathStroke(IM_COL32(255, 255, 255, 255), ImDrawFlags_Closed, 1.5f * scl);

        // 4. >>> 内部精致防滑玻璃管装置 (Subtle Glass Rods Grips) <<<
        // 缩放基准配置:
        float gap_x = 6.0f * scl;
        float rod_w = 3.0f * scl;
        float rod_h = 40.0f * scl;
        float center_w = 4.0f * scl;
        float center_h = 52.0f * scl;

        // X轴刻度落位
        float xs[] = {
            center.x - gap_x * 3.0f,
            center.x - gap_x * 1.5f,
            center.x, // 核心道
            center.x + gap_x * 1.5f,
            center.x + gap_x * 3.0f
        };

        for (int i = 0; i < 5; ++i) {
            if (i == 2) {
                // 中轴线 - 黑色透明滑轨背景
                ImVec2 t_tl = ImVec2(xs[i] - center_w*0.5f, center.y - center_h*0.5f);
                ImVec2 t_br = ImVec2(xs[i] + center_w*0.5f, center.y + center_h*0.5f);
                draw_list->AddRectFilled(t_tl, t_br, IM_COL32(0, 0, 0, 40), center_w*0.5f);
                draw_list->AddRect(t_tl, t_br, IM_COL32(0, 0, 0, 100), center_w*0.5f, 0, 1.0f);
                
                // Cyan 发光微粒 (Art scale marker)
                float tick_w = 2.0f * scl;
                float tick_h = 16.0f * scl;
                draw_list->AddRectFilled(
                    ImVec2(center.x - tick_w*0.5f, center.y - tick_h*0.5f),
                    ImVec2(center.x + tick_w*0.5f, center.y + tick_h*0.5f),
                    IM_COL32(6, 182, 212, 255), // Cyan-400
                    tick_w*0.5f);
                
                // 微粒光晕
                draw_list->AddCircleFilled(center, 4.0f * scl, IM_COL32(6, 182, 212, 100)); 
            } else {
                // 普通玻璃圆管
                DrawGlassRod(draw_list, ImVec2(xs[i], center.y), rod_w, rod_h);
            }
        }
    }

    // ---------------------------------------------------------------------------------
    // 主题化版本：LED 色通过参数传入，支持任意 float 尺寸，浅色主题降低基底不透明度
    // ---------------------------------------------------------------------------------
    void DrawArtAcrylicFaderThemed(ImVec2 center, float size,
                                    bool is_hovered, ImVec4 ledColor,
                                    bool isLightTheme) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        float scl = size / 84.0f;

        // 1. Soft drop shadow. Keep this small so the control reads as
        // translucent white acrylic, not a heavy hardware cap.
        float shadow_offset = (is_hovered ? 7.0f : 5.0f) * scl;
        for (int i = 0; i < 2; i++) {
            PathAcrylicWaist(draw_list, ImVec2(center.x, center.y + shadow_offset - (i * 1.5f * scl)), size + (i * 2.5f * scl));
            draw_list->PathFillConvex(IM_COL32(0, 0, 0, 28 - i * 10));
        }

        // 2. White translucent acrylic base fill.
        PathAcrylicWaist(draw_list, center, size);
        draw_list->PathFillConvex(isLightTheme ? IM_COL32(255, 255, 255, 138)
                                               : IM_COL32(255, 255, 255, 118));

        float glare_h  = size * 0.3f;
        ImVec2 glare_tl = ImVec2(center.x - size * 0.45f, center.y - size * 0.45f);
        ImVec2 glare_br = ImVec2(center.x + size * 0.45f, center.y - size * 0.5f + glare_h);
        draw_list->AddRectFilledMultiColor(glare_tl, glare_br,
            IM_COL32(255, 255, 255, 210), IM_COL32(255, 255, 255, 210),
            IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));

        // 3. Crystal bevels
        PathAcrylicWaist(draw_list, center, size);
        draw_list->PathStroke(IM_COL32(255, 255, 255, 138), ImDrawFlags_Closed, 1.4f * scl);

        PathAcrylicWaist(draw_list, ImVec2(center.x, center.y + 0.5f * scl), size);
        draw_list->PathStroke(IM_COL32(255, 255, 255, 245), ImDrawFlags_Closed, 1.0f * scl);

        // 4. Glass rod grips
        float gap_x    = 6.0f * scl;
        float rod_w    = 3.0f * scl;
        float rod_h    = 40.0f * scl;
        float center_w = 4.0f * scl;
        float center_h = 52.0f * scl;

        float xs[] = {
            center.x - gap_x * 3.0f,
            center.x - gap_x * 1.5f,
            center.x,
            center.x + gap_x * 1.5f,
            center.x + gap_x * 3.0f
        };

        for (int i = 0; i < 5; ++i) {
            if (i == 2) {
                ImVec2 t_tl = ImVec2(xs[i] - center_w * 0.5f, center.y - center_h * 0.5f);
                ImVec2 t_br = ImVec2(xs[i] + center_w * 0.5f, center.y + center_h * 0.5f);
                draw_list->AddRectFilled(t_tl, t_br, IM_COL32(255, 255, 255, 34), center_w * 0.5f);
                draw_list->AddRect(t_tl, t_br, IM_COL32(255, 255, 255, 72), center_w * 0.5f, 0, 0.8f);
            } else {
                DrawGlassRod(draw_list, ImVec2(xs[i], center.y), rod_w, rod_h);
            }
        }
    }

    void DrawDotPattern(ImDrawList* draw_list, ImVec2 p0, ImVec2 p1,
                        ImU32 col, float spacing) {
        for (float y = p0.y + spacing * 0.5f; y < p1.y; y += spacing) {
            for (float x = p0.x + spacing * 0.5f; x < p1.x; x += spacing) {
                draw_list->AddRectFilled(
                    ImVec2(x - 1.0f, y - 1.0f),
                    ImVec2(x + 1.0f, y + 1.0f),
                    col);
            }
        }
    }

    // ---------------------------------------------------------------------------------
    // 交互推子：横向轨道 + 亚克力帽 + 鼠标输入处理
    // ---------------------------------------------------------------------------------
    bool AcrySliderFloat(const char* id, float* value, float min, float max,
                         float trackWidth, float trackHeight, float capSize,
                         ImVec4 ledColor, bool isLightTheme) {
        ImGui::PushID(id);

        ImVec2 trackPos = ImGui::GetCursorScreenPos();
        bool changed = false;

        ImGui::InvisibleButton("##acry", ImVec2(trackWidth, trackHeight));
        bool hovered = ImGui::IsItemHovered();
        bool active  = ImGui::IsItemActive();

        if (active) {
            float mx    = ImGui::GetIO().MousePos.x;
            float left  = trackPos.x + capSize * 0.5f + 12.0f;
            float right = trackPos.x + trackWidth - capSize * 0.5f - 12.0f;
            float t     = (right > left) ? (mx - left) / (right - left) : 0.0f;
            t = std::clamp(t, 0.0f, 1.0f);
            float newVal = min + t * (max - min);
            if (newVal != *value) {
                *value  = newVal;
                changed = true;
            }
        }

        if (hovered && ImGui::GetIO().MouseWheel != 0.0f) {
            float step = (max - min) * 0.01f * ImGui::GetIO().MouseWheel;
            *value = std::clamp(*value + step, min, max);
            changed = true;
        }

        // Draw the Smoke UI v5 host card, scale ticks, deep slot, and
        // acrylic cap. The caller controls height so compact production
        // placements can use the same design without depending on dev tools.
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float travelL = trackPos.x + capSize * 0.5f + 12.0f;
        const float travelR = trackPos.x + trackWidth - capSize * 0.5f - 12.0f;
        const float travel  = std::max(1.0f, travelR - travelL);
        float t = (max > min) ? (*value - min) / (max - min) : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);

        const float trackCY = trackPos.y + trackHeight * 0.50f;
        const float capCX   = travelL + t * travel;
        const ImVec2 cardMax(trackPos.x + trackWidth, trackPos.y + trackHeight);

        const ImU32 cardBG = isLightTheme ? IM_COL32(222, 230, 242, 150)
                                          : IM_COL32(12, 23, 42, 168);
        const ImU32 cardBorder = isLightTheme ? IM_COL32(88, 116, 154, 125)
                                              : IM_COL32(0, 0, 0, 180);
        const ImU32 dotCol = isLightTheme ? IM_COL32(85, 120, 170, 55)
                                          : IM_COL32(70, 118, 172, 75);
        dl->AddRectFilled(trackPos, cardMax, cardBG, 6.0f);
        DrawDotPattern(dl,
                       ImVec2(trackPos.x + 8.0f, trackPos.y + 5.0f),
                       ImVec2(cardMax.x - 8.0f, cardMax.y - 5.0f),
                       dotCol, 9.0f);
        dl->AddRect(trackPos, cardMax, cardBorder, 6.0f, 0, 1.0f);

        DrawArtAcrylicFaderThemed(ImVec2(capCX, trackCY), capSize,
                                  hovered || active, ledColor, isLightTheme);

        ImGui::PopID();
        return changed;
    }

} // namespace ui::synth
