import json
import subprocess
from pathlib import Path


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "scripts" / "local_current_qa_oneclick.ps1"
CLICKABLE = ROOT / "scripts" / "local_current_qa_oneclick.cmd"


def run(*args, cwd=ROOT):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True)


def fixture(tmp_path, branch="qa/current", change_during_build=False,
            disable_isolation_during_build=False):
    repo = tmp_path / "repo"
    repo.mkdir()
    assert run("git", "init", "-b", branch, str(repo)).returncode == 0
    assert run("git", "config", "user.email", "qa-test@example.invalid", cwd=repo).returncode == 0
    assert run("git", "config", "user.name", "QA Test", cwd=repo).returncode == 0
    (repo / "src.txt").write_text("committed", encoding="utf-8")
    (repo / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(qa_fixture NONE)\n"
        "option(ARIS_PREVIEW_ISOLATION \"\" OFF)\n"
        "set(ARIS_PREVIEW_ISOLATION_ROOT \"\" CACHE PATH \"\")\n",
        encoding="utf-8",
    )
    scripts = repo / "scripts"
    scripts.mkdir()
    packager = scripts / "package_qa_exe.ps1"
    packager.write_text(
        "param([string]$BuildDir,[string]$DistDir,[string]$FeedbackMode,"
        "[string]$PackageKeyId,[string]$PackagePublicKeyHex)\n"
        "$target = Join-Path (Join-Path $DistDir 'Airs') 'HarkenSearch.exe'\n"
        "New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null\n"
        "[IO.File]::WriteAllText($target, 'fake-exe')\n"
        + ("[IO.File]::WriteAllText((Join-Path (Split-Path $PSScriptRoot -Parent) 'src.txt'), 'changed-during-build')\n"
           if change_during_build else ""),
        encoding="utf-8",
    )
    if disable_isolation_during_build:
        packager.write_text(
            packager.read_text(encoding="utf-8")
            + "$cache = Join-Path $BuildDir 'CMakeCache.txt'\n"
            + "(Get-Content -LiteralPath $cache) -replace 'ARIS_PREVIEW_ISOLATION:BOOL=ON', 'ARIS_PREVIEW_ISOLATION:BOOL=OFF' | Set-Content -LiteralPath $cache\n",
        encoding="utf-8",
        )
    assert run("git", "add", "src.txt", "CMakeLists.txt", "scripts/package_qa_exe.ps1", cwd=repo).returncode == 0
    assert run("git", "commit", "-m", "fixture", cwd=repo).returncode == 0
    (repo / "src.txt").write_text("local uncommitted edit", encoding="utf-8")
    return repo


def invoke(repo, dist):
    return run(
        "powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(SCRIPT),
        "-RepoPath", str(repo), "-DistDir", str(dist), "-PackageKeyId", "test-key",
        "-PackagePublicKeyHex", "a" * 64, "-NoOpen",
    )


def test_packages_current_dirty_qa_without_changing_it(tmp_path):
    repo = fixture(tmp_path)
    dist = tmp_path / "output"
    before = run("git", "rev-parse", "HEAD", cwd=repo).stdout.strip()
    result = invoke(repo, dist)
    assert result.returncode == 0, result.stderr
    assert (dist / "Airs" / "HarkenSearch.exe").is_file()
    cache = (repo / "build_qa_oneclick_isolated" / "CMakeCache.txt").read_text(encoding="utf-8")
    assert "ARIS_PREVIEW_ISOLATION:BOOL=ON" in cache
    assert ("ARIS_PREVIEW_ISOLATION_ROOT:PATH=" + str(dist / "data").replace("\\", "/")) in cache
    manifest = json.loads((dist / "local-qa-source.json").read_text(encoding="utf-8"))
    assert manifest["sourceCommit"] == before
    assert manifest["hasUncommittedChanges"] is True
    assert repo.joinpath("src.txt").read_text(encoding="utf-8") == "local uncommitted edit"
    assert run("git", "rev-parse", "HEAD", cwd=repo).stdout.strip() == before


def test_defaults_to_checkout_containing_the_script(tmp_path):
    repo = fixture(tmp_path)
    (repo / "scripts" / SCRIPT.name).write_bytes(SCRIPT.read_bytes())
    dist = tmp_path / "output"
    result = run(
        "powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
        str(repo / "scripts" / SCRIPT.name), "-DistDir", str(dist),
        "-PackageKeyId", "test-key", "-PackagePublicKeyHex", "a" * 64, "-NoOpen",
    )
    assert result.returncode == 0, result.stderr
    assert (dist / "Airs" / "HarkenSearch.exe").is_file()


def test_refuses_wrong_branch_before_packaging(tmp_path):
    repo = fixture(tmp_path, branch="main")
    dist = tmp_path / "output"
    result = invoke(repo, dist)
    assert result.returncode != 0
    assert not (dist / "Airs" / "HarkenSearch.exe").exists()


def test_refuses_fixed_remote_qa_output(tmp_path):
    repo = fixture(tmp_path)
    dist = repo / "dist_qa"
    result = invoke(repo, dist)
    assert result.returncode != 0
    assert not (dist / "Airs" / "HarkenSearch.exe").exists()


def test_source_change_during_build_prevents_success(tmp_path):
    repo = fixture(tmp_path, change_during_build=True)
    result = invoke(repo, tmp_path / "output")
    assert result.returncode != 0
    assert "changed during packaging" in result.stderr


def test_packager_cannot_disable_preview_isolation(tmp_path):
    repo = fixture(tmp_path, disable_isolation_during_build=True)
    result = invoke(repo, tmp_path / "output")
    assert result.returncode != 0
    assert "Preview isolation is not enabled" in result.stderr


def test_double_click_entry_invokes_script():
    assert '"%~dp0local_current_qa_oneclick.ps1"' in CLICKABLE.read_text(encoding="utf-8")


def test_opening_requires_a_visible_window():
    source = SCRIPT.read_text(encoding="utf-8")
    assert "MainWindowHandle" in source
    assert "did not show a window" in source
