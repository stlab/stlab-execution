#!/usr/bin/env python
"""VS Code worktree tasks: pass names as data, never as shell source."""

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys
import os


def run(command, workspace, optional=False):
    executable = shutil.which(command[0])
    if executable is None:
        if optional:
            print("tokensave not installed; skipping optional worktree synchronization.")
            return False
        raise RuntimeError(f"{command[0]} not found on PATH")

    argv = [executable, *command[1:]]
    env = None
    if Path(executable).suffix.lower() in (".cmd", ".bat"):
        if command[0] == "code":
            # Read only the native launcher line as data; never evaluate the shim.
            shim = Path(executable).resolve()
            install = shim.parent.parent
            try:
                contents = shim.read_text(encoding="utf-8-sig")
            except (OSError, UnicodeError) as error:
                raise RuntimeError(f"code launcher could not be read: {error}") from error
            paths = re.findall(
                r'^[ \t]*"%~dp0\.\.[\\/]Code\.exe"[ \t]+'
                r'"%~dp0\.\.[\\/]((?:[0-9a-f]+[\\/])?resources[\\/]app[\\/]out[\\/]cli\.js)"'
                r'[ \t]+%\*[ \t]*$',
                contents, re.MULTILINE | re.IGNORECASE)
            if len(paths) != 1:
                raise RuntimeError("code launcher has no unambiguous supported native CLI path")
            cli = install.joinpath(*re.split(r"[\\/]", paths[0])).resolve()
            electron = install / "Code.exe"
            if not cli.is_relative_to(install) or not cli.is_file() or not electron.is_file():
                raise RuntimeError("code native executable or CLI file is missing or outside its installation")
            argv = [str(electron), str(cli), *command[1:]]
            env = dict(os.environ, ELECTRON_RUN_AS_NODE="1")
            env.pop("VSCODE_DEV", None)
        else:
            message = f"{command[0]} uses a shell launcher; a native executable is required"
            if optional:
                print(f"Warning: {message}; skipping optional synchronization.", file=sys.stderr)
                return False
            raise RuntimeError(message)

    try:
        result = subprocess.run(argv, cwd=workspace, env=env, shell=False)
    except OSError as error:
        if optional:
            print(f"Warning: {command[0]} failed: {error}", file=sys.stderr)
            return False
        raise RuntimeError(f"{command[0]} failed: {error}") from error
    if result.returncode:
        message = f"{command[0]} {' '.join(command[1:2])} failed (exit {result.returncode})"
        if optional:
            print(f"Warning: {message}; continuing without synchronization.", file=sys.stderr)
            return False
        raise RuntimeError(message)
    return True


def main(argv=None, workspace=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("create", "remove"))
    parser.add_argument("name", help="Single directory name: letters, digits, dots, underscores, hyphens")
    args = parser.parse_args(argv)
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", args.name):
        print("Error: worktree name must start with a letter or digit and contain only "
              "letters, digits, dots, underscores, or hyphens.", file=sys.stderr)
        return 1

    workspace = Path(workspace or Path.cwd()).resolve()
    target = workspace / ".claude" / "worktrees" / args.name
    if not target.resolve().is_relative_to(workspace):
        print("Error: worktree name resolves outside the workspace.", file=sys.stderr)
        return 1
    branch = "worktree-" + args.name
    try:
        run(["git", "check-ref-format", "--branch", branch], workspace)
        if args.action == "create":
            run(["git", "worktree", "add", str(target), "-b", branch], workspace)
            if run(["tokensave", "init", str(target)], workspace, optional=True):
                run(["tokensave", "branch", "add", branch, "--path", str(target)],
                    workspace, optional=True)
            run(["code", "--new-window", str(target)], workspace)
        else:
            run(["git", "worktree", "remove", str(target)], workspace)
            run(["tokensave", "branch", "gc"], workspace, optional=True)
    except RuntimeError as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
