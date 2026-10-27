import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class DockFloatHandoffContract(unittest.TestCase):
    def test_hold_captures_pointer_offset_and_moves_immediately(self):
        source = (ROOT / 'src/main.cpp').read_text(encoding='utf-8')
        # Runtime policy is exercised by DockLayoutTree/DockDragSession. Keep
        # this test limited to the actual application's integration boundary.
        begin = source.split('auto beginDockedPanelDrag =', 1)[1].split('auto beginSharedDockWindowHost =', 1)[0]
        self.assertTrue('ImGui::IsItemActivated()' in begin, 'press must originate on this item')
        activation = begin.split('if (ImGui::IsItemActivated()', 1)[1].split('pressDockTitle(', 1)[0]
        self.assertNotIn('IsItemHovered()', activation, 'hover feedback must not gate an already activated press')
        self.assertLess(begin.index('g_dockDrag.BeginDrag('), begin.index('FloatDockedPanel('))
        driver = source.split('auto driveFloatingDockDrag =', 1)[1].split('auto drawFloatingDockTestPanel =', 1)[0]
        self.assertTrue('mouse.x - (g_dockTitleGesture.StartX() - g_dockGestureRectBefore.x)' in driver)
        self.assertTrue('mouse.y - (g_dockTitleGesture.StartY() - g_dockGestureRectBefore.y)' in driver)
        self.assertNotIn('GetCapture()', driver)
        self.assertNotIn('GetAsyncKeyState', driver)
        self.assertTrue('dockDragLabel.Contains(ImGui::GetMousePos())' in driver)

    def test_handoff_none_release_stays_floating(self):
        source = (ROOT / 'src/main.cpp').read_text(encoding='utf-8')
        driver = source.split('auto driveFloatingDockDrag =', 1)[1].split('auto drawFloatingDockTestPanel =', 1)[0]
        self.assertTrue('g_dockTitleGesture.Owns(moduleId)' in driver)
        self.assertTrue('g_dockDrag.OwnsDrag(moduleId)' in driver)
        self.assertTrue('g_dockTitleGesture.DropArmed()' in driver)
        self.assertTrue('HasMovedFromDockOrigin' in driver)
        self.assertLess(driver.index('zone = detectDockZone('), driver.index('g_dockDrag.UpdatePreview(zone)'))
        self.assertLess(driver.index('g_dockDrag.UpdatePreview(zone)'), driver.index('if (!step.released) return;'))
        self.assertNotIn('PreferCommittedDockHit', driver)
        self.assertTrue('commitHit.kind == aura::ui::dock::DockHitKind::None' in driver)
        self.assertTrue('g_dockDrag.CommitFloatingOnMouseUp();' in driver)
        self.assertTrue('panel.floating = true;' in driver)

if __name__ == '__main__':
    unittest.main()
