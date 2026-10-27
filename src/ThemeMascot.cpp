
// ThemeMascot.cpp - 主题精灵系统实现

// 每个主题拥有独特的宠物精灵和音量条特效
// Dark:     幽灵 👻    + 迷雾边框 + 灵魂过载
// Ocean:    美人鱼 🧜  + 气泡边框 + 水花过载
// Sunset:   蘑菇精 🍄  + 熔岩边框 + 岩浆过载
// RoseCedar:猫咪 🐱   + 星光边框 + 爱心过载
// Gray:     机器人 🤖  + 脉冲边框 + 电流过载

#include "ThemeMascot.h"
#include <cstdio>
#include <cmath>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4244)
#endif

// 单例实现

ThemeMascot& ThemeMascot::getInstance() {
    static ThemeMascot instance;
    return instance;
}

ThemeMascot::ThemeMascot() {
    m_mascotBoundsMin = ImVec2(0, 0);
    m_mascotBoundsMax = ImVec2(0, 0);
}

// 辅助: 绘制单个像素块

void ThemeMascot::drawPixel(ImDrawList* dl, float baseX, float baseY, float px,
                            float gx, float gy, ImU32 col) const {
    // 应用全局透明度
    int a = (int)((col >> 24) * m_currentAlpha);
    ImU32 finalCol = (col & 0x00FFFFFF) | ((ImU32)a << 24);
    dl->AddRectFilled(
        ImVec2(baseX + gx * px, baseY + gy * px),
        ImVec2(baseX + (gx + 1) * px, baseY + (gy + 1) * px),
        finalCol);
}

// 更新动画状态 (每帧调用)

void ThemeMascot::update(float deltaTime, float volume, bool isPlaying) {
    m_time += deltaTime;
    m_frameTimer += deltaTime;
    m_stateTimer += deltaTime;
    
// 性能优化: 控制渲染帧率
    m_renderAccum += deltaTime;
    if (m_renderAccum >= RENDER_INTERVAL) {
        m_renderAccum = 0.0f;
        m_needsRender = true;
        // 缓存常用三角函数值 (减少每帧重复计算)
        m_cachedSinT2 = sinf(m_time * 2.0f);
        m_cachedSinT3 = sinf(m_time * 3.0f);
        m_cachedCosT = cosf(m_time);
    }
    
    // 帧动画 (~12 FPS)
    if (m_frameTimer > 0.08f) {
        m_frameTimer = 0.0f;
        m_frame = (m_frame + 1) % SPRITE_FRAMES;
    }
    
    // 蘑菇奔跑
    if (isPlaying || m_animState == MascotAnimState::Excited) {
        m_mushroomRunPos += deltaTime * 35.0f;
    }
    
    // 更新动画状态
    updateAnimState(volume, isPlaying);
    
    // 平滑透明度过渡
    if (m_currentAlpha < m_targetAlpha) {
        m_currentAlpha = std::min(m_currentAlpha + ALPHA_FADE_SPEED * deltaTime, m_targetAlpha);
    } else if (m_currentAlpha > m_targetAlpha) {
        m_currentAlpha = std::max(m_currentAlpha - ALPHA_FADE_SPEED * deltaTime, m_targetAlpha);
    }
    
    // 点击闪光计时
    if (m_clickFlashTimer > 0.0f) {
        m_clickFlashTimer -= deltaTime;
        if (m_clickFlashTimer <= 0.0f) m_clickFlashTimer = 0.0f;
    }
    
    // 回头看计时
    if (m_headTurnTimer > 0.0f) {
        m_headTurnTimer -= deltaTime;
        if (m_headTurnTimer <= 0.0f) {
            m_headTurnTimer = 0.0f;
            m_lookingAtUser = false;
        }
    }
    
    // 清除上帧点击状态
    m_wasClicked = false;
}

void ThemeMascot::updateAnimState(float volume, bool isPlaying) {
    if (volume > 100.0f) {
        m_animState = MascotAnimState::Excited;
        m_targetAlpha = 1.0f;
        m_stateTimer = 0.0f;
    } else if (m_lookingAtUser) {
        m_animState = MascotAnimState::Clicked;
        m_targetAlpha = 1.0f;
    } else if (volume < 20.0f) {
        m_animState = MascotAnimState::FadeOut;
        // 渐变到30%透明度
        m_targetAlpha = 0.3f;
    } else {
        m_animState = MascotAnimState::Idle;
        m_targetAlpha = 1.0f;
        if (!isPlaying && m_stateTimer > 5.0f) {
            // 没有播放时慢慢变淡
            m_targetAlpha = 0.5f;
        }
    }
}

// 拖拽处理

void ThemeMascot::updateDragState() {
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool mouseDown = ImGui::GetIO().MouseDown[0];
    bool mouseClicked = ImGui::GetIO().MouseClicked[0];
    
    // 检查是否在精灵范围内
    bool inBounds = (mousePos.x >= m_mascotBoundsMin.x && mousePos.x <= m_mascotBoundsMax.x &&
                     mousePos.y >= m_mascotBoundsMin.y && mousePos.y <= m_mascotBoundsMax.y);
    
    if (mouseClicked && inBounds) {
        // 开始拖拽
        m_isDragging = true;
        m_dragOffsetX = mousePos.x - m_mascotScreenX;
        m_dragOffsetY = mousePos.y - m_mascotScreenY;
        
        // 触发点击互动
        m_wasClicked = true;
        m_clickFlashTimer = CLICK_FLASH_DURATION;
        m_headTurnTimer = HEAD_TURN_DURATION;
        m_lookingAtUser = true;
    }
    
    if (m_isDragging) {
        if (mouseDown) {
            m_mascotScreenX = mousePos.x - m_dragOffsetX;
            m_mascotScreenY = mousePos.y - m_dragOffsetY;
            m_hasCustomPos = true;
        } else {
            m_isDragging = false;
        }
    }
}

// 🧜 美人鱼 (Ocean 主题)

void ThemeMascot::drawMermaid(ImDrawList* dl, float baseX, float baseY, float px) {
    auto dpx = [&](float x, float y, ImU32 col) {
        drawPixel(dl, baseX, baseY, px, x, y, col);
    };

    float sway = sinf(m_time * 2.0f) * 2.0f;
    
    ImU32 cSkin     = IM_COL32(255, 225, 195, 255);
    ImU32 cHair     = IM_COL32(70, 190, 240, 255);
    ImU32 cHairDark = IM_COL32(30, 130, 190, 255);
    ImU32 cHairLight= IM_COL32(140, 220, 255, 255);
    ImU32 cTail     = IM_COL32(0, 210, 230, 255);
    ImU32 cTailDark = IM_COL32(0, 150, 180, 255);
    ImU32 cTailLt   = IM_COL32(120, 240, 250, 255);
    ImU32 cShell    = IM_COL32(255, 160, 190, 255);   // 贝壳粉红
    ImU32 cShellDk  = IM_COL32(220, 110, 140, 255);   // 贝壳深色纹路
    ImU32 cShellLt  = IM_COL32(255, 210, 225, 255);   // 贝壳高光
    ImU32 cEyes     = IM_COL32(50, 100, 200, 255);
    ImU32 cLip      = IM_COL32(220, 120, 130, 200);
    ImU32 cCheek    = IM_COL32(255, 190, 190, 120);
    ImU32 cScale    = IM_COL32(0, 180, 200, 180);
    ImU32 cFinEdge  = IM_COL32(180, 255, 255, 200);   // 鳍边缘亮色
    
    bool lookLeft = m_lookingAtUser;
    
    // --- 大V形鱼尾鳍 🧜 (美人鱼标志性大扇尾!) ---
    int fw = (int)(sinf(m_time * 3.0f) * 2);
    // 尾柄 (连接尾身→扇尾)
    dpx(16, -1, cTail); dpx(16, 0, cTailDark);
    // 上扇叶 (向上展开4格)
    dpx(17, -1 + fw, cTail);    dpx(17, -2 + fw, cTailLt);
    dpx(18, -2 + fw, cTail);    dpx(18, -3 + fw, cTailLt);
    dpx(19, -3 + fw, cTailLt);  dpx(19, -4 + fw, cFinEdge);
    dpx(20, -4 + fw, cFinEdge); dpx(20, -3 + fw, cTailLt);
    // 上扇叶放射纹路
    dpx(18, -2 + fw, cTailDark); dpx(19, -3 + fw, cTailDark);
    // 下扇叶 (向下展开4格)
    dpx(17, 0 + fw, cTail);     dpx(17, 1 + fw, cTailLt);
    dpx(18, 1 + fw, cTail);     dpx(18, 2 + fw, cTailLt);
    dpx(19, 2 + fw, cTailLt);   dpx(19, 3 + fw, cFinEdge);
    dpx(20, 3 + fw, cFinEdge);  dpx(20, 2 + fw, cTailLt);
    // 下扇叶放射纹路
    dpx(18, 1 + fw, cTailDark); dpx(19, 2 + fw, cTailDark);
    // 尾鳍中脊线
    dpx(17, -1 + fw, cTailDark); dpx(18, -1 + fw, cTailDark);
    dpx(17, 0 + fw, cTailDark);  dpx(18, 0 + fw, cTailDark);
    
    // --- 鱼尾身 (3行厚实, 丰富鳞片纹) ---
    for (int i = 10; i <= 15; i++) {
        dpx((float)i, -2, cTailLt);
        dpx((float)i, -1, cTail);
        dpx((float)i, 0, cTailDark);
    }
    // 菱形鳞片纹理 (交错排列)
    dpx(10, -1, cScale); dpx(11, -2, cScale); dpx(12, -1, cScale);
    dpx(13, 0, cScale);  dpx(14, -1, cScale); dpx(11, 0, cScale);
    dpx(13, -2, cScale); dpx(15, -1, cScale);
    // 背脊高光
    dpx(10, -2, cTailLt); dpx(12, -2, cTailLt); dpx(14, -2, cTailLt);
    
    // --- 小背鳍 (鱼特征! 轻轻摆动) ---
    float finFlap = sinf(m_time * 4.0f);
    int ff = (int)(finFlap);
    dpx(11, -3 + ff, cTailLt); dpx(12, -3 + ff, cTail); dpx(13, -3 + ff, cTailLt);
    
    // --- 下半身 (臀腰过渡, 3行, 鳞片渐变) ---
    for (int i = 5; i <= 9; i++) {
        dpx((float)i, -2, cTailLt);
        dpx((float)i, -1, cTail);
        dpx((float)i, 0, cTailDark);
    }
    dpx(6, -2, cScale); dpx(8, -1, cScale); dpx(7, 0, cScale); dpx(9, -1, cScale);
    
    // --- 上身/躯干 ---
    dpx(2, -3, cSkin); dpx(3, -3, cSkin); dpx(4, -3, cSkin); dpx(5, -3, cSkin);
    dpx(3, -2, cSkin); dpx(4, -2, cSkin); dpx(5, -2, cSkin);
    
    // --- 粉红贝壳胸饰 🐚🐚 ---
    dpx(3, -3, cShell);  dpx(3, -2, cShellDk);    // 左贝壳
    dpx(5, -3, cShell);  dpx(5, -2, cShellDk);    // 右贝壳
    // 贝壳高光点
    dpx(3, -3, cShellLt);  dpx(5, -3, cShellLt);
    // 贝壳底部弧线纹路
    dl->AddLine(
        ImVec2(baseX + 2.8f * px, baseY - 2.2f * px),
        ImVec2(baseX + 3.8f * px, baseY - 2.2f * px),
        (cShellDk & 0x00FFFFFF) | ((ImU32)(int)(m_currentAlpha * 200) << 24), 1.0f);
    dl->AddLine(
        ImVec2(baseX + 4.8f * px, baseY - 2.2f * px),
        ImVec2(baseX + 5.8f * px, baseY - 2.2f * px),
        (cShellDk & 0x00FFFFFF) | ((ImU32)(int)(m_currentAlpha * 200) << 24), 1.0f);
    
    // --- 头部 (更圆润) ---
    dpx(2, -4, cSkin); dpx(3, -4, cSkin); dpx(4, -4, cSkin); dpx(5, -4, cSkin);
    dpx(2, -5, cSkin); dpx(3, -5, cSkin); dpx(4, -5, cSkin); dpx(5, -5, cSkin);
    dpx(3, -6, cSkin); dpx(4, -6, cSkin);
    
    // --- 眼睛 ---
    bool blinking = (m_frame == 0 || m_frame == 1);
    if (m_animState == MascotAnimState::FadeOut) {
        // 闭眼
        dpx(3, -5, cHairDark); dpx(5, -5, cHairDark);
    } else if (!blinking) {
        if (lookLeft) {
            dpx(2, -5, cEyes); dpx(4, -5, cEyes);
        } else {
            dpx(3, -5, cEyes); dpx(5, -5, cEyes);
        }
        // 高光
        dpx(3, -5, IM_COL32(255,255,255,120));
    } else {
        dpx(3, -5, cHairDark); dpx(5, -5, cHairDark);
    }
    
    // --- 微笑 ---
    dpx(4, -4, cLip);
    
    // --- 腮红 ---
    int ca = (int)(m_currentAlpha * 120);
    dl->AddCircleFilled(ImVec2(baseX + 2.0f * px, baseY - 4.5f * px), px * 0.9f, 
        (cCheek & 0x00FFFFFF) | ((ImU32)ca << 24), 8);
    dl->AddCircleFilled(ImVec2(baseX + 5.5f * px, baseY - 4.5f * px), px * 0.9f, 
        (cCheek & 0x00FFFFFF) | ((ImU32)ca << 24), 8);
    
    // --- 头发 (飘逸) ---
    int hw = (int)(sway * 0.5f);
    dpx(1, -6 + hw, cHair); dpx(2, -7, cHair); dpx(3, -7, cHair);
    dpx(4, -7, cHair); dpx(5, -7, cHair);
    dpx(6, -6 - hw, cHair); dpx(6, -5 - hw, cHairDark);
    dpx(0, -5 + hw, cHairDark); dpx(1, -5 + hw, cHair);
    dpx(-1, -4 + hw, cHair); dpx(0, -3 + hw, cHairLight);
    dpx(7, -4 - hw, cHair); dpx(7, -3 - hw, cHairLight);
    
    // --- 手臂 (摆动) ---
    if (m_animState != MascotAnimState::FadeOut) {
        int armWave = (m_frame / 3) % 3;
        dpx(6, -3, cSkin);
        dpx(7, -3 - armWave, cSkin);
        dpx(8, -2 - armWave, cSkin);
        if (m_animState == MascotAnimState::Excited) {
            dpx(8, -3 - armWave, cSkin);
            dpx(9, -4 - armWave, cSkin);  // 挥手更大幅
        }
    }
    
    // --- 气泡 ---
    for (int b = 0; b < 4; b++) {
        float bx = baseX + (9 + b * 2) * px + sinf(m_time * 2.0f + b) * 5;
        float by = baseY - 5 * px - fmodf(m_time * 15 + b * 10, 35);
        int ba = (int)((180 - fmodf(m_time * 15 + b * 10, 35) * 4) * m_currentAlpha);
        if (ba > 0) {
            dl->AddCircle(ImVec2(bx, by), 2.5f + b * 0.5f, 
                IM_COL32(200, 240, 255, ba), 8, 1.0f);
        }
    }
    
    // 更新包围盒 (尾鳍展开更大)
    m_mascotBoundsMin = ImVec2(baseX - 2 * px, baseY - 8 * px);
    m_mascotBoundsMax = ImVec2(baseX + 22 * px, baseY + 6 * px);
}

// 🍄 蘑菇精 (Sunset 主题)

void ThemeMascot::drawMushroom(ImDrawList* dl, float baseX, float baseY, float px) {
    auto dpx = [&](float x, float y, ImU32 col) {
        drawPixel(dl, baseX, baseY, px, x, y, col);
    };
    
    ImU32 cCap     = IM_COL32(255, 100, 70, 255);
    ImU32 cCapDark = IM_COL32(200, 60, 40, 255);
    ImU32 cCapLt   = IM_COL32(255, 150, 100, 255);
    ImU32 cSpot    = IM_COL32(255, 240, 210, 255);
    ImU32 cStem    = IM_COL32(255, 245, 235, 255);
    ImU32 cStemDk  = IM_COL32(235, 220, 200, 255);
    ImU32 cFace    = IM_COL32(60, 40, 30, 255);
    ImU32 cFeet    = IM_COL32(160, 100, 60, 255);
    ImU32 cCheek   = IM_COL32(255, 180, 170, 140);
    ImU32 cSmile   = IM_COL32(200, 80, 60, 255);
    
    bool lookLeft = m_lookingAtUser;
    
    // --- 蘑菇帽 (更大的圆顶) ---
    dpx(3, -9, cCap); dpx(4, -9, cCap); dpx(5, -9, cCap);
    dpx(2, -8, cCap); dpx(3, -8, cCap); dpx(4, -8, cCap); dpx(5, -8, cCap); dpx(6, -8, cCap);
    dpx(1, -7, cCap); dpx(2, -7, cCap); dpx(3, -7, cCap); dpx(4, -7, cCap);
    dpx(5, -7, cCap); dpx(6, -7, cCap); dpx(7, -7, cCap);
    dpx(0, -6, cCapDark); dpx(1, -6, cCap); dpx(2, -6, cCap); dpx(3, -6, cCap);
    dpx(4, -6, cCap); dpx(5, -6, cCap); dpx(6, -6, cCap); dpx(7, -6, cCap); dpx(8, -6, cCapDark);
    
    // 帽边高光
    dpx(1, -8, cCapLt); dpx(2, -9, cCapLt);
    
    // 白色圆点
    dpx(3, -9, cSpot); dpx(6, -8, cSpot); dpx(2, -7, cSpot); dpx(5, -7, cSpot);
    dpx(1, -6, cSpot); dpx(7, -6, cSpot);
    
    // --- 脸/茎 (更宽) ---
    dpx(2, -5, cStem); dpx(3, -5, cStem); dpx(4, -5, cStem); dpx(5, -5, cStem); dpx(6, -5, cStem);
    dpx(2, -4, cStem); dpx(3, -4, cStem); dpx(4, -4, cStem); dpx(5, -4, cStem); dpx(6, -4, cStem);
    dpx(2, -3, cStem); dpx(3, -3, cStem); dpx(4, -3, cStem); dpx(5, -3, cStem); dpx(6, -3, cStem);
    dpx(3, -2, cStemDk); dpx(4, -2, cStemDk); dpx(5, -2, cStemDk);
    dpx(3, -1, cStemDk); dpx(4, -1, cStemDk); dpx(5, -1, cStemDk);
    
    // --- 眼睛 ---
    if (m_animState == MascotAnimState::FadeOut) {
        // 闭眼横线
        dpx(3, -4, cFace); dpx(5, -4, cFace);
    } else {
        if (lookLeft) {
            // 往左看
            dpx(2, -5, cFace); dpx(2, -4, cFace);
            dpx(5, -5, cFace); dpx(5, -4, cFace);
        } else {
            // 正常看前方 (动漫风 ^_^)
            dpx(3, -5, cFace); dpx(3, -4, cFace);
            dpx(5, -5, cFace); dpx(5, -4, cFace);
        }
        // 高光
        dpx(3, -5, IM_COL32(255, 255, 255, 100));
        dpx(5, -5, IM_COL32(255, 255, 255, 100));
    }
    
    // --- 腮红 ---
    int ca = (int)(m_currentAlpha * 140);
    dl->AddCircleFilled(ImVec2(baseX + 2.0f * px, baseY - 3.5f * px), px * 1.0f,
        (cCheek & 0x00FFFFFF) | ((ImU32)ca << 24), 8);
    dl->AddCircleFilled(ImVec2(baseX + 6.0f * px, baseY - 3.5f * px), px * 1.0f,
        (cCheek & 0x00FFFFFF) | ((ImU32)ca << 24), 8);
    
    // --- 嘴巴 ---
    if (m_animState == MascotAnimState::Excited) {
        dpx(4, -3, cSmile); dpx(3, -3, cSmile); dpx(5, -3, cSmile);  // 大笑
    } else {
        dpx(4, -3, cSmile);  // 微笑
    }
    
    // --- 手臂 (挥手) ---
    if (m_animState == MascotAnimState::Excited || m_animState == MascotAnimState::Clicked) {
        int armUp = (m_frame / 2) % 3;
        dpx(7, -4 - armUp, cStem); dpx(8, -5 - armUp, cStem);
    }
    
    // --- 跑步腿 ---
    int legFrame = m_frame % 6;
    if (m_animState == MascotAnimState::FadeOut) {
        // 站着不动
        dpx(3, 0, cFeet); dpx(5, 0, cFeet);
    } else {
        if (legFrame < 3) {
            dpx(3, 0, cFeet); dpx(5, 1, cFeet);
        } else {
            dpx(3, 1, cFeet); dpx(5, 0, cFeet);
        }
    }
    
    // --- 奔跑尘土 ---
    if (m_animState == MascotAnimState::Idle || m_animState == MascotAnimState::Excited) {
        for (int d = 0; d < 5; d++) {
            float dx = baseX - d * 5 - 5;
            float dy = baseY + 1.5f * px + d;
            int da = (int)((120 - d * 25) * m_currentAlpha);
            if (da > 0) {
                dl->AddCircleFilled(ImVec2(dx, dy), 2.5f - d * 0.3f,
                    IM_COL32(200, 180, 150, da), 6);
            }
        }
    }
    
    // 更新包围盒
    m_mascotBoundsMin = ImVec2(baseX - 1 * px, baseY - 10 * px);
    m_mascotBoundsMax = ImVec2(baseX + 9 * px, baseY + 2 * px);
}

// 👻 幽灵 (Dark 主题)

void ThemeMascot::drawGhost(ImDrawList* dl, float baseX, float baseY, float px) {
    auto dpx = [&](float x, float y, ImU32 col) {
        drawPixel(dl, baseX, baseY, px, x, y, col);
    };
    
    float floatY = sinf(m_time * 2.0f) * 5.0f;
    float floatX = sinf(m_time * 1.3f) * 3.0f;
    baseY += floatY;
    baseX += floatX;
    
    int alphaBase = (m_animState == MascotAnimState::FadeOut) ? 80 : 200;
    int alphaPulse = (int)(sinf(m_time * 3.0f) * 30);
    int a = (int)((alphaBase + alphaPulse) * m_currentAlpha);
    
    ImU32 cGhost    = IM_COL32(225, 230, 245, a);
    ImU32 cGhostDk  = IM_COL32(190, 200, 220, (int)(a * 0.85f));
    ImU32 cGhostLt  = IM_COL32(240, 245, 255, (int)(a * 0.95f));
    ImU32 cEyes     = IM_COL32(40, 60, 120, 255);
    ImU32 cMouth    = IM_COL32(100, 120, 170, 200);
    ImU32 cBlush    = IM_COL32(200, 180, 220, (int)(a * 0.4f));
    
    bool lookLeft = m_lookingAtUser;
    
    // --- 身体 (更大更圆润) ---
    for (int i = 3; i < 8; i++) dpx((float)i, -8, cGhostLt);
    for (int i = 2; i < 9; i++) dpx((float)i, -7, cGhost);
    for (int i = 1; i < 10; i++) dpx((float)i, -6, cGhost);
    for (int i = 0; i < 11; i++) dpx((float)i, -5, cGhost);
    for (int i = 0; i < 11; i++) dpx((float)i, -4, cGhost);
    for (int i = 0; i < 11; i++) dpx((float)i, -3, cGhost);
    for (int i = 0; i < 11; i++) dpx((float)i, -2, cGhostDk);
    for (int i = 0; i < 11; i++) dpx((float)i, -1, cGhostDk);
    
    // 波浪形底部
    int wave = m_frame % 6;
    for (int i = 0; i < 11; i++) {
        int waveOff = ((i + wave) % 3 == 0) ? 1 : 0;
        dpx((float)i, 0 + waveOff, cGhostDk);
    }
    
    // --- 眼睛 ---
    if (m_animState == MascotAnimState::FadeOut) {
        dpx(3, -5, cEyes); dpx(4, -5, cEyes);
        dpx(7, -5, cEyes); dpx(8, -5, cEyes);
    } else {
        // 大眼睛
        dpx(3, -6, cEyes); dpx(4, -6, cEyes);
        dpx(3, -5, cEyes); dpx(4, -5, cEyes);
        dpx(7, -6, cEyes); dpx(8, -6, cEyes);
        dpx(7, -5, cEyes); dpx(8, -5, cEyes);
        // 高光
        dpx(3, -6, IM_COL32(255, 255, 255, 180));
        dpx(7, -6, IM_COL32(255, 255, 255, 180));
        if (lookLeft) {
            dpx(3, -5, IM_COL32(20, 20, 60, 255));
            dpx(7, -5, IM_COL32(20, 20, 60, 255));
        }
    }
    
    // --- 腮红 ---
    dl->AddCircleFilled(ImVec2(baseX + 2.0f * px, baseY - 4.0f * px), px * 1.0f, cBlush, 8);
    dl->AddCircleFilled(ImVec2(baseX + 9.0f * px, baseY - 4.0f * px), px * 1.0f, cBlush, 8);
    
    // --- 嘴 ---
    if (m_animState == MascotAnimState::Excited) {
        dpx(5, -4, cMouth); dpx(6, -4, cMouth);  // 惊讶O
    } else {
        dpx(4, -3, cMouth); dpx(5, -3, cMouth); dpx(6, -3, cMouth); dpx(7, -3, cMouth);
    }
    
    // --- 星光闪烁 ---
    if (m_frame % 3 == 0) {
        float sx = baseX + (rand() % 14 - 2) * px;
        float sy = baseY + (rand() % 10 - 10) * px;
        int sa = (int)(180 * m_currentAlpha);
        dl->AddCircleFilled(ImVec2(sx, sy), 2.5f, IM_COL32(255, 255, 255, sa), 4);
    }
    
    // --- 过载时发光 ---
    if (m_animState == MascotAnimState::Excited) {
        float glowPulse = 0.5f + 0.5f * sinf(m_time * 6.0f);
        int ga = (int)(60 * glowPulse * m_currentAlpha);
        dl->AddCircleFilled(ImVec2(baseX + 5 * px, baseY - 4 * px), 
            px * 8, IM_COL32(180, 200, 255, ga), 16);
    }
    
    // 更新包围盒 (需要考虑浮动偏移)
    m_mascotBoundsMin = ImVec2(baseX - 1 * px, baseY - 9 * px);
    m_mascotBoundsMax = ImVec2(baseX + 12 * px, baseY + 2 * px);
}

// 点击光效闪烁

void ThemeMascot::drawClickFlash(ImDrawList* dl, float cx, float cy) {
    if (m_clickFlashTimer <= 0.0f) return;
    
    float t = m_clickFlashTimer / CLICK_FLASH_DURATION;
    float radius = (1.0f - t) * 40.0f + 10.0f;
    int alpha = (int)(t * 180);
    
    // 放射光芒
    int numRays = 8;
    for (int i = 0; i < numRays; i++) {
        float angle = (float)i / numRays * 3.14159f * 2.0f + m_time * 2.0f;
        float innerR = radius * 0.3f;
        float outerR = radius;
        ImVec2 inner(cx + cosf(angle) * innerR, cy + sinf(angle) * innerR);
        ImVec2 outer(cx + cosf(angle) * outerR, cy + sinf(angle) * outerR);
        dl->AddLine(inner, outer, IM_COL32(255, 255, 200, alpha), 2.0f);
    }
    
    // 中心圆
    dl->AddCircleFilled(ImVec2(cx, cy), radius * 0.2f, IM_COL32(255, 255, 255, alpha / 2), 12);
    dl->AddCircle(ImVec2(cx, cy), radius * 0.5f, IM_COL32(255, 255, 200, alpha / 3), 12, 1.5f);
}

// ZZZ 睡眠文字 (低音量)

void ThemeMascot::drawSleepZzz(ImDrawList* dl, float baseX, float baseY, float px) {
    if (m_animState != MascotAnimState::FadeOut) return;
    if (m_currentAlpha > 0.4f) return;  // 只有足够淡时才显示
    
    float zAlpha = 120 + 50 * sinf(m_time * 3.0f);
    int zzz = m_frame / 4;
    
    if (zzz >= 0) dl->AddText(ImVec2(baseX + 12 * px, baseY - 8 * px), 
        IM_COL32(255, 255, 255, (int)(zAlpha * 0.8f)), "z");
    if (zzz >= 1) dl->AddText(ImVec2(baseX + 14 * px, baseY - 10 * px), 
        IM_COL32(255, 255, 255, (int)(zAlpha * 0.6f)), "z");
    if (zzz >= 2) dl->AddText(ImVec2(baseX + 16 * px, baseY - 12 * px), 
        IM_COL32(255, 255, 255, (int)(zAlpha * 0.4f)), "Z");
}

// 音量条边框特效

void ThemeMascot::renderVolumeBarEffects(ImDrawList* dl, ThemeType theme,
                                         ImVec2 pos, float w, float h, float volume) {
    switch (theme) {
        case ThemeType::Ocean:   drawOceanBubbles(dl, pos, w, h); break;
        case ThemeType::Sunset:  drawSunsetLava(dl, pos, w, h); break;
        case ThemeType::Dark:    drawDarkMist(dl, pos, w, h); break;
        case ThemeType::RoseCedar: drawKittySparkles(dl, pos, w, h); break;
        case ThemeType::Gray:    drawGrayPulse(dl, pos, w, h); break;
        default: break;
    }
}

void ThemeMascot::drawOceanBubbles(ImDrawList* dl, ImVec2 pos, float w, float h) {
    // 🌊 水光边框 + 气泡 (加强版!)
    float glowPulse = 0.4f + 0.35f * sinf(m_time * 1.8f);
    // 内层青光
    dl->AddRect(ImVec2(pos.x - 1, pos.y - 1),
                ImVec2(pos.x + w + 1, pos.y + h + 1),
                IM_COL32(60, 190, 255, (int)(80 * glowPulse)), 4.0f, 0, 1.5f);
    // 外层柔光
    dl->AddRect(ImVec2(pos.x - 3, pos.y - 3),
                ImVec2(pos.x + w + 3, pos.y + h + 3),
                IM_COL32(40, 160, 240, (int)(45 * glowPulse)), 5.0f, 0, 2.5f);
    // 气泡 (更多更大更明显)
    for (int b = 0; b < 6; b++) {
        float bubbleX = pos.x + w * (0.08f + 0.16f * b);
        float bubbleY = pos.y + h - fmodf(m_time * 15.0f + b * 12.0f, h + 20);
        float bubbleSize = 2.0f + sinf(m_time * 1.5f + b) * 1.0f;
        int bubbleAlpha = (int)(120 - (pos.y + h - bubbleY) * 2.0f);
        if (bubbleAlpha > 0 && bubbleY > pos.y - 8) {
            dl->AddCircleFilled(ImVec2(bubbleX, bubbleY), bubbleSize,
                IM_COL32(140, 210, 255, bubbleAlpha / 2), 8);
            dl->AddCircle(ImVec2(bubbleX, bubbleY), bubbleSize,
                IM_COL32(180, 230, 255, bubbleAlpha), 8, 1.0f);
        }
    }
}

void ThemeMascot::drawSunsetLava(ImDrawList* dl, ImVec2 pos, float w, float h) {
    // 🌅 暖琥珀脉动边框 (加强版!)
    float lavaPulse = 0.4f + 0.35f * sinf(m_time * 2.0f);
    // 内层暖光
    dl->AddRect(ImVec2(pos.x - 1, pos.y - 1),
                ImVec2(pos.x + w + 1, pos.y + h + 1),
                IM_COL32(255, 170, 60, (int)(75 * lavaPulse)), 4.0f, 0, 1.5f);
    // 外层柔光
    dl->AddRect(ImVec2(pos.x - 3, pos.y - 3),
                ImVec2(pos.x + w + 3, pos.y + h + 3),
                IM_COL32(255, 130, 45, (int)(40 * lavaPulse)), 5.0f, 0, 2.5f);
    // 底部暖色光带 (更宽更亮)
    float barGlow = 0.5f + 0.4f * sinf(m_time * 2.5f + 0.5f);
    dl->AddRectFilledMultiColor(
        ImVec2(pos.x, pos.y + h),
        ImVec2(pos.x + w, pos.y + h + 5),
        IM_COL32(255, 140, 45, (int)(50 * barGlow)),
        IM_COL32(255, 110, 35, (int)(40 * barGlow)),
        IM_COL32(255, 80, 20, 0),
        IM_COL32(255, 120, 40, 0));
    // 小熔岩点 (缓慢飘动)
    for (int i = 0; i < 3; i++) {
        float phase = fmodf(m_time * 0.4f + i * 1.2f, 1.0f);
        float lx = pos.x + w * (0.2f + 0.3f * i) + sinf(m_time + i) * 6.0f;
        float ly = pos.y + h + 2 - phase * 12.0f;
        float life = 1.0f - phase;
        dl->AddCircleFilled(ImVec2(lx, ly), 2.0f * life,
            IM_COL32(255, 150, 50, (int)(80 * life)), 6);
    }
}

void ThemeMascot::drawDarkMist(ImDrawList* dl, ImVec2 pos, float w, float h) {
    // 🌫️ 幽暗薄雾边框 (加强版!)
    float mistPulse = 0.3f + 0.3f * sinf(m_time * 1.2f);
    // 内层紫幽光
    dl->AddRect(ImVec2(pos.x - 1, pos.y - 1),
                ImVec2(pos.x + w + 1, pos.y + h + 1),
                IM_COL32(155, 150, 210, (int)(60 * mistPulse)), 4.0f, 0, 1.5f);
    // 外层迷雾光
    dl->AddRect(ImVec2(pos.x - 3, pos.y - 3),
                ImVec2(pos.x + w + 3, pos.y + h + 3),
                IM_COL32(130, 120, 195, (int)(35 * mistPulse)), 5.0f, 0, 2.5f);
    // 飘动雾气团 (更多更明显)
    for (int i = 0; i < 4; i++) {
        float phase = m_time * 0.35f + i * 1.57f;
        float wispX = pos.x + w * (0.15f + 0.23f * i) + sinf(phase) * 10.0f;
        float wispY = pos.y + h * 0.5f + cosf(phase * 0.7f) * 5.0f;
        int wispAlpha = (int)(35 + 20 * sinf(m_time * 1.5f + i));
        dl->AddCircleFilled(ImVec2(wispX, wispY), 8.0f + 3.0f * sinf(m_time * 0.8f + i),
            IM_COL32(170, 165, 220, wispAlpha), 12);
    }
}

// 过载特效 (音量 > 100%)

void ThemeMascot::renderOverdriveEffect(ImDrawList* dl, ThemeType theme,
                                        ImVec2 pos, float w, float h,
                                        float volume, float fireSpace) {
    if (volume <= 100.0f) return;
    float intensity = std::min((volume - 100.0f) / 20.0f, 1.0f);
    
    switch (theme) {
        case ThemeType::Ocean:   drawWaterOverdrive(dl, pos, w, h, intensity, fireSpace); break;
        case ThemeType::Sunset:  drawLavaOverdrive(dl, pos, w, h, intensity, fireSpace); break;
        case ThemeType::Dark:    drawMistOverdrive(dl, pos, w, h, intensity, fireSpace); break;
        case ThemeType::RoseCedar: drawHeartOverdrive(dl, pos, w, h, intensity, fireSpace); break;
        case ThemeType::Gray:    drawGrayOverdrive(dl, pos, w, h, intensity, fireSpace); break;
        default:                 drawFireOverdrive(dl, pos, w, h, intensity, fireSpace); break;
    }
}

void ThemeMascot::drawFireOverdrive(ImDrawList* dl, ImVec2 pos, float w, float h,
                                    float intensity, float fireSpace) {

    // Warm halo uses soft light and gradients; particles stay disabled.

    // 1) 底部热辐射渐变 (从音量条向上扩散的暖色光)
    float breathe = 0.6f + 0.4f * sinf(m_time * 2.5f);  // 缓慢呼吸感
    float glowH = fireSpace * 0.6f * intensity * breathe;
    if (glowH > 2.0f) {
        // 多层渐变叠加 — 越往上越透明
        for (int layer = 0; layer < 3; layer++) {
            float layerRatio = (float)layer / 3.0f;
            float layerH = glowH * (1.0f - layerRatio * 0.5f);
            int aBase = (int)((35 - layer * 10) * intensity);
            // 暖色渐变: 底部橙黄 → 顶部透明
            dl->AddRectFilledMultiColor(
                ImVec2(pos.x + 2 + layer * 3, pos.y - layerH),
                ImVec2(pos.x + w - 2 - layer * 3, pos.y + 2),
                IM_COL32(255, 160, 40, 0),      // 左上 (透明)
                IM_COL32(255, 120, 20, 0),       // 右上 (透明)
                IM_COL32(255, 90, 20, aBase),    // 右下 (橙)
                IM_COL32(255, 140, 40, aBase));   // 左下 (橙黄)
        }
    }
    
    // 2) 边缘发光线条 (音量条上下两道细光线在呼吸)
    float edgePulse = 0.5f + 0.5f * sinf(m_time * 3.0f);
    int edgeAlpha = (int)(50 * intensity * edgePulse);
    // 上边缘: 明亮的暖白线
    dl->AddLine(ImVec2(pos.x + 4, pos.y - 1), ImVec2(pos.x + w - 4, pos.y - 1),
        IM_COL32(255, 200, 120, edgeAlpha), 1.5f);
    // 下边缘
    dl->AddLine(ImVec2(pos.x + 4, pos.y + h + 1), ImVec2(pos.x + w - 4, pos.y + h + 1),
        IM_COL32(255, 150, 60, (int)(edgeAlpha * 0.6f)), 1.0f);
    
    // 3) 微尘火星 (极小、低透明度、缓慢上升 — 像余烬)
    for (int i = 0; i < (int)(5 * intensity); i++) {
        float seed = (float)i * 1.618f;  // 黄金比例分布
        float ex = pos.x + w * (0.2f + 0.6f * fmodf(seed * 0.618f, 1.0f));
        ex += sinf(m_time * 1.5f + seed * 3.0f) * 4.0f;  // 轻微左右飘
        float phase = fmodf(m_time * 0.8f + seed, 1.0f);  // 慢速上升
        float ey = pos.y - phase * fireSpace * 0.5f;
        float life = (1.0f - phase);
        float sparkSize = 1.0f + 0.5f * life;
        int sa = (int)(life * life * 120 * intensity);  // 二次衰减更自然
        if (sa > 5 && ey < pos.y) {
            // 暖白核心 + 橙色外晕
            dl->AddCircleFilled(ImVec2(ex, ey), sparkSize + 1.5f,
                IM_COL32(255, 120, 30, sa / 3), 6);
            dl->AddCircleFilled(ImVec2(ex, ey), sparkSize,
                IM_COL32(255, 230, 180, sa), 4);
        }
    }
    
    // 4) 呼吸光晕框 (柔和的双层边框)
    float outerPulse = 0.4f + 0.3f * sinf(m_time * 2.0f);
    float innerPulse = 0.5f + 0.3f * sinf(m_time * 2.8f + 1.0f);
    // 外层: 大范围柔光
    dl->AddRect(ImVec2(pos.x - 4, pos.y - 4),
                ImVec2(pos.x + w + 4, pos.y + h + 4),
                IM_COL32(255, 100, 30, (int)(25 * intensity * outerPulse)), 6.0f, 0, 3.0f);
    // 内层: 明亮细线
    dl->AddRect(ImVec2(pos.x - 1, pos.y - 1),
                ImVec2(pos.x + w + 1, pos.y + h + 1),
                IM_COL32(255, 180, 80, (int)(40 * intensity * innerPulse)), 4.0f, 0, 1.0f);
}

void ThemeMascot::drawWaterOverdrive(ImDrawList* dl, ImVec2 pos, float w, float h,
                                     float intensity, float fireSpace) {

    // 🌊 深海生物发光 — 优雅的焦散光纹和柔和的水光

    // 1) 水面焦散光纹 (像阳光透过水面的波纹投影)
    float causticH = fireSpace * 0.4f * intensity;
    if (causticH > 2.0f) {
        // 多条缓慢流动的光线
        for (int i = 0; i < 6; i++) {
            float seed = (float)i * 1.3f;
            float cx1 = pos.x + w * (0.1f + 0.15f * i) + sinf(m_time * 1.2f + seed) * 12.0f;
            float cx2 = cx1 + 15.0f + sinf(m_time * 0.8f + seed * 2.0f) * 8.0f;
            float cy = pos.y - causticH * (0.2f + 0.15f * sinf(m_time * 1.5f + seed * 0.7f));
            float lineAlpha = (30 + 15 * sinf(m_time * 2.0f + seed)) * intensity;
            // 柔光线条
            dl->AddLine(ImVec2(cx1, cy), ImVec2(cx2, cy + 3),
                IM_COL32(140, 220, 255, (int)lineAlpha), 2.0f);
            // 光线外晕
            dl->AddLine(ImVec2(cx1 - 2, cy), ImVec2(cx2 + 2, cy + 3),
                IM_COL32(80, 180, 240, (int)(lineAlpha * 0.3f)), 5.0f);
        }
    }
    
    // 2) 渐变水光 (从音量条向上的蓝色雾气)
    float breathe = 0.5f + 0.5f * sinf(m_time * 1.8f);
    float glowH = fireSpace * 0.35f * intensity * breathe;
    if (glowH > 1.0f) {
        dl->AddRectFilledMultiColor(
            ImVec2(pos.x + 3, pos.y - glowH),
            ImVec2(pos.x + w - 3, pos.y + 2),
            IM_COL32(0, 150, 220, 0),          // 左上 (透明)
            IM_COL32(0, 120, 200, 0),           // 右上
            IM_COL32(40, 160, 230, (int)(30 * intensity)),  // 右下
            IM_COL32(60, 180, 240, (int)(30 * intensity))); // 左下
    }
    
    // 3) 生物发光微粒 (极小的光点缓慢漂浮 — 像深海浮游生物)
    for (int i = 0; i < (int)(4 * intensity); i++) {
        float seed = (float)i * 2.236f;  // √5
        float px = pos.x + w * fmodf(seed * 0.382f + m_time * 0.03f, 1.0f);
        float phase = fmodf(m_time * 0.6f + seed * 0.5f, 1.0f);
        float py = pos.y - phase * fireSpace * 0.4f;
        px += sinf(m_time * 1.0f + seed) * 3.0f;
        float life = 1.0f - phase;
        // 闪烁感: 明暗交替
        float flicker = 0.5f + 0.5f * sinf(m_time * 4.0f + seed * 3.0f);
        int sa = (int)(life * flicker * 100 * intensity);
        if (sa > 5 && py < pos.y) {
            dl->AddCircleFilled(ImVec2(px, py), 2.5f,
                IM_COL32(100, 200, 255, sa / 3), 6);  // 外晕
            dl->AddCircleFilled(ImVec2(px, py), 1.2f,
                IM_COL32(180, 240, 255, sa), 4);       // 亮核
        }
    }
    
    // 4) 边框水光环 (双层脉动)
    float outerP = 0.3f + 0.3f * sinf(m_time * 1.5f);
    float innerP = 0.4f + 0.3f * sinf(m_time * 2.2f + 0.8f);
    dl->AddRect(ImVec2(pos.x - 4, pos.y - 4),
                ImVec2(pos.x + w + 4, pos.y + h + 4),
                IM_COL32(0, 140, 220, (int)(20 * intensity * outerP)), 6.0f, 0, 3.0f);
    dl->AddRect(ImVec2(pos.x - 1, pos.y - 1),
                ImVec2(pos.x + w + 1, pos.y + h + 1),
                IM_COL32(80, 200, 255, (int)(35 * intensity * innerP)), 4.0f, 0, 1.0f);
    
    // 5) 顶部光带闪烁 (像水面反光)
    float shineX = pos.x + fmodf(m_time * 25.0f, w + 40) - 20;  // 光带从左到右扫过
    float shineW = 30.0f;
    float shineA = 25.0f * intensity;
    if (shineX > pos.x - 20 && shineX < pos.x + w + 20) {
        dl->AddRectFilledMultiColor(
            ImVec2(shineX, pos.y - 2),
            ImVec2(shineX + shineW, pos.y + 1),
            IM_COL32(200, 240, 255, 0),
            IM_COL32(200, 240, 255, (int)shineA),
            IM_COL32(200, 240, 255, (int)shineA),
            IM_COL32(200, 240, 255, 0));
    }
}

void ThemeMascot::drawLavaOverdrive(ImDrawList* dl, ImVec2 pos, float w, float h,
                                    float intensity, float fireSpace) {

    // 🌅 琥珀熔光 — 温暖而内敛的能量辐射，像夕阳余晖

    // 1) 暖光渐变 (从音量条缓慢向上扩散的琥珀色光)
    float breathe = 0.5f + 0.5f * sinf(m_time * 2.0f);
    float glowH = fireSpace * 0.35f * intensity * breathe;
    if (glowH > 1.0f) {
        // 琥珀→透明渐变
        dl->AddRectFilledMultiColor(
            ImVec2(pos.x + 3, pos.y - glowH),
            ImVec2(pos.x + w - 3, pos.y + 2),
            IM_COL32(255, 160, 60, 0),         // 左上 (透明)
            IM_COL32(255, 130, 40, 0),          // 右上
            IM_COL32(255, 100, 30, (int)(25 * intensity)),  // 右下
            IM_COL32(255, 140, 50, (int)(25 * intensity))); // 左下
    }
    
    // 2) 热力线 (横向波动的半透明热线 — 像热空气折射)
    for (int i = 0; i < 3; i++) {
        float lineY = pos.y - fireSpace * 0.08f * (i + 1) * intensity;
        float waveAmp = 2.0f + sinf(m_time * 1.5f + i * 1.2f) * 1.5f;
        int lineAlpha = (int)((20 - i * 5) * intensity);
        
        // 用多段短线模拟热力折射
        ImVec2 prev(pos.x + 5, lineY + sinf(m_time * 2.0f + i) * waveAmp);
        for (int s = 1; s <= 8; s++) {
            float sx = pos.x + 5 + (w - 10) * (float)s / 8.0f;
            float sy = lineY + sinf(m_time * 2.0f + s * 0.8f + i * 1.5f) * waveAmp;
            dl->AddLine(prev, ImVec2(sx, sy),
                IM_COL32(255, 180, 80, lineAlpha), 1.5f);
            prev = ImVec2(sx, sy);
        }
    }
    
    // 3) 金色微粒 (像日落时空气中的金色尘埃)
    for (int i = 0; i < (int)(4 * intensity); i++) {
        float seed = (float)i * 1.414f;  // √2
        float px = pos.x + w * fmodf(seed * 0.618f + m_time * 0.02f, 1.0f);
        float phase = fmodf(m_time * 0.5f + seed * 0.7f, 1.0f);
        float py = pos.y - phase * fireSpace * 0.3f;
        px += sinf(m_time * 0.8f + seed * 2.0f) * 5.0f;
        float life = 1.0f - phase;
        // 闪烁 (像金粉在光线下)
        float shimmer = 0.3f + 0.7f * powf(0.5f + 0.5f * sinf(m_time * 5.0f + seed * 4.0f), 3.0f);
        int sa = (int)(life * shimmer * 110 * intensity);
        if (sa > 5 && py < pos.y) {
            dl->AddCircleFilled(ImVec2(px, py), 2.0f,
                IM_COL32(255, 180, 60, sa / 3), 6);
            dl->AddCircleFilled(ImVec2(px, py), 0.8f,
                IM_COL32(255, 240, 180, sa), 4);
        }
    }
    
    // 4) 琥珀边框光环
    float outerP = 0.3f + 0.3f * sinf(m_time * 1.8f);
    float innerP = 0.4f + 0.3f * sinf(m_time * 2.5f + 0.5f);
    dl->AddRect(ImVec2(pos.x - 4, pos.y - 4),
                ImVec2(pos.x + w + 4, pos.y + h + 4),
                IM_COL32(255, 130, 40, (int)(18 * intensity * outerP)), 6.0f, 0, 3.0f);
    dl->AddRect(ImVec2(pos.x - 1, pos.y - 1),
                ImVec2(pos.x + w + 1, pos.y + h + 1),
                IM_COL32(255, 200, 100, (int)(30 * intensity * innerP)), 4.0f, 0, 1.0f);
    
    // 5) 底部余晖带 (音量条底部有一道暖色光带)
    float barGlowA = 20.0f * intensity * (0.6f + 0.4f * sinf(m_time * 2.2f));
    dl->AddRectFilledMultiColor(
        ImVec2(pos.x, pos.y + h),
        ImVec2(pos.x + w, pos.y + h + 4),
        IM_COL32(255, 140, 50, (int)barGlowA),
        IM_COL32(255, 100, 30, (int)barGlowA),
        IM_COL32(255, 80, 20, 0),
        IM_COL32(255, 120, 40, 0));
}

void ThemeMascot::drawMistOverdrive(ImDrawList* dl, ImVec2 pos, float w, float h,
                                    float intensity, float fireSpace) {

    // 👻 灵魂脉冲 — 暗色调中的幽蓝紫光，高级感十足

    // 1) 灵魂能量场 (从音量条向上扩散的幽蓝紫光)
    float breathe = 0.4f + 0.6f * sinf(m_time * 1.5f);
    float glowH = fireSpace * 0.3f * intensity * breathe;
    if (glowH > 1.0f) {
        dl->AddRectFilledMultiColor(
            ImVec2(pos.x + 4, pos.y - glowH),
            ImVec2(pos.x + w - 4, pos.y + 2),
            IM_COL32(120, 100, 180, 0),         // 左上 (透明)
            IM_COL32(100, 90, 170, 0),           // 右上
            IM_COL32(140, 130, 200, (int)(22 * intensity)),  // 右下
            IM_COL32(120, 120, 190, (int)(22 * intensity))); // 左下
    }
    
    // 2) 扫描线 (一道淡光从左到右缓慢扫过 — 像灵魂探测)
    float scanX = pos.x + fmodf(m_time * 15.0f, w + 60) - 30;
    float scanW = 40.0f;
    int scanA = (int)(20 * intensity);
    if (scanX > pos.x - 30 && scanX < pos.x + w + 30) {
        // 扫描线从音量条扫过上方空间
        float scanTop = pos.y - fireSpace * 0.3f * intensity;
        dl->AddRectFilledMultiColor(
            ImVec2(scanX, scanTop),
            ImVec2(scanX + scanW, pos.y + 2),
            IM_COL32(160, 140, 220, 0),
            IM_COL32(180, 170, 240, scanA),
            IM_COL32(180, 170, 240, scanA),
            IM_COL32(160, 140, 220, 0));
    }
    
    // 3) 幽光微粒 (比其他主题更稀疏更暗淡 — 神秘气氛)
    for (int i = 0; i < (int)(3 * intensity); i++) {
        float seed = (float)i * 1.732f;  // √3
        float px = pos.x + w * fmodf(seed * 0.618f + m_time * 0.015f, 1.0f);
        float phase = fmodf(m_time * 0.4f + seed * 0.6f, 1.0f);
        float py = pos.y - phase * fireSpace * 0.35f;
        px += sinf(m_time * 0.7f + seed * 1.5f) * 6.0f;
        py += cosf(m_time * 0.5f + seed * 2.0f) * 3.0f;  // 不规则漂浮
        float life = 1.0f - phase;
        // 幽灵般的忽明忽暗
        float flicker = powf(0.5f + 0.5f * sinf(m_time * 3.0f + seed * 5.0f), 2.0f);
        int sa = (int)(life * flicker * 80 * intensity);
        if (sa > 3 && py < pos.y) {
            dl->AddCircleFilled(ImVec2(px, py), 3.0f,
                IM_COL32(150, 140, 210, sa / 3), 8);  // 柔和外晕
            dl->AddCircleFilled(ImVec2(px, py), 1.0f,
                IM_COL32(200, 200, 255, sa), 4);       // 微弱亮核
        }
    }
    
    // 4) 灵魂边框 (紫灰双层脉动，比其他主题更暗)
    float outerP = 0.25f + 0.25f * sinf(m_time * 1.2f);
    float innerP = 0.3f + 0.25f * sinf(m_time * 1.8f + 1.0f);
    dl->AddRect(ImVec2(pos.x - 4, pos.y - 4),
                ImVec2(pos.x + w + 4, pos.y + h + 4),
                IM_COL32(130, 120, 190, (int)(15 * intensity * outerP)), 6.0f, 0, 3.0f);
    dl->AddRect(ImVec2(pos.x - 1, pos.y - 1),
                ImVec2(pos.x + w + 1, pos.y + h + 1),
                IM_COL32(170, 165, 220, (int)(28 * intensity * innerP)), 4.0f, 0, 1.0f);
    
    // 5) 微弱的边缘发光线 (底部)
    float edgePulse = 0.3f + 0.3f * sinf(m_time * 2.0f);
    dl->AddLine(ImVec2(pos.x + 6, pos.y + h + 1), ImVec2(pos.x + w - 6, pos.y + h + 1),
        IM_COL32(160, 150, 210, (int)(25 * intensity * edgePulse)), 1.0f);
}

// 🐱 猫咪 (Kitty 主题) — Hello Kitty 风格可爱小猫

void ThemeMascot::drawKitty(ImDrawList* dl, float baseX, float baseY, float px) {
    auto dpx = [&](float x, float y, ImU32 col) {
        drawPixel(dl, baseX, baseY, px, x, y, col);
    };
    
    // 参考 Hello Kitty 原版配色
    ImU32 cWhite   = IM_COL32(255, 252, 248, 255);    // 猫咪白色
    ImU32 cOutline = IM_COL32(230, 160, 180, 255);     // 粉色轮廓 (参照图)
    ImU32 cPink    = IM_COL32(240, 160, 185, 255);     // 蝴蝶结粉
    ImU32 cPinkDk  = IM_COL32(210, 110, 145, 255);     // 深粉
    ImU32 cPinkLt  = IM_COL32(255, 200, 215, 255);     // 浅粉
    ImU32 cNose    = IM_COL32(255, 210, 80, 255);      // 鼻子黄
    ImU32 cEyes    = IM_COL32(40, 35, 35, 255);        // 眼睛黑
    ImU32 cBody    = IM_COL32(245, 180, 195, 255);     // 身体粉 (条纹衣)
    ImU32 cStripe  = IM_COL32(255, 220, 230, 255);     // 浅条纹
    ImU32 cBlue    = IM_COL32(160, 200, 240, 255);     // 水蓝 (参照图翅膀)
    
    float bob = sinf(m_time * 2.5f) * 2.0f;
    baseY += bob;
    
    bool lookLeft = m_lookingAtUser;
    
// 粉色轮廓 (整体描边 — Hello Kitty 标志性粉边)
    // 头顶轮廓
    dpx(0, -8, cOutline); dpx(9, -8, cOutline);
    dpx(0, -7, cOutline); dpx(10, -7, cOutline);
    for (int i = 0; i < 11; i++) dpx((float)i, -10, cOutline); // 顶部
    dpx(0, -9, cOutline); dpx(10, -9, cOutline);
    // 侧面轮廓
    for (int y = -7; y <= -2; y++) { dpx(-1, (float)y, cOutline); dpx(10, (float)y, cOutline); }
    // 底部轮廓
    for (int i = 0; i < 10; i++) dpx((float)i, -1, cOutline);
    
// 耳朵 (三角形 + 粉色内部)
    dpx(1, -10, cWhite); dpx(2, -10, cWhite);   // 左耳尖
    dpx(1, -9, cWhite); dpx(2, -9, cWhite); dpx(3, -9, cWhite);
    dpx(7, -10, cWhite); dpx(8, -10, cWhite);   // 右耳尖
    dpx(6, -9, cWhite); dpx(7, -9, cWhite); dpx(8, -9, cWhite);
    // 耳内粉色
    dpx(2, -9, cPinkLt); dpx(7, -9, cPinkLt);
    
// 头部 (大圆脸 — 白色)
    for (int i = 1; i < 9; i++) dpx((float)i, -8, cWhite);
    for (int i = 0; i < 10; i++) dpx((float)i, -7, cWhite);
    for (int i = 0; i < 10; i++) dpx((float)i, -6, cWhite);
    for (int i = 0; i < 10; i++) dpx((float)i, -5, cWhite);
    for (int i = 0; i < 10; i++) dpx((float)i, -4, cWhite);
    for (int i = 1; i < 9; i++) dpx((float)i, -3, cWhite);
    
// 蝴蝶结 (右耳旁 — 更大更明显)
    // 蝴蝶结左瓣
    dpx(8, -9, cPink); dpx(9, -9, cPink);
    dpx(8, -8, cPink); dpx(9, -8, cPinkDk);
    dpx(8, -7, cPink);
    // 蝴蝶结右瓣
    dpx(10, -9, cPink); dpx(11, -9, cPink);
    dpx(10, -8, cPinkDk); dpx(11, -8, cPink);
    dpx(11, -7, cPink);
    // 蝴蝶结中心 (深色圆点)
    dpx(9, -8, cPinkDk); dpx(10, -8, cPinkDk);
    // 蝴蝶结小尾巴
    dpx(12, -8, cPinkLt);
    
// 眼睛 (椭圆形黑豆眼)
    if (m_animState == MascotAnimState::FadeOut) {
        // 闭眼 (弯线)
        dpx(2, -6, cEyes); dpx(3, -6, cEyes);
        dpx(7, -6, cEyes); dpx(8, -6, cEyes);
    } else {
        // 大黑豆眼 (2x2)
        dpx(2, -7, cEyes); dpx(3, -7, cEyes);
        dpx(2, -6, cEyes); dpx(3, -6, cEyes);
        dpx(7, -7, cEyes); dpx(8, -7, cEyes);
        dpx(7, -6, cEyes); dpx(8, -6, cEyes);
        // 白色高光 (左上角)
        dpx(2, -7, IM_COL32(255, 255, 255, 180));
        dpx(7, -7, IM_COL32(255, 255, 255, 180));
        // 点击时瞳孔偏移
        if (lookLeft) {
            dpx(2, -6, IM_COL32(15, 10, 15, 255));
            dpx(7, -6, IM_COL32(15, 10, 15, 255));
        }
    }
    
// 鼻子 (Hello Kitty 标志 — 小黄椭圆, 没有嘴巴!)
    dpx(5, -5, cNose); dpx(4, -5, cNose);
    
// 胡须 (6根 — 经典斜线)
    int wa = (int)(200 * m_currentAlpha);
    ImU32 wCol = IM_COL32(50, 40, 45, wa);
    float whiskerWave = sinf(m_time * 2.0f) * 0.8f;
    // 左侧3根
    dl->AddLine(ImVec2(baseX - 2*px, baseY - 6*px + whiskerWave), ImVec2(baseX + 1*px, baseY - 5.5f*px), wCol, 1.2f);
    dl->AddLine(ImVec2(baseX - 2*px, baseY - 5*px), ImVec2(baseX + 1*px, baseY - 5*px), wCol, 1.2f);
    dl->AddLine(ImVec2(baseX - 2*px, baseY - 4*px - whiskerWave), ImVec2(baseX + 1*px, baseY - 4.5f*px), wCol, 1.2f);
    // 右侧3根
    dl->AddLine(ImVec2(baseX + 9*px, baseY - 5.5f*px), ImVec2(baseX + 12*px, baseY - 6*px + whiskerWave), wCol, 1.2f);
    dl->AddLine(ImVec2(baseX + 9*px, baseY - 5*px), ImVec2(baseX + 12*px, baseY - 5*px), wCol, 1.2f);
    dl->AddLine(ImVec2(baseX + 9*px, baseY - 4.5f*px), ImVec2(baseX + 12*px, baseY - 4*px - whiskerWave), wCol, 1.2f);
    
// 身体 (粉色条纹衣服 — 参照图)
    for (int i = 2; i < 8; i++) dpx((float)i, -2, cBody);
    for (int i = 1; i < 9; i++) dpx((float)i, -1, cBody);
    for (int i = 2; i < 8; i++) dpx((float)i, 0, cBody);
    // 条纹
    dpx(3, -1, cStripe); dpx(5, -1, cStripe); dpx(7, -1, cStripe);
    dpx(3, 0, cStripe); dpx(5, 0, cStripe);
    // 小脚丫 (白色)
    dpx(2, 1, cWhite); dpx(3, 1, cWhite);
    dpx(6, 1, cWhite); dpx(7, 1, cWhite);
    dpx(2, 2, cOutline); dpx(7, 2, cOutline);
    
// 尾巴 (长弧形 — 更明显!)
    float tailWave = sinf(m_time * 3.0f) * 2.0f;
    // 尾巴根部 → 延伸到右侧弯曲向上
    dpx(8, 0, cWhite);
    dpx(9, -1, cWhite);
    dpx(10, -2 + tailWave * 0.3f, cWhite);
    dpx(11, -3 + tailWave * 0.5f, cWhite);
    dpx(12, -4 + tailWave * 0.7f, cWhite);
    dpx(13, -5 + tailWave, cWhite);
    dpx(13, -6 + tailWave, cPinkLt);  // 尾尖粉色
    // 尾巴轮廓
    dl->AddLine(ImVec2(baseX + 8*px + px*0.5f, baseY + 0*px + px*0.5f),
                ImVec2(baseX + 13*px + px*0.5f, baseY + (-5 + tailWave)*px + px*0.5f),
                IM_COL32(230, 160, 180, (int)(160 * m_currentAlpha)), 1.5f);
    
// 水蓝翅膀 (参照图 — 左侧小翅膀)
    int wingBob = (m_frame % 8 < 3) ? -1 : 0;
    dpx(-1, -3 + wingBob, cBlue); dpx(-2, -4 + wingBob, cBlue);
    dpx(-1, -4 + wingBob, cBlue); dpx(-2, -5 + wingBob, cBlue);
    dpx(-1, -5 + wingBob, IM_COL32(190, 220, 250, 200));
    
// 腮红
    int ca = (int)(m_currentAlpha * 100);
    dl->AddCircleFilled(ImVec2(baseX + 1.0f * px, baseY - 5.0f * px), px * 1.3f,
        IM_COL32(255, 190, 195, ca), 8);
    dl->AddCircleFilled(ImVec2(baseX + 8.5f * px, baseY - 5.0f * px), px * 1.3f,
        IM_COL32(255, 190, 195, ca), 8);
    
// 过载时蝴蝶结闪烁 + 翅膀发光
    if (m_animState == MascotAnimState::Excited) {
        float flash = 0.5f + 0.5f * sinf(m_time * 8.0f);
        int fa = (int)(80 * flash * m_currentAlpha);
        dl->AddCircleFilled(ImVec2(baseX + 10 * px, baseY - 8.5f * px), px * 3.0f,
            IM_COL32(255, 180, 220, fa), 10);
        dl->AddCircleFilled(ImVec2(baseX - 1.5f * px, baseY - 4 * px), px * 2.5f,
            IM_COL32(160, 200, 240, fa / 2), 8);
    }
    
// 点击时尾巴翘起变粉
    if (m_lookingAtUser) {
        dpx(13, -6 + tailWave, cPink);
        dpx(12, -5 + tailWave * 0.7f, cPinkLt);
    }
    
    // 更新包围盒
    m_mascotBoundsMin = ImVec2(baseX - 3 * px, baseY - 11 * px);
    m_mascotBoundsMax = ImVec2(baseX + 14 * px, baseY + 3 * px);
}

// 🐱 Kitty 星光边框

void ThemeMascot::drawKittySparkles(ImDrawList* dl, ImVec2 pos, float w, float h) {
    // 粉色脉动边框 (加强版!)
    float glowPulse = 0.4f + 0.35f * sinf(m_time * 2.0f);
    dl->AddRect(ImVec2(pos.x - 1, pos.y - 1),
                ImVec2(pos.x + w + 1, pos.y + h + 1),
                IM_COL32(240, 140, 180, (int)(75 * glowPulse)), 4.0f, 0, 1.5f);
    dl->AddRect(ImVec2(pos.x - 3, pos.y - 3),
                ImVec2(pos.x + w + 3, pos.y + h + 3),
                IM_COL32(210, 110, 160, (int)(40 * glowPulse)), 5.0f, 0, 2.5f);
    // 星星闪烁 (更多更大更亮!)
    for (int i = 0; i < 5; i++) {
        float sx = pos.x + w * (0.1f + 0.2f * i) + sinf(m_time * 1.5f + i * 2.0f) * 6;
        float sy = pos.y + h * 0.5f + cosf(m_time * 1.0f + i * 1.5f) * (h * 0.35f);
        float sparkle = 0.4f + 0.6f * powf(0.5f + 0.5f * sinf(m_time * 3.0f + i * 2.5f), 2.0f);
        int sa = (int)(110 * sparkle);
        // 四角星
        float sz = 3.5f * sparkle;
        dl->AddLine(ImVec2(sx - sz, sy), ImVec2(sx + sz, sy), IM_COL32(255, 200, 235, sa), 1.2f);
        dl->AddLine(ImVec2(sx, sy - sz), ImVec2(sx, sy + sz), IM_COL32(255, 200, 235, sa), 1.2f);
        // 对角线 (八角星效果)
        float sz2 = sz * 0.6f;
        dl->AddLine(ImVec2(sx - sz2, sy - sz2), ImVec2(sx + sz2, sy + sz2), IM_COL32(255, 180, 220, sa / 2), 0.8f);
        dl->AddLine(ImVec2(sx + sz2, sy - sz2), ImVec2(sx - sz2, sy + sz2), IM_COL32(255, 180, 220, sa / 2), 0.8f);
    }
}

// 🐱 Kitty 爱心过载特效

void ThemeMascot::drawHeartOverdrive(ImDrawList* dl, ImVec2 pos, float w, float h,
                                      float intensity, float fireSpace) {

    // 💕 粉色柔光 + 飘浮爱心 — 可爱但不过分

    // 1) 粉色渐变光
    float breathe = 0.5f + 0.5f * sinf(m_time * 2.2f);
    float glowH = fireSpace * 0.35f * intensity * breathe;
    if (glowH > 1.0f) {
        dl->AddRectFilledMultiColor(
            ImVec2(pos.x + 3, pos.y - glowH),
            ImVec2(pos.x + w - 3, pos.y + 2),
            IM_COL32(230, 120, 170, 0),
            IM_COL32(220, 100, 150, 0),
            IM_COL32(240, 140, 180, (int)(25 * intensity)),
            IM_COL32(230, 130, 170, (int)(25 * intensity)));
    }
    
    // 2) 飘浮爱心 (小小的, 柔和的)
    for (int i = 0; i < (int)(3 * intensity); i++) {
        float seed = (float)i * 1.618f;
        float hx = pos.x + w * fmodf(seed * 0.382f + m_time * 0.025f, 1.0f);
        float phase = fmodf(m_time * 0.6f + seed * 0.5f, 1.0f);
        float hy = pos.y - phase * fireSpace * 0.4f;
        hx += sinf(m_time * 1.0f + seed) * 4.0f;
        float life = 1.0f - phase;
        int ha = (int)(life * life * 90 * intensity);
        float hsz = 2.5f + 1.0f * life;
        if (ha > 5 && hy < pos.y) {
            // 简易心形: 两个小圆 + 一个三角
            dl->AddCircleFilled(ImVec2(hx - hsz * 0.3f, hy - hsz * 0.2f), hsz * 0.5f,
                IM_COL32(240, 140, 180, ha), 6);
            dl->AddCircleFilled(ImVec2(hx + hsz * 0.3f, hy - hsz * 0.2f), hsz * 0.5f,
                IM_COL32(240, 140, 180, ha), 6);
            dl->AddTriangleFilled(
                ImVec2(hx - hsz * 0.6f, hy), ImVec2(hx + hsz * 0.6f, hy),
                ImVec2(hx, hy + hsz * 0.7f),
                IM_COL32(240, 140, 180, ha));
        }
    }
    
    // 3) 粉色边框脉动
    float outerP = 0.3f + 0.3f * sinf(m_time * 1.8f);
    float innerP = 0.4f + 0.3f * sinf(m_time * 2.5f + 0.6f);
    dl->AddRect(ImVec2(pos.x - 4, pos.y - 4),
                ImVec2(pos.x + w + 4, pos.y + h + 4),
                IM_COL32(220, 100, 150, (int)(18 * intensity * outerP)), 6.0f, 0, 3.0f);
    dl->AddRect(ImVec2(pos.x - 1, pos.y - 1),
                ImVec2(pos.x + w + 1, pos.y + h + 1),
                IM_COL32(240, 160, 190, (int)(32 * intensity * innerP)), 4.0f, 0, 1.0f);
}

// 🤖 机器人 (Gray 太空灰主题) — 调音师小助手

void ThemeMascot::drawRobot(ImDrawList* dl, float baseX, float baseY, float px) {
    auto dpx = [&](float x, float y, ImU32 col) {
        drawPixel(dl, baseX, baseY, px, x, y, col);
    };
    
    // 🤖 Q版机器人 — 大头方身、LED眼睛、天线、小短手
    //    符合Pro Tools专业调音师风格, 科技感金属质感
    
    float bob = sinf(m_time * 2.0f) * 1.0f;
    if (m_animState == MascotAnimState::FadeOut) bob *= 0.2f;
    baseY += bob;
    
    // --- 颜色 ---
    ImU32 cBody    = IM_COL32(100, 105, 115, 255);    // 金属灰
    ImU32 cDark    = IM_COL32(70, 75, 85, 255);       // 暗部
    ImU32 cLight   = IM_COL32(140, 145, 155, 255);    // 高光
    ImU32 cAccent  = IM_COL32(74, 144, 217, 255);     // 蓝色强调
    ImU32 cLED     = IM_COL32(100, 200, 255, 255);    // LED亮蓝
    ImU32 cScreen  = IM_COL32(30, 35, 45, 255);       // 屏幕底色
    ImU32 cAntenna = IM_COL32(90, 95, 105, 255);      // 天线

    // ① 天线 (头顶两根)

    dpx(4, -13, cAntenna); dpx(4, -14, cAntenna);
    dpx(10, -13, cAntenna); dpx(10, -14, cAntenna);
    // 天线顶部LED小球 (闪烁)
    float blink1 = 0.5f + 0.5f * sinf(m_time * 4.0f);
    float blink2 = 0.5f + 0.5f * sinf(m_time * 4.0f + 1.5f);
    int ledA1 = (int)(255 * blink1 * m_currentAlpha);
    int ledA2 = (int)(255 * blink2 * m_currentAlpha);
    dl->AddCircleFilled(ImVec2(baseX + 4 * px, baseY - 15 * px), px * 0.8f,
        IM_COL32(100, 200, 255, ledA1), 8);
    dl->AddCircleFilled(ImVec2(baseX + 10 * px, baseY - 15 * px), px * 0.8f,
        IM_COL32(255, 150, 100, ledA2), 8);

    // ② 大方头 (圆角矩形)

    // 头部主体 (6x6格)
    for (int y = -12; y <= -7; y++) {
        for (int x = 2; x <= 12; x++) {
            dpx((float)x, (float)y, cBody);
        }
    }
    // 头顶高光
    for (int x = 3; x <= 11; x++) dpx((float)x, -12, cLight);
    dpx(2, -11, cLight); dpx(2, -10, cLight);
    // 头部暗边
    for (int x = 3; x <= 11; x++) dpx((float)x, -7, cDark);
    dpx(12, -11, cDark); dpx(12, -10, cDark); dpx(12, -9, cDark);

    // ③ 脸部屏幕 (深色区域 + LED眼睛)

    // 屏幕背景
    for (int y = -11; y <= -8; y++) {
        for (int x = 4; x <= 10; x++) {
            dpx((float)x, (float)y, cScreen);
        }
    }
    
    // LED眼睛 (会眨眼, 会看人)
    bool blink = (fmodf(m_time, 3.0f) < 0.15f);  // 每3秒眨一次
    float lookX = 0;
    if (m_lookingAtUser) lookX = 0.5f;  // 点击后看向用户
    
    if (m_animState == MascotAnimState::FadeOut) {
        // 睡眠模式 - 横线眼
        dpx(5, -9, cLED); dpx(6, -9, cLED);
        dpx(8, -9, cLED); dpx(9, -9, cLED);
    } else if (blink) {
        // 眨眼 - 横线
        dpx(5, -9, cLED); dpx(6, -9, cLED);
        dpx(8, -9, cLED); dpx(9, -9, cLED);
    } else {
        // 正常眼睛 (2x2像素方块)
        dpx(5 + lookX, -10, cLED); dpx(6 + lookX, -10, cLED);
        dpx(5 + lookX, -9, cLED); dpx(6 + lookX, -9, cLED);
        dpx(8 + lookX, -10, cLED); dpx(9 + lookX, -10, cLED);
        dpx(8 + lookX, -9, cLED); dpx(9 + lookX, -9, cLED);
        // 眼睛光晕
        dl->AddCircleFilled(ImVec2(baseX + (5.5f + lookX) * px, baseY - 9.5f * px), px * 1.5f,
            IM_COL32(74, 144, 217, (int)(40 * m_currentAlpha)), 8);
        dl->AddCircleFilled(ImVec2(baseX + (8.5f + lookX) * px, baseY - 9.5f * px), px * 1.5f,
            IM_COL32(74, 144, 217, (int)(40 * m_currentAlpha)), 8);
    }

    // ④ 小身体 (比头窄一点)

    for (int y = -6; y <= -2; y++) {
        for (int x = 4; x <= 10; x++) {
            dpx((float)x, (float)y, cBody);
        }
    }
    // 身体高光和暗部
    dpx(4, -6, cLight); dpx(5, -6, cLight);
    dpx(10, -3, cDark); dpx(10, -4, cDark);
    
    // 胸口音量指示器 (随音量变化)
    float vuLevel = m_animState == MascotAnimState::Excited ? 1.0f :
                    m_animState == MascotAnimState::Idle ? 0.6f : 0.2f;
    int vuBars = (int)(vuLevel * 3);
    ImU32 vuColors[] = {
        IM_COL32(80, 200, 80, 255),   // 绿
        IM_COL32(200, 200, 80, 255),  // 黄
        IM_COL32(200, 80, 80, 255)    // 红
    };
    for (int i = 0; i < vuBars; i++) {
        dpx(6 + (float)i, -4, vuColors[i]);
    }

    // ⑤ 小短手 (从身体两侧伸出)

    float armWave = sinf(m_time * 4.0f) * 0.3f;
    dpx(3, -5 + armWave, cBody); dpx(2, -4 + armWave, cBody);
    dpx(11, -5 - armWave, cBody); dpx(12, -4 - armWave, cBody);
    // 手掌 (小方块)
    dpx(1, -4 + armWave, cAccent); dpx(13, -4 - armWave, cAccent);

    // ⑥ 小短腿和脚 (方形)

    dpx(5, -1, cDark); dpx(5, 0, cDark);
    dpx(9, -1, cDark); dpx(9, 0, cDark);
    // 脚 (宽一点)
    dpx(4, 1, cBody); dpx(5, 1, cBody); dpx(6, 1, cBody);
    dpx(8, 1, cBody); dpx(9, 1, cBody); dpx(10, 1, cBody);

    // ⑦ Excited 状态特效 (电流环绕)

    if (m_animState == MascotAnimState::Excited) {
        float t = m_time;
        int ea = (int)(80 * m_currentAlpha);
        // 电流弧线
        for (int arc = 0; arc < 4; arc++) {
            float angle = t * 3.0f + arc * 1.57f;
            float ax = baseX + 7 * px + cosf(angle) * 10 * px;
            float ay = baseY - 6 * px + sinf(angle) * 5 * px;
            dl->AddCircleFilled(ImVec2(ax, ay), px * 0.5f,
                IM_COL32(100, 180, 255, ea), 6);
        }
        // 背后蓝色光晕
        dl->AddCircleFilled(ImVec2(baseX + 7 * px, baseY - 6 * px), px * 8,
            IM_COL32(74, 144, 217, (int)(30 * m_currentAlpha)), 16);
    }

    // ⑧ 点击特效 (屏幕闪烁)

    if (m_clickFlashTimer > 0.0f) {
        float flash = m_clickFlashTimer / CLICK_FLASH_DURATION;
        int fa = (int)(flash * 150 * m_currentAlpha);
        dl->AddRectFilled(
            ImVec2(baseX + 4 * px, baseY - 11 * px),
            ImVec2(baseX + 10 * px, baseY - 8 * px),
            IM_COL32(150, 200, 255, fa));
    }
    
    // 更新包围盒
    m_mascotBoundsMin = ImVec2(baseX + 1 * px, baseY - 16 * px);
    m_mascotBoundsMax = ImVec2(baseX + 14 * px, baseY + 2 * px);
}

// 🎛️ 太空灰边框特效 (Pro Tools 风格 - 电子脉冲)

void ThemeMascot::drawGrayPulse(ImDrawList* dl, ImVec2 pos, float w, float h) {
    // 冷调蓝色脉冲边框
    float glowPulse = 0.3f + 0.25f * sinf(m_time * 2.0f);
    // 内层蓝光
    dl->AddRect(ImVec2(pos.x - 1, pos.y - 1),
                ImVec2(pos.x + w + 1, pos.y + h + 1),
                IM_COL32(74, 144, 217, (int)(50 * glowPulse)), 4.0f, 0, 1.5f);
    // 外层柔光
    dl->AddRect(ImVec2(pos.x - 3, pos.y - 3),
                ImVec2(pos.x + w + 3, pos.y + h + 3),
                IM_COL32(50, 100, 180, (int)(25 * glowPulse)), 5.0f, 0, 2.0f);
    
    // 扫描线效果 (从左到右移动的光线)
    float scanPos = fmodf(m_time * 0.4f, 1.0f);
    float scanX = pos.x + w * scanPos;
    float scanAlpha = sinf(scanPos * 3.14159f) * 60;  // 中间亮两边暗
    dl->AddLine(ImVec2(scanX, pos.y - 2), ImVec2(scanX, pos.y + h + 2),
                IM_COL32(120, 180, 255, (int)scanAlpha), 2.0f);
    
    // 小光点 (科技感)
    for (int i = 0; i < 3; i++) {
        float seed = (float)i * 1.732f;
        float dotX = pos.x + w * fmodf(seed * 0.618f + m_time * 0.05f, 1.0f);
        float dotY = (i % 2 == 0) ? pos.y - 3 : pos.y + h + 3;
        float blink = 0.5f + 0.5f * sinf(m_time * 3.0f + seed * 2.0f);
        int dotAlpha = (int)(50 * blink);
        dl->AddCircleFilled(ImVec2(dotX, dotY), 1.5f, IM_COL32(100, 170, 255, dotAlpha), 6);
    }
}

// 🔷 太空灰过载特效 (电流/能量场)

void ThemeMascot::drawGrayOverdrive(ImDrawList* dl, ImVec2 pos, float w, float h,
                                    float intensity, float fireSpace) {
    // 1) 蓝色能量渐变
    float breathe = 0.5f + 0.5f * sinf(m_time * 2.2f);
    float glowH = fireSpace * 0.4f * intensity * breathe;
    if (glowH > 2.0f) {
        dl->AddRectFilledMultiColor(
            ImVec2(pos.x + 3, pos.y - glowH),
            ImVec2(pos.x + w - 3, pos.y + 2),
            IM_COL32(74, 144, 217, 0),
            IM_COL32(50, 100, 180, 0),
            IM_COL32(74, 144, 217, (int)(25 * intensity)),
            IM_COL32(50, 100, 180, (int)(25 * intensity)));
    }
    
    // 2) 电流弧线 (锯齿状闪电线条)
    for (int arc = 0; arc < (int)(4 * intensity); arc++) {
        float seed = (float)arc * 2.618f;
        float startX = pos.x + w * 0.1f + w * 0.8f * fmodf(seed * 0.382f, 1.0f);
        float arcHeight = fireSpace * 0.3f * intensity * (0.5f + 0.5f * sinf(m_time * 5.0f + seed));
        
        // 闪电点
        ImVec2 pts[4];
        pts[0] = ImVec2(startX, pos.y);
        pts[1] = ImVec2(startX + 3 * sinf(m_time * 8.0f + seed), pos.y - arcHeight * 0.3f);
        pts[2] = ImVec2(startX - 2 * cosf(m_time * 10.0f + seed), pos.y - arcHeight * 0.6f);
        pts[3] = ImVec2(startX + sinf(m_time * 7.0f + seed * 2.0f), pos.y - arcHeight);
        
        int arcAlpha = (int)(80 * intensity * (0.5f + 0.5f * sinf(m_time * 6.0f + seed)));
        for (int i = 0; i < 3; i++) {
            dl->AddLine(pts[i], pts[i+1], IM_COL32(100, 180, 255, arcAlpha), 1.5f);
        }
        // 核心亮点
        dl->AddCircleFilled(pts[3], 2.0f, IM_COL32(180, 220, 255, arcAlpha), 6);
    }
    
    // 3) 边框脉冲加强
    float outerP = 0.3f + 0.3f * sinf(m_time * 1.8f);
    float innerP = 0.4f + 0.3f * sinf(m_time * 2.5f + 0.5f);
    dl->AddRect(ImVec2(pos.x - 4, pos.y - 4),
                ImVec2(pos.x + w + 4, pos.y + h + 4),
                IM_COL32(50, 100, 180, (int)(20 * intensity * outerP)), 6.0f, 0, 2.5f);
    dl->AddRect(ImVec2(pos.x - 1, pos.y - 1),
                ImVec2(pos.x + w + 1, pos.y + h + 1),
                IM_COL32(74, 144, 217, (int)(35 * intensity * innerP)), 4.0f, 0, 1.5f);
}

// 过载提示颜色 (每个主题不同)

ImU32 ThemeMascot::getOverdriveColor(ThemeType theme, float volume) const {
    if (volume <= 100.0f) return IM_COL32(50, 55, 65, 255);
    float pulse = 0.7f + 0.3f * sinf(m_time * 4.0f);
    int a = (int)(200 * pulse);
    switch (theme) {
        case ThemeType::Ocean:   return IM_COL32(0, 180, 255, a);      // 海蓝
        case ThemeType::Sunset:  return IM_COL32(255, 140, 40, a);     // 岩浆橙
        case ThemeType::Dark:    return IM_COL32(160, 170, 230, a);    // 灵魂紫
        case ThemeType::RoseCedar: return IM_COL32(217, 152, 167, a);  // 柔玫瑰粉
        case ThemeType::Gray:    return IM_COL32(74, 144, 217, a);     // 电子蓝
        default:                 return IM_COL32(200, 70, 40, a);
    }
}

ImU32 ThemeMascot::getOverdriveFillStart(ThemeType theme) const {
    switch (theme) {
        case ThemeType::Ocean:   return IM_COL32(0, 160, 230, 255);
        case ThemeType::Sunset:  return IM_COL32(255, 120, 40, 255);
        case ThemeType::Dark:    return IM_COL32(140, 150, 200, 255);
        case ThemeType::RoseCedar: return IM_COL32(234, 176, 192, 255);
        case ThemeType::Gray:    return IM_COL32(74, 144, 217, 255);    // 电子蓝
        default:                 return IM_COL32(255, 100, 30, 255);
    }
}

ImU32 ThemeMascot::getOverdriveFillEnd(ThemeType theme) const {
    switch (theme) {
        case ThemeType::Ocean:   return IM_COL32(0, 100, 200, 255);
        case ThemeType::Sunset:  return IM_COL32(255, 80, 20, 255);
        case ThemeType::Dark:    return IM_COL32(100, 110, 180, 255);
        case ThemeType::RoseCedar: return IM_COL32(188, 118, 141, 255);
        case ThemeType::Gray:    return IM_COL32(50, 100, 180, 255);    // 深蓝
        default:                 return IM_COL32(255, 60, 20, 255);
    }
}

// 精灵主渲染函数 (最高层级)

bool ThemeMascot::renderMascot(ImDrawList* dl, ThemeType theme,
                               ImVec2 barPos, float barWidth, float barHeight) {
    float px = PIXEL_SIZE;
    
    // 计算默认位置 (在音量条上方)
    float defaultX = barPos.x + barWidth * 0.15f;
    float defaultY = barPos.y - 5.0f;
    
    // 蘑菇精特殊: 沿音量条奔跑
    if (theme == ThemeType::Sunset && !m_hasCustomPos) {
        float runPos = fmodf(m_mushroomRunPos, barWidth + 60) - 30;
        defaultX = barPos.x + runPos;
        float runBounce = fabsf(sinf(m_time * 12.0f)) * 4.0f;
        defaultY = barPos.y - 3 - runBounce;
    }
    
    // 使用自定义位置 (被拖拽过) 或默认位置
    if (!m_hasCustomPos) {
        m_mascotScreenX = defaultX;
        m_mascotScreenY = defaultY;
    }
    
    // 处理拖拽
    updateDragState();
    
    float mx = m_mascotScreenX;
    float my = m_mascotScreenY;
    
    // 根据主题绘制精灵
    switch (theme) {
        case ThemeType::Ocean:   drawMermaid(dl, mx, my, px); break;
        case ThemeType::Sunset:  drawMushroom(dl, mx, my, px); break;
        case ThemeType::Dark:    drawGhost(dl, mx, my, px); break;
        case ThemeType::RoseCedar: drawKitty(dl, mx, my, px); break;
        case ThemeType::Gray:    drawRobot(dl, mx, my, px); break;
        default: break;
    }
    
    // 绘制ZZZ
    drawSleepZzz(dl, mx, my, px);
    
    // 绘制点击光效
    if (m_clickFlashTimer > 0.0f) {
        float cx = (m_mascotBoundsMin.x + m_mascotBoundsMax.x) / 2;
        float cy = (m_mascotBoundsMin.y + m_mascotBoundsMax.y) / 2;
        drawClickFlash(dl, cx, cy);
    }
    
    // 拖拽时显示光标提示
    if (m_isDragging) {
        ImVec2 mousePos = ImGui::GetIO().MousePos;
        bool inBounds = (mousePos.x >= m_mascotBoundsMin.x && mousePos.x <= m_mascotBoundsMax.x &&
                         mousePos.y >= m_mascotBoundsMin.y && mousePos.y <= m_mascotBoundsMax.y);
        if (inBounds) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
    }
    
    return m_wasClicked;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
