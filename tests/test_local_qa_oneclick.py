import os
import subprocess
from pathlib import Path


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "scripts" / "local_qa_oneclick.ps1"
CLICKABLE = ROOT / "scripts" / "local_qa_oneclick.cmd"


def run(*args, cwd=ROOT):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True)


def checkout(tmp_path, branch):
    repo = tmp_path / "qa"
    repo.mkdir()
    assert run("git", "init", "-b", branch, str(repo)).returncode == 0
    remote = run("git", "remote", "get-url", "origin").stdout.strip()
    assert run("git", "remote", "add", "origin", remote, cwd=repo).returncode == 0
    return repo


def invoke(repo):
    return run("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(SCRIPT),
               "-WorkRepo", str(repo), "-NoOpen")


def test_refuses_other_branch_without_changing_files(tmp_path):
    repo = checkout(tmp_path, "main")
    marker = repo / "keep.txt"
    marker.write_text("keep", encoding="utf-8")
    result = invoke(repo)
    assert result.returncode != 0
    assert "must be on qa/current" in result.stderr
    assert marker.read_text(encoding="utf-8") == "keep"


def test_refuses_dirty_qa_checkout_without_changing_files(tmp_path):
    repo = checkout(tmp_path, "qa/current")
    marker = repo / "keep.txt"
    marker.write_text("keep", encoding="utf-8")
    result = invoke(repo)
    assert result.returncode != 0
    assert "local changes" in result.stderr
    assert marker.read_text(encoding="utf-8") == "keep"


def test_packaging_precedes_opening():
    source = SCRIPT.read_text(encoding="utf-8")
    assert source.index("package_qa_exe.ps1") < source.index("Start-Process")
    assert "if ($LASTEXITCODE -ne 0) { throw \"QA packaging failed" in source


def test_launcher_isolates_qa_state_and_checks_process_stays_alive():
    source = SCRIPT.read_text(encoding="utf-8")
    for name in ("LOCALAPPDATA", "APPDATA", "AURA_RUNTIME_ROOT", "TEMP", "TMP"):
        assert f"$env:{name}" in source
    assert "Start-Process -FilePath $exe" in source
    assert "-PassThru" in source
    assert "$started.HasExited" in source


def test_clickable_entry_runs_the_oneclick_script():
    source = CLICKABLE.read_text(encoding="utf-8")
    assert '"%~dp0local_qa_oneclick.ps1"' in source
    assert "if errorlevel 1 pause" in source


def test_oneclick_package_uses_its_own_compiled_preview_root(tmp_path):
    source = tmp_path / "source"
    source.mkdir()
    assert run("git", "init", "-b", "qa/current", str(source)).returncode == 0
    assert run("git", "config", "user.email", "qa-test@example.invalid", cwd=source).returncode == 0
    assert run("git", "config", "user.name", "QA Test", cwd=source).returncode == 0
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(qa_fixture NONE)\n"
        "option(ARIS_PREVIEW_ISOLATION \"\" OFF)\n"
        "set(ARIS_PREVIEW_ISOLATION_ROOT \"\" CACHE PATH \"\")\n",
        encoding="utf-8",
    )
    scripts = source / "scripts"
    scripts.mkdir()
    (scripts / SCRIPT.name).write_bytes(SCRIPT.read_bytes())
    (scripts / "package_qa_exe.ps1").write_text(
        "param([string]$BuildDir,[string]$DistDir,[string]$FeedbackMode,"
        "[string]$PackageKeyId,[string]$PackagePublicKeyHex)\n"
        "$target = Join-Path (Join-Path $DistDir 'Airs') 'HarkenSearch.exe'\n"
        "New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null\n"
        "[IO.File]::WriteAllText($target, 'fake-exe')\n",
        encoding="utf-8",
    )
    assert run("git", "add", "CMakeLists.txt", "scripts", cwd=source).returncode == 0
    assert run("git", "commit", "-m", "fixture", cwd=source).returncode == 0
    remote = tmp_path / "remote.git"
    assert run("git", "clone", "--bare", str(source), str(remote)).returncode == 0
    assert run("git", "remote", "add", "origin", str(remote), cwd=source).returncode == 0
    work = tmp_path / "work"
    assert run("git", "clone", "--branch", "qa/current", str(remote), str(work)).returncode == 0
    dist = tmp_path / "output"
    env = os.environ.copy()
    env["STABLE_UPDATE_PACKAGE_KEY_ID"] = "test-key"
    env["ARIS_BOOTSTRAP_PACKAGE_PUBLIC_KEY_HEX"] = "a" * 64
    result = subprocess.run(
        ("powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(scripts / SCRIPT.name),
         "-WorkRepo", str(work), "-DistDir", str(dist), "-NoOpen"),
        cwd=source, env=env, capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stderr
    cache = (work / "build_qa_oneclick_isolated" / "CMakeCache.txt").read_text(encoding="utf-8")
    assert "ARIS_PREVIEW_ISOLATION:BOOL=ON" in cache
    assert ("ARIS_PREVIEW_ISOLATION_ROOT:PATH=" + str(dist / "data").replace("\\", "/")) in cache
    assert (dist / "Airs" / "HarkenSearch.exe").read_text(encoding="utf-8") == "fake-exe"
