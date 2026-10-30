"""Cross-platform fault injection for disposable fixture checkpoint/restore."""
from __future__ import annotations

import json
import os
from pathlib import Path
import sys
import tempfile
import threading
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from recovery_rehearsal import (MAX_SNAPSHOT_BYTES, RehearsalError,
                                SyntheticCheckpoint, capture_preview, digest,
                                validate_preview)

RAW = (json.dumps({
    'ok': True,
    'mode': 'simulation_only',
    'side_effects': False,
    'tool': 'monitoring.health',
    'resource': 'recovery_fixture',
}, sort_keys=True) + '\n').encode()


class RecoveryRehearsalTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='synthetic-rehearsal-')
        self.addCleanup(self.temp.cleanup)
        self.checkpoint = SyntheticCheckpoint(Path(self.temp.name))

    def test_round_trip_restores_damaged_temporary_state(self):
        self.checkpoint.checkpoint(RAW)
        self.checkpoint.working.write_bytes(b'corrupted fixture')
        result = self.checkpoint.restore()
        self.assertEqual(self.checkpoint.working.read_bytes(), RAW)
        self.assertEqual(result['sha256'], digest(RAW))
        self.assertFalse(result['real_rto_measured'])
        self.assertFalse(result['external_side_effects'])

    def test_tampered_backup_is_rejected_before_write(self):
        self.checkpoint.checkpoint(RAW)
        self.checkpoint.backup.write_bytes(b'forged backup')
        self.checkpoint.working.write_bytes(b'original working bytes')
        with self.assertRaises(RehearsalError):
            self.checkpoint.restore()
        self.assertEqual(self.checkpoint.working.read_bytes(), b'original working bytes')

    def test_tampered_manifest_is_rejected_before_write(self):
        self.checkpoint.checkpoint(RAW)
        meta = json.loads(self.checkpoint.manifest.read_text())
        meta['sha256'] = '0' * 64
        self.checkpoint.manifest.write_text(json.dumps(meta))
        with self.assertRaisesRegex(RehearsalError, 'integrity'):
            self.checkpoint.restore()
        self.assertFalse(self.checkpoint.working.exists())

    def test_manifest_wrong_scope_is_rejected(self):
        self.checkpoint.checkpoint(RAW)
        meta = json.loads(self.checkpoint.manifest.read_text())
        meta['scope'] = 'customer_data'
        self.checkpoint.manifest.write_text(json.dumps(meta))
        with self.assertRaisesRegex(RehearsalError, 'scope'):
            self.checkpoint.restore()

    def test_manifest_wrong_size_is_rejected(self):
        self.checkpoint.checkpoint(RAW)
        meta = json.loads(self.checkpoint.manifest.read_text())
        meta['bytes'] += 1
        self.checkpoint.manifest.write_text(json.dumps(meta))
        with self.assertRaisesRegex(RehearsalError, 'size'):
            self.checkpoint.restore()

    def test_missing_backup_is_rejected(self):
        self.checkpoint.checkpoint(RAW)
        self.checkpoint.backup.unlink()
        with self.assertRaisesRegex(RehearsalError, 'unavailable'):
            self.checkpoint.restore()

    def test_extra_manifest_field_is_rejected(self):
        self.checkpoint.checkpoint(RAW)
        meta = json.loads(self.checkpoint.manifest.read_text())
        meta['unexpected'] = True
        self.checkpoint.manifest.write_text(json.dumps(meta))
        with self.assertRaisesRegex(RehearsalError, 'shape'):
            self.checkpoint.restore()

    def test_oversized_or_wrong_mode_previews_cannot_be_backed_up(self):
        with self.assertRaisesRegex(RehearsalError, 'size'):
            self.checkpoint.checkpoint(b'x' * (MAX_SNAPSHOT_BYTES + 1))
        invalid = RAW.replace(b'simulation_only', b'production')
        with self.assertRaisesRegex(RehearsalError, 'contract'):
            self.checkpoint.checkpoint(invalid)
        self.assertFalse(self.checkpoint.backup.exists())

    def test_bad_json_and_side_effect_claim_fail_closed(self):
        with self.assertRaisesRegex(RehearsalError, 'json'):
            validate_preview(b'not json')
        with self.assertRaisesRegex(RehearsalError, 'contract'):
            validate_preview(RAW.replace(b'false', b'true'))

    def test_events_do_not_disclose_payload_or_local_path(self):
        self.checkpoint.checkpoint(RAW)
        event = json.dumps(self.checkpoint.restore(), sort_keys=True)
        self.assertNotIn(self.temp.name, event)
        self.assertNotIn('monitoring.health', event)
        self.assertNotIn('recovery_fixture', event)
        self.assertIn('temporary_fixture_only', event)

    def test_parallel_fixture_scopes_remain_separate(self):
        failures = []
        def worker(index):
            try:
                with tempfile.TemporaryDirectory() as folder:
                    c = SyntheticCheckpoint(Path(folder))
                    c.checkpoint(RAW)
                    c.working.write_bytes(str(index).encode())
                    c.restore()
                    if c.working.read_bytes() != RAW:
                        failures.append(index)
            except Exception:
                failures.append(index)
        threads = [threading.Thread(target=worker, args=(n,)) for n in range(8)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        self.assertEqual(failures, [])

    def test_actual_executable_if_configured(self):
        executable = os.environ.get('SIMULATOR_NATIVE_EXE')
        if not executable:
            self.skipTest('Real native executable is exercised in both CI pipelines')
        raw = capture_preview(Path(executable))
        self.assertTrue(validate_preview(raw)['ok'])
        self.checkpoint.checkpoint(raw)
        self.assertEqual(self.checkpoint.restore()['result'], 'verified')


if __name__ == '__main__':
    unittest.main()
