"""Offline Git-object source gate tests; no external repository is contacted."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from git_publication_gate import scan_staged, scan_pre_push, outgoing_commits


class GitObjectPublicationTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="synthetic-git-gate-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.git("init", "--quiet")
        self.git("config", "user.name", "Test Fixture")
        self.git("config", "user.email", "local-test@example.invalid")
        self.make("README.md", "Fixture: permitted public content")
        self.git("add", "README.md")
        self.git("commit", "--quiet", "-m", "fixture")
        self.base = self.git("rev-parse", "HEAD").strip()
        self.assertEqual(scan_staged(self.root)["decision"], "go")

    def git(self, *args):
        executable = os.environ.get("GIT_EXECUTABLE", "git")
        out = subprocess.run(
            [executable, "-C", str(self.root), *args],
            check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20,
        )
        return out.stdout.decode("utf-8").strip()

    def make(self, name, text):
        file = self.root / name
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(text, encoding="utf-8")

    def push_line(self, local=None, remote=None):
        return f"refs/heads/main {local or self.git('rev-parse', 'HEAD')} refs/heads/main {remote or self.base}\n"

    def test_staged_clean_index(self):
        self.make("file.txt", "public text")
        self.git("add", "file.txt")
        report = scan_staged(self.root)
        self.assertEqual(report["decision"], "go")
        self.assertEqual(report["checked_files"], 2)
        self.assertFalse(report["worktree_content_used"])

    def test_staged_bytes_not_working_tree_bytes(self):
        self.make("safe.txt", ("-----BEGIN " + "PRIVATE KEY-----\nFICTIONAL_TEST"))
        self.git("add", "safe.txt")
        self.make("safe.txt", "Now harmless on disk")
        report = scan_staged(self.root)
        self.assertEqual(report["decision"], "no_go")
        self.assertIn("private_key", report["risk_classes"])
        self.assertNotIn("safe.txt", str(report))
        self.assertNotIn("PRIVATE KEY", str(report))

    def test_unstaged_private_content_not_yet_in_commit(self):
        self.make("safe.txt", "Public staged")
        self.git("add", "safe.txt")
        self.make("safe.txt", ("-----BEGIN " + "PRIVATE KEY-----"))
        self.assertEqual(scan_staged(self.root)["decision"], "go")

    def test_staged_private_archive_rejected_without_contents(self):
        self.make("reference.pdf", "Not a real PDF, merely a filename fixture")
        self.git("add", "reference.pdf")
        report = scan_staged(self.root)
        self.assertEqual(report["decision"], "no_go")
        self.assertIn("proprietary_or_sensitive_file_type", report["risk_classes"])

    def test_pre_push_clean_change(self):
        self.make("safe.txt", "public data")
        self.git("add", "safe.txt")
        self.git("commit", "--quiet", "-m", "safe")
        report = scan_pre_push(self.root, self.push_line())
        self.assertEqual(report["decision"], "go")
        self.assertEqual(report["checked_commits"], 1)

    def test_pre_push_detects_deleted_private_blob_in_intermediate_commit(self):
        self.make("private.pdf", "synthetic fixture, no actual source")
        self.git("add", "private.pdf")
        self.git("commit", "--quiet", "-m", "bad fixture")
        self.git("rm", "--quiet", "private.pdf")
        self.git("commit", "--quiet", "-m", "delete fixture")
        self.assertEqual(scan_staged(self.root)["decision"], "go")
        report = scan_pre_push(self.root, self.push_line())
        self.assertEqual(report["decision"], "no_go")
        self.assertEqual(report["checked_commits"], 2)
        self.assertIn("proprietary_or_sensitive_file_type", report["risk_classes"])
        self.assertNotIn("private.pdf", str(report))

    def test_new_branch_scans_full_history(self):
        self.make("customer-data/synthetic.txt", "offline fixture")
        self.git("add", "customer-data/synthetic.txt")
        self.git("commit", "--quiet", "-m", "bad fixture")
        self.git("rm", "--quiet", "customer-data/synthetic.txt")
        self.git("commit", "--quiet", "-m", "remove fixture")
        zero = "0" * len(self.base)
        result = scan_pre_push(self.root, self.push_line(remote=zero))
        self.assertEqual(result["decision"], "no_go")
        self.assertIn("private_path", result["risk_classes"])

    def test_no_pending_objects_yields_go(self):
        report = scan_pre_push(self.root, self.push_line(local=self.base))
        self.assertEqual(report["decision"], "go")
        self.assertEqual(report["checked_commits"], 0)

    def test_deletion_transmits_no_new_commits(self):
        zero = "0" * len(self.base)
        report = scan_pre_push(self.root, self.push_line(local=zero))
        self.assertEqual(report["decision"], "go")
        self.assertEqual(report["checked_commits"], 0)

    def test_pre_push_malformed_ref_rejected(self):
        for text in ("", "incomplete\n", "refs/heads/main not-a-sha refs/heads/main 123\n"):
            with self.subTest(text=text):
                with self.assertRaises(ValueError):
                    outgoing_commits(self.root, text)

    def test_rejects_staged_symlink_or_gitlink(self):
        # Use ls-files --stage to detect a symbolic link object without touching it.
        self.make("link_target", "ordinary")
        self.git("add", "link_target")
        if os.name != "nt":
            (self.root / "external_link").symlink_to("link_target")
            self.git("add", "external_link")
            report = scan_staged(self.root)
            self.assertIn("symlink_requires_manual_review", report["risk_classes"])
        else:
            self.skipTest("Windows symlink support depends on host privileges")


if __name__ == "__main__":
    unittest.main()
