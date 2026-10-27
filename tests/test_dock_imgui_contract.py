"""RQ-20260928-01: pin the vendored docking snapshot and host window lifecycle."""

from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PIN = ROOT / "thirdparty" / "imgui" / "IMGUI_DOCKING_PIN.txt"
IMGUI_H = ROOT / "thirdparty" / "imgui" / "imgui.h"
WIN32_CPP = ROOT / "thirdparty" / "imgui" / "backends" / "imgui_impl_win32.cpp"
GL_CPP = ROOT / "thirdparty" / "imgui" / "backends" / "imgui_impl_opengl3.cpp"
WINMAIN = ROOT / "src" / "WindowsMain.cpp"
CMAKE = ROOT / "CMakeLists.txt"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _pin_fields() -> dict[str, str]:
    fields: dict[str, str] = {}
    for line in _read(PIN).splitlines():
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        fields[key.strip()] = value.strip()
    return fields


class DockImguiPinContract(unittest.TestCase):
    def test_pin_locks_the_checked_docking_snapshot(self) -> None:
        fields = _pin_fields()
        self.assertEqual(fields["source"], "https://github.com/ocornut/imgui")
        self.assertEqual(fields["branch"], "docking")
        self.assertEqual(fields["commit"], "d6d558009919275453f710539d8c835ff4623a8f")
        self.assertEqual(fields["version"], "1.92.6 WIP")
        self.assertEqual(fields["version_num"], "19259")
        self.assertEqual(fields["backends"], "imgui_impl_win32,imgui_impl_opengl3")
        self.assertEqual(fields["local_patches"], "none")
        header = _read(IMGUI_H)
        self.assertIn('#define IMGUI_VERSION       "1.92.6 WIP"', header)
        self.assertIn("#define IMGUI_VERSION_NUM   19259", header)
        self.assertIn("#define IMGUI_HAS_DOCK", header)
        self.assertIn("#define IMGUI_HAS_VIEWPORT", header)
        self.assertIn("ImGuiBackendFlags_PlatformHasViewports", _read(WIN32_CPP))
        self.assertIn("ImGuiBackendFlags_RendererHasViewports", _read(GL_CPP))

    def test_product_keeps_viewports_and_restores_the_host_gl_context(self) -> None:
        source = _read(WINMAIN)
        render_at = source.index("ImGui::Render();")
        update_at = source.index("ImGui::UpdatePlatformWindows();", render_at)
        render_platform_at = source.index("ImGui::RenderPlatformWindowsDefault();", update_at)
        restore_at = source.index("wglMakeCurrent(", render_platform_at)
        self.assertLess(render_at, update_at)
        self.assertLess(update_at, render_platform_at)
        self.assertLess(render_platform_at, restore_at)
        self.assertIn("g_mainHdc = *hdc;", source)
        self.assertIn("g_hRC = *hglrc;", source)
        self.assertIn("wglMakeCurrent(g_mainHdc ? g_mainHdc : hdc, g_hRC ? g_hRC : hglrc)", source)
        self.assertIn("ImGui::DestroyPlatformWindows();", source)
        self.assertNotIn("ImGuiConfigFlags_DockingEnable", source)
        self.assertNotIn("ArisHostDockSpace", source)
        self.assertNotIn("ImGui::DockSpace(", source)
        self.assertIn("ImGuiConfigFlags_ViewportsEnable", source)
        backend = _read(WIN32_CPP)
        self.assertTrue("WM_MOVE" in backend, "platform backend handles window movement")
        self.assertTrue("WM_SIZE" in backend, "platform backend handles resize")
        self.assertTrue("WM_DPICHANGED" in backend, "platform backend handles DPI changes")
        cmake = _read(CMAKE)
        self.assertIn("tests/test_imgui_docking_viewport_contract.py", cmake)
