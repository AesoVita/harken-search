from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_authenticated_bootstrap_successor_is_not_short_circuited_by_single_instance():
    source = (ROOT / "src" / "WindowsMain.cpp").read_text(encoding="utf-8")

    capture = "const bool bootstrapHealthSuccessor =\n        aris::update::CaptureHarkenSearchBootstrapHealthChallenge();"
    assert capture in source

    single_instance_gate = (
        "if (ShouldUseSingleInstanceBroker() && !bootstrapHealthSuccessor) {"
    )
    assert single_instance_gate in source


def test_terminal_successor_does_not_request_unavailable_job_breakaway():
    source = (
        ROOT / "src" / "bootstrap" / "WindowsBootstrapRuntimeWin32.cpp"
    ).read_text(encoding="utf-8")

    assert "TerminalCleanupCreateFlags" in source
    assert "JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE" in source
    assert "CREATE_BREAKAWAY_FROM_JOB" in source
    assert "const DWORD successorCreateFlags = TerminalCleanupCreateFlags();" in source
    assert "if (successorCreateFlags == 0U)" in source


def test_local_demo_keeps_terminal_successor_stages_separate_from_parent_stage():
    source = (
        ROOT / "src" / "bootstrap" / "HarkenSearchBootstrapMain.cpp"
    ).read_text(encoding="utf-8")

    assert "bootstrap-demo-terminal-stage.txt" in source
    assert "writeLocalDemoTerminalStageDiagnostic" in source
    assert "terminal-channel" in source
    assert "terminal-ready" in source


def test_local_demo_records_terminal_successor_exit_before_ready_failure():
    source = (
        ROOT / "src" / "bootstrap" / "WindowsBootstrapRuntimeWin32.cpp"
    ).read_text(encoding="utf-8")

    assert "bootstrap-demo-successor-exit.txt" in source
    assert "GetExitCodeProcess" in source
    assert "writeLocalDemoSuccessorExitDiagnostic" in source


def test_local_demo_exposes_terminal_invocation_decode_reason():
    source = (
        ROOT / "src" / "bootstrap" / "HarkenSearchBootstrapMain.cpp"
    ).read_text(encoding="utf-8")

    assert "invocation-decode-status" in source
    assert "decoded.status" in source
    assert "decoded.diagnostic" in source


def test_invocation_decoder_keeps_invalid_identity_reason_localizable():
    source = (
        ROOT / "src" / "bootstrap" / "HarkenSearchBootstrapAdmission.cpp"
    ).read_text(encoding="utf-8")

    assert "old-process-id-mismatch" in source
    assert "parent-process-parse" in source


def test_terminal_cleanup_identity_serializers_emit_json_objects():
    source = (
        ROOT / "src" / "bootstrap" / "WindowsBootstrapRuntimeWin32.cpp"
    ).read_text(encoding="utf-8")

    process_begin = source.index("json terminalProcessIdentityJson")
    deployment_begin = source.index("json terminalDeploymentIdentityJson")
    handoff_begin = source.index("std::string encodeTerminalCleanupHandoff")
    process_helper = source[process_begin:deployment_begin]
    deployment_helper = source[deployment_begin:handoff_begin]

    assert 'return {{"creation_time", identity.creationTime},' in process_helper
    assert '{"process_id", identity.processId}};' in process_helper
    assert 'return {{"canonical_path", identity.canonicalPath},' in deployment_helper
    assert '{"volume_serial", identity.volumeSerial}};' in deployment_helper


def test_terminal_channel_parent_basename_aligns_with_runtime_pair_bootstrap_name():
    source = (
        ROOT / "src" / "bootstrap" / "WindowsBootstrapRuntimeWin32.cpp"
    ).read_text(encoding="utf-8")

    channel_begin = source.index(
        "BootstrapTerminalChannelResult ReadAuthenticatedBootstrapTerminalChannel"
    )
    channel_end = source.index(
        "BootstrapTerminalReadyResult SignalBootstrapTerminalReady", channel_begin
    )
    channel_body = source[channel_begin:channel_end]

    assert '!= "harkensearchbootstrap.exe"' in channel_body
    assert '!= "arisbootstrap.exe"' not in channel_body
