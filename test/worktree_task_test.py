#!/usr/bin/env python
"""Run with Python; all worktree mutations stay inside disposable repositories."""

import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
TASKS = json.loads("\n".join(
    line for line in (ROOT / ".vscode" / "tasks.json").read_text().splitlines()
    if not line.lstrip().startswith("//")
))


class WorktreeTaskTest(unittest.TestCase):
    def test_names_cross_the_task_boundary_as_arguments(self):
        for action in ("create", "remove"):
            task = next(t for t in TASKS["tasks"] if t["label"] == "worktree: " + action)
            self.assertEqual(task["type"], "process",
                             "worktreeName must never be interpolated into shell source")
            self.assertEqual(task["args"][-3:], [action, "--", "${input:worktreeName}"])
            self.assertNotIn("${input:worktreeName}", task["command"])
            self.assertNotIn("shell", task.get("windows", {}).get("options", {}))
            self.assertEqual(task["windows"]["args"][-3:],
                             [action, "--", "${input:worktreeName}"])


@unittest.skipUnless((ROOT / "scripts" / "worktree.py").exists(), "helper not implemented")
class WorktreeHelperTest(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location("worktree", ROOT / "scripts" / "worktree.py")
        self.helper = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.helper)
        self.fixture = tempfile.TemporaryDirectory(prefix="worktree fixture ", dir=ROOT)
        self.addCleanup(self.fixture.cleanup)
        self.workspace = Path(self.fixture.name)
        self.git("init", "--quiet")
        self.git("config", "core.hooksPath", str(self.workspace / "disabled-hooks"))
        self.git("-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
                 "commit", "--quiet", "--allow-empty", "-m", "fixture")
        self.real_run = subprocess.run
        self.calls = []

    def git(self, *args):
        return subprocess.run(["git", *args], cwd=self.workspace, check=True,
                              capture_output=True, text=True).stdout

    def invoke(self, action, name, failures=(), missing=()):
        def run(argv, **kwargs):
            self.calls.append(argv)
            self.assertFalse(kwargs.get("shell", False))
            if Path(argv[0]).stem == "git":
                return self.real_run(argv, **kwargs)
            result = 7 if tuple(argv[1:]) in failures else 0
            return subprocess.CompletedProcess(argv, result)

        def which(command):
            return None if command in missing else command

        stdout, stderr = io.StringIO(), io.StringIO()
        with patch.object(self.helper.shutil, "which", side_effect=which), \
                patch.object(self.helper.subprocess, "run", side_effect=run), \
                contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            result = self.helper.main([action, "--", name], workspace=self.workspace)
        return result, stdout.getvalue(), stderr.getvalue()

    def test_malicious_names_are_rejected_without_starting_a_process(self):
        for name in ('$(printf inert-marker)', '`echo inert-marker`',
                     'x" & echo inert-marker & rem "', "x;echo inert-marker",
                     "%COMSPEC%", "../outside", r"..\outside", "/absolute",
                     "", ".", "..", "-option", "a/b", "a b", "bad\nname"):
            for action in ("create", "remove"):
                with self.subTest(name=name, action=action):
                    self.calls.clear()
                    result, _, error = self.invoke(action, name)
                    self.assertNotEqual(result, 0)
                    self.assertIn("name", error.lower())
                    self.assertEqual(self.calls, [])
        self.assertEqual(sum(line.startswith("worktree ") for line in
                             self.git("worktree", "list", "--porcelain").splitlines()), 1)

    def test_create_remove_in_space_workspace_and_optional_sync(self):
        target = self.workspace / ".claude" / "worktrees" / "feature-1"
        result, _, error = self.invoke("create", "feature-1")
        self.assertEqual((result, error), (0, ""))
        self.assertTrue((target / ".git").is_file())
        self.assertEqual(self.git("-C", str(target), "branch", "--show-current").strip(),
                         "worktree-feature-1")
        self.assertIn(["tokensave", "init", str(target)], self.calls)
        self.assertIn(["tokensave", "branch", "add", "worktree-feature-1",
                       "--path", str(target)], self.calls)
        self.assertIn(["code", "--new-window", str(target)], self.calls)
        result, _, error = self.invoke("remove", "feature-1")
        self.assertEqual((result, error), (0, ""))
        self.assertFalse(target.exists())
        self.assertIn(["tokensave", "branch", "gc"], self.calls)
        self.assertIn("refs/heads/worktree-feature-1", self.git("show-ref"))

    def test_missing_tokensave_is_optional(self):
        result, output, error = self.invoke("create", "no-sync", missing=("tokensave",))
        self.assertEqual((result, error), (0, ""))
        self.assertIn("tokensave", output)
        result, _, error = self.invoke("remove", "no-sync", missing=("tokensave",))
        self.assertEqual((result, error), (0, ""))

    def test_tokensave_failure_warns_but_still_opens_and_removes(self):
        target = str(self.workspace / ".claude" / "worktrees" / "sync-fail")
        result, _, error = self.invoke("create", "sync-fail", failures=(("init", target),))
        self.assertEqual(result, 0)
        self.assertIn("tokensave", error)
        self.assertIn("7", error)
        self.assertIn(["code", "--new-window", target], self.calls)
        self.assertNotIn(["tokensave", "branch", "add", "worktree-sync-fail",
                          "--path", target], self.calls)
        result, _, error = self.invoke("remove", "sync-fail", failures=(("branch", "gc"),))
        self.assertEqual(result, 0)
        self.assertIn("tokensave", error)
        self.assertFalse(Path(target).exists())

    def test_git_failure_stops_followup_commands(self):
        result, _, error = self.invoke("remove", "missing")
        self.assertNotEqual(result, 0)
        self.assertIn("git", error)
        self.assertFalse(any(call[0] in ("tokensave", "code") for call in self.calls))

    def test_editor_failure_is_reported_without_undoing_worktree(self):
        target = self.workspace / ".claude" / "worktrees" / "editor-fail"
        result, _, error = self.invoke("create", "editor-fail",
                                       failures=(("--new-window", str(target)),))
        self.assertNotEqual(result, 0)
        self.assertIn("code", error)
        self.assertTrue(target.exists())
        self.assertEqual(self.invoke("remove", "editor-fail")[0], 0)

    def test_invalid_git_branch_name_does_not_create_worktree(self):
        result, _, error = self.invoke("create", "bad..branch")
        self.assertNotEqual(result, 0)
        self.assertIn("git", error)
        self.assertFalse((self.workspace / ".claude" / "worktrees" / "bad..branch").exists())

    def test_process_task_rejects_malicious_names_in_a_space_workspace(self):
        for action in ("create", "remove"):
            task = next(t for t in TASKS["tasks"] if t["label"] == "worktree: " + action)
            for name in ('$(printf inert-marker)', 'x" & echo inert-marker & rem "'):
                args = [arg.replace("${workspaceFolder}", str(ROOT))
                        .replace("${input:worktreeName}", name) for arg in task["args"]]
                result = self.real_run([sys.executable, *args], cwd=self.workspace,
                                       capture_output=True, text=True, shell=False)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("worktree name", result.stderr)
                self.assertNotIn("inert-marker", result.stdout)
        self.assertFalse((self.workspace / ".claude").exists())

    def test_optional_branch_registration_failure_still_opens(self):
        target = str(self.workspace / ".claude" / "worktrees" / "branch-fail")
        result, _, error = self.invoke("create", "branch-fail", failures=(
            ("branch", "add", "worktree-branch-fail", "--path", target),))
        self.assertEqual(result, 0)
        self.assertIn("tokensave", error)
        self.assertIn(["code", "--new-window", target], self.calls)
        self.assertEqual(self.invoke("remove", "branch-fail")[0], 0)

    def test_missing_editor_is_a_meaningful_failure(self):
        result, _, error = self.invoke("create", "no-editor", missing=("code",))
        self.assertNotEqual(result, 0)
        self.assertIn("code not found", error)
        self.assertEqual(self.invoke("remove", "no-editor")[0], 0)

    def test_optional_launch_error_warns(self):
        error = io.StringIO()
        with patch.object(self.helper.shutil, "which", return_value="tokensave"), \
                patch.object(self.helper.subprocess, "run", side_effect=OSError("fixture error")), \
                contextlib.redirect_stderr(error):
            self.assertFalse(self.helper.run(["tokensave", "branch", "gc"],
                                             self.workspace, optional=True))
        self.assertIn("tokensave failed", error.getvalue())

    def test_windows_editor_shim_uses_native_cli_not_cmd(self):
        for version in ("", "07f806f999"):
            with self.subTest(version=version):
                install = self.workspace / ("VS Code " + (version or "legacy"))
                relative_cli = ((version + "\\") if version else "") + \
                    "resources\\app\\out\\cli.js"
                cli = install.joinpath(*relative_cli.split("\\"))
                cli.parent.mkdir(parents=True)
                cli.touch()
                stale_cli = install / "deadbeef" / "resources" / "app" / "out" / "cli.js"
                stale_cli.parent.mkdir(parents=True)
                stale_cli.touch()
                (install / "Code.exe").touch()
                shim = install / "bin" / "code.cmd"
                shim.parent.mkdir()
                shim.write_text('@echo off\nsetlocal\nset VSCODE_DEV=\n'
                                'set ELECTRON_RUN_AS_NODE=1\n'
                                '"%~dp0..\\Code.exe" "%~dp0..\\' + relative_cli + '" %*\n'
                                'endlocal\n')
                with patch.object(self.helper.shutil, "which", return_value=str(shim)), \
                        patch.dict(os.environ, {"VSCODE_DEV": "fixture",
                                                "ELECTRON_RUN_AS_NODE": "0"}), \
                        patch.object(self.helper.subprocess, "run",
                                     return_value=subprocess.CompletedProcess([], 0)) as run:
                    self.assertTrue(self.helper.run(
                        ["code", "--new-window", str(self.workspace)], self.workspace))
                self.assertEqual(run.call_args.args[0],
                                 [str(install / "Code.exe"), str(cli),
                                  "--new-window", str(self.workspace)])
                self.assertFalse(run.call_args.kwargs["shell"])
                self.assertEqual(run.call_args.kwargs["env"]["ELECTRON_RUN_AS_NODE"], "1")
                self.assertFalse("VSCODE_DEV" in run.call_args.kwargs["env"])

    def test_windows_editor_unrecognized_shim_is_never_evaluated(self):
        install = self.workspace / "VS Code"
        shim = install / "bin" / "code.cmd"
        shim.parent.mkdir(parents=True)
        for line in (
                '"%~dp0..\\Code.exe" "%~dp0..\\resources\\app\\out\\cli.js" %* & echo inert-marker',
                '"%~dp0..\\Code.exe" "%~dp0..\\..\\resources\\app\\out\\cli.js" %*'):
            with self.subTest(line=line):
                shim.write_text(line)
                with patch.object(self.helper.shutil, "which", return_value=str(shim)), \
                        patch.object(self.helper.subprocess, "run") as run:
                    with self.assertRaisesRegex(RuntimeError, "code"):
                        self.helper.run(["code", "--new-window", str(self.workspace)],
                                        self.workspace)
                run.assert_not_called()

    def test_optional_batch_launcher_is_not_executed(self):
        error = io.StringIO()
        with patch.object(self.helper.shutil, "which", return_value="tokensave.cmd"), \
                patch.object(self.helper.subprocess, "run") as run, \
                contextlib.redirect_stderr(error):
            self.assertFalse(self.helper.run(["tokensave", "init", str(self.workspace)],
                                             self.workspace, optional=True))
        run.assert_not_called()
        self.assertIn("native executable", error.getvalue())


if __name__ == "__main__":
    unittest.main()
