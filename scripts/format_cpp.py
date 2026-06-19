#!/usr/bin/env python3
"""Format tracked C and C++ files with the repository clang-format style."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Iterable


CPP_EXTENSIONS = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="check formatting without modifying files",
    )
    parser.add_argument(
        "--clang-format",
        help="clang-format executable to use; defaults to CLANG_FORMAT, then PATH",
    )
    parser.add_argument(
        "files",
        nargs="*",
        help="optional files or directories to format; defaults to tracked C/C++ files",
    )
    return parser.parse_args()


def repo_root() -> Path:
    result = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    if result.returncode == 0:
        return Path(result.stdout.strip()).resolve()
    return Path.cwd().resolve()


def is_cpp_file(path: Path) -> bool:
    return path.suffix.lower() in CPP_EXTENSIONS


def sorted_unique(paths: Iterable[Path]) -> list[Path]:
    return sorted(dict.fromkeys(paths), key=lambda path: path.as_posix())


def tracked_cpp_files(root: Path) -> list[Path] | None:
    patterns = [f"*{extension}" for extension in sorted(CPP_EXTENSIONS)]
    try:
        result = subprocess.run(
            ["git", "ls-files", "-z", "--", *patterns],
            cwd=root,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            check=False,
        )
    except FileNotFoundError:
        return None

    if result.returncode != 0:
        return None

    return sorted_unique(
        Path(name.decode()) for name in result.stdout.split(b"\0") if name
    )


def scanned_cpp_files(root: Path) -> list[Path]:
    files: list[Path] = []
    for directory_name in ("src", "include"):
        directory = root / directory_name
        if not directory.is_dir():
            continue
        files.extend(
            path.relative_to(root)
            for path in directory.rglob("*")
            if path.is_file() and is_cpp_file(path)
        )
    return sorted_unique(files)


def explicit_cpp_files(root: Path, names: list[str]) -> list[Path]:
    files: list[Path] = []
    missing: list[str] = []

    for name in names:
        path = Path(name)
        absolute_path = path if path.is_absolute() else root / path

        if absolute_path.is_dir():
            files.extend(
                candidate.relative_to(root)
                for candidate in absolute_path.rglob("*")
                if candidate.is_file() and is_cpp_file(candidate)
            )
        elif absolute_path.is_file():
            if is_cpp_file(absolute_path):
                files.append(absolute_path.relative_to(root))
        else:
            missing.append(name)

    if missing:
        raise FileNotFoundError("missing paths: " + ", ".join(missing))

    return sorted_unique(files)


def resolve_clang_format(explicit_tool: str | None) -> str:
    candidates = [explicit_tool, os.environ.get("CLANG_FORMAT"), "clang-format"]

    for candidate in candidates:
        if not candidate:
            continue

        if os.path.sep in candidate:
            path = Path(candidate)
            if path.exists():
                return str(path)
            continue

        resolved = shutil.which(candidate)
        if resolved:
            return resolved

    raise FileNotFoundError(
        "clang-format was not found; set CLANG_FORMAT or pass --clang-format"
    )


def run_clang_format(clang_format: str, files: list[Path], check_only: bool) -> int:
    if not files:
        print("No C/C++ files to format.")
        return 0

    command = [clang_format]
    if check_only:
        command.extend(["--dry-run", "--Werror"])
    else:
        command.append("-i")
    command.append("--style=file")
    command.extend(path.as_posix() for path in files)

    result = subprocess.run(command, check=False)
    return result.returncode


def main() -> int:
    args = parse_args()
    root = repo_root()

    try:
        clang_format = resolve_clang_format(args.clang_format)
        files = explicit_cpp_files(root, args.files) if args.files else tracked_cpp_files(root)
        if files is None:
            files = scanned_cpp_files(root)
        return run_clang_format(clang_format, files, args.check)
    except FileNotFoundError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
