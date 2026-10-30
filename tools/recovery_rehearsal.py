"""Local-only integrity and recovery rehearsal for synthetic simulator previews.

Only writes to a caller-provided disposable test directory. Does not operate on
customer data, production services, or repository contents.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
from bounded_subprocess import run_bounded
import tempfile
import time

MAX_SNAPSHOT_BYTES = 65_536
VALID_RESOURCE = 'recovery_fixture'


class RehearsalError(Exception):
    """Expected rejection of an unverified snapshot or operation."""


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def validate_preview(raw: bytes) -> dict:
    if not raw or len(raw) > MAX_SNAPSHOT_BYTES:
        raise RehearsalError('invalid_preview_size')
    try:
        value = json.loads(raw)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise RehearsalError('invalid_preview_json') from exc
    if not isinstance(value, dict) or not (
        value.get('ok') is True
        and value.get('mode') == 'simulation_only'
        and value.get('side_effects') is False
        and value.get('tool') == 'monitoring.health'
        and value.get('resource') == VALID_RESOURCE
    ):
        raise RehearsalError('preview_contract_rejected')
    return value


def capture_preview(executable: Path) -> bytes:
    if not executable.is_file():
        raise RehearsalError('executable_missing')
    result = run_bounded(
        [str(executable), 'call', 'monitoring.health', VALID_RESOURCE],
        timeout_s=5, max_output=MAX_SNAPSHOT_BYTES,
    )
    if result.reason is not None or result.returncode != 0:
        raise RehearsalError('preview_execution_failed')
    validate_preview(result.stdout)
    return result.stdout


def _atomic_write(path: Path, data: bytes) -> None:
    """Atomic content replacement of a disposable local fixture (not system DR)."""
    with tempfile.NamedTemporaryFile(dir=path.parent, prefix='.tmp-', delete=False) as temp:
        staged = Path(temp.name)
        try:
            temp.write(data)
            temp.flush()
            os.fsync(temp.fileno())
        except Exception:
            staged.unlink(missing_ok=True)
            raise
    try:
        os.replace(staged, path)
    finally:
        staged.unlink(missing_ok=True)


class SyntheticCheckpoint:
    def __init__(self, directory: Path):
        self.directory = Path(directory)
        if not self.directory.is_dir():
            raise RehearsalError('fixture_directory_missing')
        self.backup = self.directory / 'checkpoint.bin'
        self.manifest = self.directory / 'checkpoint.manifest.json'
        self.working = self.directory / 'working.json'

    def checkpoint(self, raw: bytes) -> None:
        validate_preview(raw)
        meta = {
            'schema_version': 1,
            'scope': 'synthetic_only',
            'bytes': len(raw),
            'sha256': digest(raw),
        }
        _atomic_write(self.backup, raw)
        _atomic_write(self.manifest, json.dumps(meta, sort_keys=True).encode('utf-8'))

    def _verified_backup(self) -> bytes:
        try:
            meta = json.loads(self.manifest.read_bytes())
            raw = self.backup.read_bytes()
        except (OSError, ValueError, UnicodeDecodeError) as exc:
            raise RehearsalError('checkpoint_unavailable_or_invalid') from exc
        if not isinstance(meta, dict) or set(meta) != {'schema_version', 'scope', 'bytes', 'sha256'}:
            raise RehearsalError('manifest_shape_rejected')
        if type(meta['schema_version']) is not int or meta['schema_version'] != 1 or meta['scope'] != 'synthetic_only':
            raise RehearsalError('manifest_scope_rejected')
        size = meta['bytes']
        if type(size) is not int or not (0 < size <= MAX_SNAPSHOT_BYTES) or size != len(raw):
            raise RehearsalError('checkpoint_size_rejected')
        checksum = meta['sha256']
        if not isinstance(checksum, str) or len(checksum) != 64 or digest(raw) != checksum:
            raise RehearsalError('checkpoint_integrity_rejected')
        validate_preview(raw)
        return raw

    def restore(self) -> dict:
        started = time.monotonic_ns()
        raw = self._verified_backup()  # Must verify before any write.
        _atomic_write(self.working, raw)
        if digest(self.working.read_bytes()) != digest(raw):
            raise RehearsalError('restore_integrity_rejected')
        duration_ms = (time.monotonic_ns() - started) / 1_000_000
        return {
            'event': 'synthetic_recovery_rehearsal',
            'result': 'verified',
            'scope': 'temporary_fixture_only',
            'sha256': digest(raw),
            'bytes': len(raw),
            'duration_ms': round(duration_ms, 3),
            'external_side_effects': False,
            'temporary_fixture_writes': True,
            'real_rto_measured': False,
            'real_rpo_measured': False,
        }


def main() -> int:
    import argparse
    p = argparse.ArgumentParser(description='Offline synthetic recovery-integrity rehearsal')
    p.add_argument('--simulator', required=True, type=Path)
    a = p.parse_args()
    try:
        raw = capture_preview(a.simulator)
        with tempfile.TemporaryDirectory(prefix='local-rehearsal-') as tempdir:
            c = SyntheticCheckpoint(Path(tempdir))
            c.checkpoint(raw)
            c.working.write_bytes(b'invalid, synthetic corruption')
            result = c.restore()
            print(json.dumps(result, sort_keys=True))
            return 0
    except RehearsalError as exc:
        print(json.dumps({'event': 'synthetic_recovery_rehearsal', 'result': 'rejected',
                          'reason': str(exc), 'external_side_effects': False}, sort_keys=True))
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
