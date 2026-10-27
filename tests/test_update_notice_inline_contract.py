"""Entry-point regressions for the user-selected inline announcement."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def test_release_notes_do_not_open_a_second_modal_over_the_update_center():
    source = (ROOT / 'src/main.cpp').read_text(encoding='utf-8')
    assert 'BeginTransientScrollbarPopupModal(kReleaseNotesPopupId' not in source
    assert 'ImGui::OpenPopup(kReleaseNotesPopupId)' not in source

def test_full_notice_is_visible_without_expand_button():
    source = (ROOT / 'src/update/HarkenSearchUpdateNoticeUi.cpp').read_text(encoding='utf-8')
    assert 'view-full-update-content' not in source
    assert 'RenderReleaseContents' in source

def test_ready_notice_uses_the_same_update_center():
    source = (ROOT / 'src/main.cpp').read_text(encoding='utf-8')
    assert 'BeginTransientScrollbarPopupModal(kUpdateNoticePopupId' not in source
    assert 'readyChanged' in source
