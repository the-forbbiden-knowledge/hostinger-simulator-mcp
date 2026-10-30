"""Public source snapshot safeguard for CI and optional pre-commit use.

This heuristic rejects private file types/paths and high-confidence credential
patterns in tracked files. It does not prove licensing or absence of all secrets.
No tracked payload, file name, or credential value is emitted in diagnostics.
"""
from __future__ import annotations

from collections import Counter
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess

MAX_TEXT_BYTES = 2_000_000
PRIVATE_SEGMENTS = frozenset({
    'private', 'library', 'customer', 'customers', 'customer-data',
    'client-documents', 'third-party', 'third_party', 'reference-library',
    'secret', 'secrets', 'credentials', 'private-catalog',
})
BLOCKED_EXTENSIONS = frozenset({
    '.pdf', '.zip', '.7z', '.rar', '.tar', '.tgz', '.gz',
    '.doc', '.docx', '.xls', '.xlsx', '.ppt', '.pptx',
    '.pem', '.pfx', '.p12', '.key', '.kdbx', '.sqlite', '.db', '.dump',
})
SECRET_PATTERNS = (
    ('private_key', re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----')),
    ('cloud_key', re.compile(rb'\bAKIA[A-Z0-9]{16}\b')),
    ('provider_token', re.compile(rb'\b(?:gh[pousr]_|github_pat_)[A-Za-z0-9_]{30,}\b')),
)


def check_entry(name: str, content: bytes, *, symlink: bool = False) -> set[str]:
    """Report categories only; never echo potential private paths or contents."""
    problems: set[str] = set()
    standardized = name.replace('\\', '/')
    path = PurePosixPath(standardized)
    segments = tuple(p.lower() for p in path.parts)
    if path.is_absolute() or not segments or '.' in segments or '..' in segments:
        problems.add('invalid_path')
    if set(segments) & PRIVATE_SEGMENTS:
        problems.add('private_path')
    basename = segments[-1] if segments else ''
    if basename == '.env' or (basename.startswith('.env.') and basename != '.env.example'):
        problems.add('environment_file')
    if path.suffix.lower() in BLOCKED_EXTENSIONS:
        problems.add('proprietary_or_sensitive_file_type')
    if symlink:
        problems.add('symlink_requires_manual_review')
    if len(content) > MAX_TEXT_BYTES:
        problems.add('oversized_unreviewed_file')
        return problems
    for category, expression in SECRET_PATTERNS:
        if expression.search(content):
            problems.add(category)
    return problems


def inspect_entries(entries: list[tuple[str, bytes, bool]]) -> dict:
    failures: list[dict] = []
    counts: Counter = Counter()
    for path, data, symlink in entries:
        problems = sorted(check_entry(path, data, symlink=symlink))
        if problems:
            counts.update(problems)
            failures.append({
                'path_sha256_prefix': hashlib.sha256(path.encode('utf-8')).hexdigest()[:16],
                'classes': problems,
            })
    return {
        'event': 'public_source_snapshot_gate',
        'decision': 'go' if not failures else 'no_go',
        'checked_files': len(entries),
        'violating_files': len(failures),
        'risk_classes': dict(sorted(counts.items())),
        'violations': failures,
        'secret_values_logged': False,
        'file_names_logged': False,
        'licensing_completeness_claim': False,
    }


def scan_tracked(root: Path) -> dict:
    root = root.resolve()
    result = subprocess.run([os.environ.get('GIT_EXECUTABLE', 'git'), '-C', str(root), 'ls-files', '--cached', '-z'],
                            check=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                            timeout=10)
    entries: list[tuple[str, bytes, bool]] = []
    for part in result.stdout.split(b'\x00'):
        if not part:
            continue
        name = part.decode('utf-8', errors='surrogateescape')
        path = root.joinpath(*PurePosixPath(name).parts)
        is_link = path.is_symlink()
        if is_link:
            entries.append((name, b'', True))
        elif path.is_file():
            with path.open('rb') as handle:
                entries.append((name, handle.read(MAX_TEXT_BYTES + 1), False))
        else:
            entries.append((name, b'', True))
    return inspect_entries(entries)


def main() -> int:
    import argparse
    parser = argparse.ArgumentParser(description='Read-only public source privacy gate')
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    options = parser.parse_args()
    try:
        report = scan_tracked(options.root)
    except (OSError, subprocess.SubprocessError, UnicodeError):
        report = {'event': 'public_source_snapshot_gate', 'decision': 'no_go',
                  'risk_classes': {'scan_failed': 1}, 'secret_values_logged': False,
                  'file_names_logged': False, 'licensing_completeness_claim': False}
    print(json.dumps(report, sort_keys=True))
    return 0 if report['decision'] == 'go' else 2


if __name__ == '__main__':
    raise SystemExit(main())
