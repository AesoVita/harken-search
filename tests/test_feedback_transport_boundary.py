from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WINDOWS_MAIN = (ROOT / "src" / "WindowsMain.cpp").read_text(encoding="utf-8")
WINDOWS_HEADER = (ROOT / "src" / "WindowsMain.h").read_text(encoding="utf-8")
TRANSPORT_HEADER = ROOT / "src" / "FeedbackTransport.h"
TRANSPORT_SOURCE = ROOT / "src" / "FeedbackTransport.cpp"
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")


def test_legacy_trello_transport_files_and_build_wiring_are_removed():
    assert not TRANSPORT_HEADER.exists()
    assert not TRANSPORT_SOURCE.exists()
    assert "FeedbackTransport.h" not in WINDOWS_HEADER
    assert "src/FeedbackTransport.cpp" not in CMAKE


def test_windows_main_no_longer_owns_trello_transport_definition():
    assert "bool PostTrelloCard(const std::string& name" not in WINDOWS_MAIN
    assert "OpenPreconfiguredInternetSession(L\"ArisFeedback/1.0\")" not in WINDOWS_MAIN
    assert "SendHttpRequest(" not in WINDOWS_MAIN


def test_orchestration_preserves_nonblocking_poll_contract():
    main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
    completion = (
        ROOT / "src" / "CueviFeedbackCompletionWorkflow.cpp"
    ).read_text(encoding="utf-8")
    completion_contract = (
        ROOT / "src" / "CueviFeedbackCompletionWorkflow.h"
    ).read_text(encoding="utf-8")
    assert main.count("CueviFeedback::AsyncSubmission::Start") == 1
    assert "F-035: Crash Recovery Dialog is parked" in main
    assert "aura::feedback::PollCueviFeedbackCompletion(" in main
    # Explicit non-blocking Poll contract: this guards the existing workflow
    # behavior and is not a Trello transport behavior change.
    assert "Apply/Poll call. Poll never" in completion_contract
    assert "blocks; it only consumes" in completion_contract
    assert "task.Poll()" in completion
    assert "task.Take()" not in completion
    assert "PostTrelloCard(" not in main
    assert "state.submitting = false" in completion
