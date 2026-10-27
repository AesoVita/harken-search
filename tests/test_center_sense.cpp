// TDD: 中心感旋钮系数（design + 实现计划 Task 2）
// 仅依赖 CenterSenseDsp，无 miniaudio/UI。
#include "CenterSenseDsp.h"
#include <cmath>
#include <cstring>
#include <iostream>

static int failed = 0;

#define ASSERT_AMOUNTS(k, aLF, aMid, aHF) do { \
    float lf, mid, hf; \
    CenterSenseDsp::getCenterSenseAmounts((k), lf, mid, hf); \
    if (std::abs(lf - (aLF)) > 1e-5f || std::abs(mid - (aMid)) > 1e-5f || std::abs(hf - (aHF)) > 1e-5f) { \
        std::cerr << "FAIL: getCenterSenseAmounts(" << (k) << ") got (" << lf << "," << mid << "," << hf << "), expected (" << (aLF) << "," << (aMid) << "," << (aHF) << ")\n"; \
        failed++; \
    } else std::cout << "OK: amounts(" << (k) << ") = (" << lf << "," << mid << "," << hf << ")\n"; \
} while(0)

#define ASSERT_WIDTHS(k, wLF, wMid, wHF) do { \
    float lf, mid, hf; \
    CenterSenseDsp::getCenterSenseWidthFactors((k), lf, mid, hf); \
    if (std::abs(lf - (wLF)) > 1e-5f || std::abs(mid - (wMid)) > 1e-5f || std::abs(hf - (wHF)) > 1e-5f) { \
        std::cerr << "FAIL: getCenterSenseWidthFactors(" << (k) << ") got (" << lf << "," << mid << "," << hf << "), expected (" << (wLF) << "," << (wMid) << "," << (wHF) << ")\n"; \
        failed++; \
    } else std::cout << "OK: widths(" << (k) << ") = (" << lf << "," << mid << "," << hf << ")\n"; \
} while(0)

int main() {
    // k=0: amount 全 0，width 全 1
    ASSERT_AMOUNTS(0.0f, 0.0f, 0.0f, 0.0f);
    ASSERT_WIDTHS(0.0f, 1.0f, 1.0f, 1.0f);

    // k=-1: amount 全 1
    ASSERT_AMOUNTS(-1.0f, 1.0f, 1.0f, 1.0f);

    // k=-0.5: amountMid = 0.25（平方关系），LF/HF = 0.5
    ASSERT_AMOUNTS(-0.5f, 0.5f, 0.25f, 0.5f);

    // k=+1: widthHF > widthMid > widthLF（2, 1.6, 1.2）
    ASSERT_WIDTHS(1.0f, 1.2f, 1.6f, 2.0f);

    // k=+0.5: 1.1, 1.3, 1.5
    ASSERT_WIDTHS(0.5f, 1.1f, 1.3f, 1.5f);

    // 拓宽侧 k>0 时 amount 为 0；收紧侧 k<0 时 width 为 1
    ASSERT_AMOUNTS(0.5f, 0.0f, 0.0f, 0.0f);
    ASSERT_WIDTHS(-0.5f, 1.0f, 1.0f, 1.0f);

// Bass Mono tests
    std::cout << "\n--- Bass Mono tests ---\n";

    // cutoffHz=0: passthrough (no change)
    {
        float buf[8] = { 0.5f, -0.3f, 0.7f, -0.1f, 0.2f, 0.8f, -0.4f, 0.6f };
        float orig[8]; memcpy(orig, buf, sizeof(buf));
        CenterSenseDsp::BassMonoState st = {};
        CenterSenseDsp::processBassMono(buf, 4, 48000, 0.0f, &st);
        bool pass = true;
        for (int i = 0; i < 8; i++) { if (std::abs(buf[i] - orig[i]) > 1e-9f) pass = false; }
        if (pass) std::cout << "OK: bassMono cutoff=0 passthrough\n";
        else { std::cerr << "FAIL: bassMono cutoff=0 should be passthrough\n"; failed++; }
    }

    // Low-frequency content (DC-like): after many samples with cutoff=500Hz, L and R should converge
    {
        const size_t N = 8000;
        float buf[N * 2];
        for (size_t i = 0; i < N; i++) { buf[2*i] = 1.0f; buf[2*i+1] = -1.0f; }
        CenterSenseDsp::BassMonoState st = {};
        CenterSenseDsp::processBassMono(buf, N, 48000, 500.0f, &st);
        // After settling, the last frame's L and R should be close to 0 (DC mono of 1 + -1)
        float lastL = buf[2*(N-1)];
        float lastR = buf[2*(N-1)+1];
        if (std::abs(lastL - lastR) < 0.01f && std::abs(lastL) < 0.1f) {
            std::cout << "OK: bassMono DC collapse (L=" << lastL << " R=" << lastR << ")\n";
        } else {
            std::cerr << "FAIL: bassMono DC collapse expected L≈R≈0, got L=" << lastL << " R=" << lastR << "\n";
            failed++;
        }
    }

    // High-frequency content should pass through mostly unchanged
    {
        const size_t N = 4800;
        float buf[N * 2];
        const float freq = 5000.0f;
        const float sr = 48000.0f;
        for (size_t i = 0; i < N; i++) {
            float s = std::sin(2.0f * 3.14159265f * freq * i / sr);
            buf[2*i] = s;       // L = sine
            buf[2*i+1] = 0.0f;  // R = silence
        }
        CenterSenseDsp::BassMonoState st = {};
        CenterSenseDsp::processBassMono(buf, N, 48000, 80.0f, &st);
        // Check that R channel stays near zero (high freq not collapsed)
        float maxR = 0.0f;
        for (size_t i = N/2; i < N; i++) { // skip transient
            float absR = std::abs(buf[2*i+1]);
            if (absR > maxR) maxR = absR;
        }
        if (maxR < 0.05f) {
            std::cout << "OK: bassMono HF passthrough (maxR=" << maxR << ")\n";
        } else {
            std::cerr << "FAIL: bassMono HF should not leak to R, maxR=" << maxR << "\n";
            failed++;
        }
    }

    // State reset: processing, reset state, processing again should not carry old state
    {
        float buf[4] = { 0.8f, -0.8f, 0.8f, -0.8f };
        CenterSenseDsp::BassMonoState st = {};
        CenterSenseDsp::processBassMono(buf, 2, 48000, 200.0f, &st);
        // State should be non-zero now
        bool stateNonZero = (std::abs(st.lp_L) > 1e-6f || std::abs(st.lp_R) > 1e-6f);
        st = CenterSenseDsp::BassMonoState{};
        bool stateZero = (st.lp_L == 0.0f && st.lp_R == 0.0f);
        if (stateNonZero && stateZero)
            std::cout << "OK: bassMono state reset\n";
        else {
            std::cerr << "FAIL: bassMono state reset\n"; failed++;
        }
    }

    if (failed) {
        std::cerr << "Total failures: " << failed << "\n";
        return 1;
    }
    std::cout << "\nAll CenterSenseDsp tests passed.\n";
    return 0;
}
