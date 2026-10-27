import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class DockDragHotPathTests(unittest.TestCase):
    def test_preview_isolated_product_trace_does_not_require_runtime_consent(self):
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        windows = (ROOT / "src" / "WindowsMain.cpp").read_text(encoding="utf-8")
        self.assertTrue("DockFrameTrace" in main, "Dock path needs actual call counters")
        self.assertTrue("ARIS_DOCK_TRACE" in windows, "isolated trace gate missing")
        self.assertNotIn("EnsureRequiredConsent", windows)

    def test_isolated_dock_frame_replay_is_wired(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        native = (ROOT / "tests" / "test_dock_hit_test.cpp").read_text(encoding="utf-8")
        self.assertTrue("TestDockPerfReplay" in cmake, "isolated replay target missing")
        self.assertTrue("RunDockPerfReplay" in native, "isolated replay entry missing")
        for production_call in ("BuildDockFrameGeometry", "HitDockTarget", "PlanDockPreview", "CollectLegalDockHints", "SendMessageW", "ImGui::Render"):
            self.assertTrue(production_call in native, f"production call missing: {production_call}")

    def test_replay_measures_real_win32_gl_render_and_present(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        native = (ROOT / "tests" / "test_dock_hit_test.cpp").read_text(encoding="utf-8")
        self.assertIn("imgui_impl_opengl3.cpp", cmake)
        for marker in ("CreateWindowExW", "CreateOpenGLReplayContext", "ImGui_ImplOpenGL3_RenderDrawData",
                       "SwapBuffers", "glRenderP95Ms", "presentP95Ms", "ncHitMessages",
                       "clearP95Ms", "drawP95Ms"):
            self.assertIn(marker, native)

    def test_replay_measures_secondary_imgui_viewport_cost(self):
        native = (ROOT / "tests" / "test_dock_hit_test.cpp").read_text(encoding="utf-8")
        for marker in ("ImGuiConfigFlags_ViewportsEnable", "ImGui::UpdatePlatformWindows",
                       "ImGui::RenderPlatformWindowsDefault", "viewportsP95Ms", "extraViewportCount",
                       "viewportCreates", "viewportDestroys",
                       "for (int panelIndex = 0; panelIndex < panelCount; ++panelIndex)"):
            self.assertIn(marker, native)

    def test_product_trace_separates_message_pump_render_and_present(self):
        windows = (ROOT / "src" / "WindowsMain.cpp").read_text(encoding="utf-8")
        loop = windows.split("while (running) {", 1)[1]
        for marker in ("dockTracePumpMs", "dockTraceGlRenderMs", "dockTracePresentMs"):
            self.assertIn(marker, loop, f"real product frame is missing {marker}")
        for field in ("s.pumpMs", "s.glRenderMs", "s.presentMs"):
            self.assertTrue(field in windows, f"trace JSON is missing {field}")

    def test_product_trace_flushes_partial_frames_only_at_shutdown(self):
        windows = (ROOT / "src" / "WindowsMain.cpp").read_text(encoding="utf-8")
        self.assertIn("writeDockTrace(false)", windows)
        self.assertIn("writeDockTrace(true)", windows)
        self.assertIn("dockTraceCompleteAttempted", windows)
        self.assertLess(windows.index("while (running) {"), windows.index("writeDockTrace(false)"))
        self.assertIn("dockIdleSamples.empty() && dockDragSamples.empty()", windows)

    def test_caption_trace_counts_real_host_and_extra_viewport_messages(self):
        windows = (ROOT / "src" / "WindowsMain.cpp").read_text(encoding="utf-8")
        bridge = (ROOT / "src" / "DockNativeCaptionBridge.h").read_text(encoding="utf-8")
        host = windows.split("LRESULT CALLBACK WindowProc(", 1)[1].split("if (msg >= WM_APP", 1)[0]
        extra = windows.split("static LRESULT CALLBACK ExtraViewportCaptionProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {", 1)[1].split("if (msg == WM_NCDESTROY)", 1)[0]
        self.assertIn("WM_NCHITTEST", host)
        self.assertIn("WM_NCHITTEST", extra)
        self.assertIn("hostNcHitTests", bridge)
        self.assertIn("extraNcHitTests", bridge)
        self.assertIn("s.dock.hostNcHitTests", windows)
        self.assertIn("s.dock.extraNcHitTests", windows)

    def test_product_trace_records_non_sensitive_split_handoff_geometry(self):
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        windows = (ROOT / "src" / "WindowsMain.cpp").read_text(encoding="utf-8")
        bridge = (ROOT / "src" / "DockNativeCaptionBridge.h").read_text(encoding="utf-8")
        for marker in ("zoneKind", "previewKind", "commitKind", "leafCountBefore", "leafCountAfter"):
            self.assertIn(marker, bridge)
            self.assertIn("s.dock." + marker, windows)
        self.assertIn("trace.commitKind", main)
        self.assertIn("trace.leafCountAfter", main)

    def test_floating_zone_geometry_is_skipped_when_panel_is_idle(self):
        source = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        driver = source.split("auto driveFloatingDockDrag", 1)[1].split(
            "auto drawFloatingDockTestPanel", 1
        )[0]
        self.assertRegex(
            driver,
            re.compile(
                r"if\s*\(\s*(?:[a-zA-Z_][\w]*::)*ShouldEvaluateFloatingDockZone\([^;]*\)\s*\)\s*\{"
                r"\s*zone\s*=\s*detectDockZone\(pos,\s*size,\s*moduleId\);",
                re.DOTALL,
            ),
            "idle floating windows must not rebuild the root leaf geometry every frame",
        )


if __name__ == "__main__":
    unittest.main()
