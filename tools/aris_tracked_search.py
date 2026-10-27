"""Search only files tracked by the current Git worktree.

Git is used for both discovery and the explicit search entry point.  This keeps
untracked files, ignored output and nested worktree directories outside the
search boundary while still reading modified content from tracked files.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path, PurePosixPath, PureWindowsPath
from typing import Callable, Sequence


class SearchError(ValueError):
    """The search request is outside the tracked-file boundary."""


def _validate_paths(paths: Sequence[str]) -> list[str]:
    checked: list[str] = []
    for value in paths:
        path = str(value).strip().replace("\\", "/")
        posix = PurePosixPath(path)
        if (
            not path
            or path.startswith("-")
            or PureWindowsPath(path).drive
            or posix.is_absolute()
            or ".." in posix.parts
        ):
            raise SearchError(f"搜索路径必须是仓库内相对路径：{value!r}")
        checked.append(path)
    return checked


def _run_git(
    command: list[str],
    *,
    repo_root: Path,
    timeout: float,
    runner: Callable[..., subprocess.CompletedProcess[str]],
) -> subprocess.CompletedProcess[str]:
    result = runner(
        command,
        cwd=repo_root,
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        check=False,
        timeout=timeout,
    )
    return result


def list_tracked_files(
    repo_root: Path,
    *,
    paths: Sequence[str] = (),
    timeout: float = 60,
    runner: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run,
) -> list[str]:
    """Discover files through ``git ls-files`` only; never recurse from disk."""
    root = Path(repo_root).resolve()
    if not (root / ".git").exists():
        raise SearchError(f"不是 Git 工作树：{root}")
    command = ["git", "--no-pager", "ls-files", "-z", "--", *_validate_paths(paths)]
    try:
        result = _run_git(command, repo_root=root, timeout=timeout, runner=runner)
    except subprocess.TimeoutExpired as error:
        raise SearchError(f"Git 文件发现超时（{timeout:g}s）") from error
    if result.returncode != 0:
        raise SearchError(result.stderr.strip() or f"git ls-files 退出码 {result.returncode}")
    return [item for item in result.stdout.split("\0") if item]


def build_command(
    pattern: str,
    *,
    paths: Sequence[str] = (),
    literal: bool = False,
    ignore_case: bool = False,
    context: int = 0,
    extended: bool = False,
) -> list[str]:
    """Build the explicit Git search command.

    ``-e`` makes a pattern beginning with ``-`` data rather than an option;
    ``--`` makes every supplied path an operand.  ``--no-index`` is forbidden.
    """
    if not isinstance(pattern, str) or not pattern:
        raise SearchError("搜索词不能为空")
    if context < 0:
        raise SearchError("context 不能为负数")
    command = ["git", "--no-pager", "grep", "--no-color", "-n"]
    if literal:
        command.append("--fixed-strings")
    elif extended:
        command.append("--extended-regexp")
    else:
        command.append("--basic-regexp")
    if ignore_case:
        command.append("--ignore-case")
    if context:
        command.append(f"-C{context}")
    command.extend(["-e", pattern, "--", *_validate_paths(paths)])
    return command


def run_search(
    pattern: str,
    *,
    repo_root: Path,
    paths: Sequence[str] = (),
    literal: bool = False,
    ignore_case: bool = False,
    context: int = 0,
    extended: bool = False,
    timeout: float = 60,
    runner: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run,
) -> subprocess.CompletedProcess[str]:
    """Run the explicit tracked-file Git search against working-tree content."""
    root = Path(repo_root).resolve()
    command = build_command(
        pattern,
        paths=paths,
        literal=literal,
        ignore_case=ignore_case,
        context=context,
        extended=extended,
    )
    if not (root / ".git").exists():
        raise SearchError(f"不是 Git 工作树：{root}")
    try:
        result = _run_git(command, repo_root=root, timeout=timeout, runner=runner)
        if result.returncode not in (0, 1):
            raise SearchError(result.stderr.strip() or f"git grep 退出码 {result.returncode}")
        return result
    except subprocess.TimeoutExpired as error:
        raise SearchError(f"Git 搜索超时（{timeout:g}s）") from error


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="只搜索当前 Git 工作树已跟踪的文件。")
    parser.add_argument("pattern", help="搜索词；默认按基本正则处理")
    parser.add_argument("paths", nargs="*", help="仓库内相对路径，例如 src tests")
    parser.add_argument("-F", "--fixed-strings", action="store_true", dest="literal")
    parser.add_argument("-i", "--ignore-case", action="store_true")
    parser.add_argument("-C", "--context", type=int, default=0)
    parser.add_argument("-E", "--extended-regexp", action="store_true")
    parser.add_argument("--timeout", type=float, default=60)
    parser.add_argument("--list-tracked", action="store_true", help="只列出 Git 跟踪文件")
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    return parser


def main(argv: list[str] | None = None) -> int:
    try:
        args = _parser().parse_args(argv)
        if args.list_tracked:
            for path in list_tracked_files(args.repo_root, paths=args.paths, timeout=args.timeout):
                print(path)
            return 0
        result = run_search(
            args.pattern,
            repo_root=args.repo_root,
            paths=args.paths,
            literal=args.literal,
            ignore_case=args.ignore_case,
            context=args.context,
            extended=args.extended_regexp,
            timeout=args.timeout,
        )
    except SystemExit:
        print("跟踪文件搜索参数错误：未执行搜索", file=sys.stderr)
        return 64
    except (SearchError, OSError, ValueError) as error:
        print(f"跟踪文件搜索失败：{error}", file=sys.stderr)
        return 2
    if result.stdout:
        sys.stdout.write(result.stdout)
    if result.stderr:
        sys.stderr.write(result.stderr)
    return result.returncode


if __name__ == "__main__":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, OSError, ValueError):
        pass
    raise SystemExit(main())
