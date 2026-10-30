"""Inspect Git's staged blobs and commits-to-push before publishing.

Unlike working-tree scans, these checks read the exact immutable Git objects
that will be committed or transmitted. No file contents or paths are logged.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

from publication_guard import MAX_TEXT_BYTES, inspect_entries

HEX = re.compile(r"(?:[0-9a-fA-F]{40}|[0-9a-fA-F]{64})\Z")
ZERO = re.compile(r"0{40}(?:0{24})?\Z")
MAX_COMMITS = 5000
MAX_ENTRIES = 100000


def git(root: Path, *args: str, input_data: bytes | None = None) -> bytes:
    cmd = [os.environ.get("GIT_EXECUTABLE", "git"), "-C", str(root), *args]
    result = subprocess.run(cmd, input=input_data, stdout=subprocess.PIPE,
                            stderr=subprocess.DEVNULL, check=True, timeout=30)
    return result.stdout


def parse_tree_entry(record: bytes, *, indexed: bool) -> tuple[str, str, str]:
    """Return mode, object ID, filename; reject unresolved/conflicting stages."""
    header, name = record.split(b"\t", 1)
    fields = header.split(b" ")
    if indexed:
        if len(fields) != 3 or fields[2] != b"0":
            raise ValueError("unmerged_index")
        mode, oid = fields[:2]
    else:
        if len(fields) != 3 or fields[1] not in (b"blob", b"commit"):
            raise ValueError("invalid_git_tree")
        mode, oid = fields[0], fields[2]
    sha = oid.decode("ascii")
    if HEX.fullmatch(sha) is None:
        raise ValueError("invalid_git_object")
    return mode.decode("ascii"), sha, name.decode("utf-8", "surrogateescape")


def object_entry(root: Path, item: tuple[str, str, str]) -> tuple[str, bytes, bool]:
    mode, sha, path = item
    if mode not in ("100644", "100755"):
        return path, b"", True  # symlink, submodule or unknown Git mode
    size_raw = git(root, "cat-file", "-s", sha).strip()
    size = int(size_raw)
    if size < 0:
        raise ValueError("negative_git_blob_size")
    if size > MAX_TEXT_BYTES:
        return path, b"x" * (MAX_TEXT_BYTES + 1), False
    data = git(root, "cat-file", "blob", sha)
    if len(data) != size:
        raise ValueError("git_object_size_mismatch")
    return path, data, False


def inspect_git_entries(root: Path, items: list[tuple[str, str, str]], *,
                        mode: str, commits: int = 0) -> dict:
    if len(items) > MAX_ENTRIES:
        raise ValueError("unbounded_git_snapshot")
    unique = list(dict.fromkeys(items))
    result = inspect_entries([object_entry(root, item) for item in unique])
    result["event"] = "git_publication_preflight"
    result["scan_mode"] = mode
    result["checked_commits"] = commits
    result["worktree_content_used"] = False
    return result


def scan_staged(root: Path) -> dict:
    output = git(root, "ls-files", "--cached", "--stage", "-z")
    items = [parse_tree_entry(part, indexed=True)
             for part in output.split(b"\0") if part]
    return inspect_git_entries(root, items, mode="staged")


def outgoing_commits(root: Path, refs: str) -> list[str]:
    """Read Git pre-push stdin: localref localsha remoteref remotesha.

    For newly created branches, scan the full reachable history of the tip,
    including intermediate commits that added and subsequently deleted data.
    """
    commits: set[str] = set()
    seen_lines = False
    for line in refs.splitlines():
        if not line.strip():
            continue
        seen_lines = True
        fields = line.split()
        if len(fields) != 4:
            raise ValueError("invalid_pre_push_ref")
        _local_ref, local, _remote_ref, remote = fields
        if HEX.fullmatch(local) is None or HEX.fullmatch(remote) is None:
            raise ValueError("invalid_pre_push_oid")
        if ZERO.fullmatch(local):
            continue  # a deletion transmits no new commit
        rev_args = [local]
        if not ZERO.fullmatch(remote):
            rev_args.append("^" + remote)
        # When remote is new/zero, intentionally inspect all ancestors.
        history = git(root, "rev-list", *rev_args).decode("ascii").splitlines()
        if len(history) > MAX_COMMITS or len(commits) + len(history) > MAX_COMMITS:
            raise ValueError("too_many_outgoing_commits")
        if any(HEX.fullmatch(sha) is None for sha in history):
            raise ValueError("invalid_history_sha")
        commits.update(history)
    if not seen_lines:
        raise ValueError("pre_push_refs_missing")
    return sorted(commits)


def scan_pre_push(root: Path, refs: str) -> dict:
    changes = outgoing_commits(root, refs)
    items: list[tuple[str, str, str]] = []
    for sha in changes:
        tree = git(root, "ls-tree", "-rz", "-r", sha)
        items.extend(parse_tree_entry(part, indexed=False)
                     for part in tree.split(b"\0") if part)
        if len(items) > MAX_ENTRIES:
            raise ValueError("unbounded_git_history")
    return inspect_git_entries(root, items, mode="pre_push", commits=len(changes))


def main() -> int:
    parser = argparse.ArgumentParser(description="Private source pre-commit/pre-push gate")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--staged", action="store_true")
    group.add_argument("--pre-push", action="store_true")
    a = parser.parse_args()
    try:
        root = a.root.resolve()
        report = (scan_staged(root) if a.staged else
                  scan_pre_push(root, sys.stdin.read()))
    except (OSError, subprocess.SubprocessError, ValueError, UnicodeError,
            OverflowError):
        report = {
            "event": "git_publication_preflight",
            "decision": "no_go",
            "risk_classes": {"scan_failed": 1},
            "secret_values_logged": False,
            "file_names_logged": False,
            "worktree_content_used": False,
        }
    print(json.dumps(report, sort_keys=True))
    return 0 if report["decision"] == "go" else 2


if __name__ == "__main__":
    raise SystemExit(main())
