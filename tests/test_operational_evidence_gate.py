"""Synthetic evidence only. Passing proves validation logic, never production GO."""
from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from operational_evidence_gate import assess, ValidationProblem

ROLES = ['architecture_security', 'reliability_operations', 'testing_supply_chain', 'governance_audit']


class EvidenceGateTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='review-structure-fixture-')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.manifest = {
            'schema_version': 1,
            'release_commit': 'a' * 40,
            'environment': 'authorized_preproduction',
            'scope_authorized': True,
            'reviewers': [], 'gates': {},
            'measured_recovery': {
                'rto_target_seconds': 60, 'rto_measured_seconds': 50,
                'rpo_target_seconds': 30, 'rpo_measured_seconds': 1,
            },
        }
        for i, role in enumerate(ROLES):
            att = self.make_evidence(f'review_{i}.txt', f'untrusted sample reviewer {i}')
            self.manifest['reviewers'].append({'role': role, 'reviewer_id': f'reviewer_{i}',
                                                'decision': 'approve', 'evidence': [att]})
        for i in range(8):
            att = self.make_evidence(f'gate_{i}.txt', f'untrusted sample gate {i}')
            self.manifest['gates'][f'G{i}'] = {'decision': 'pass', 'evidence': [att]}

    def make_evidence(self, path, content):
        f = self.root / path
        f.write_text(content)
        return {'path': path, 'sha256': hashlib.sha256(content.encode()).hexdigest()}

    def check_reject(self, expected):
        with self.assertRaisesRegex(ValidationProblem, expected):
            assess(self.manifest, self.root)

    def test_complete_synthetic_sample_requires_external_decision(self):
        result = assess(self.manifest, self.root)
        self.assertEqual(result['evidence_structure'], 'READY_FOR_EXTERNAL_REVIEW')
        self.assertEqual(result['production_decision'], 'NO_GO_UNTIL_INDEPENDENT_AUTHORIZATION')
        self.assertFalse(result['reviewer_independence_verified'])
        self.assertEqual(result['evidence_attachments_hashed'], 12)
        self.assertNotIn(str(self.root), str(result))
        self.assertNotIn('reviewer_0', str(result))

    def test_missing_reviewer_is_rejected(self):
        self.manifest['reviewers'].pop()
        self.check_reject('four_reviewer')

    def test_duplicate_identity_different_roles_is_rejected(self):
        self.manifest['reviewers'][1]['reviewer_id'] = self.manifest['reviewers'][0]['reviewer_id'].upper()
        self.check_reject('independent_review')

    def test_duplicate_role_is_rejected(self):
        self.manifest['reviewers'][2]['role'] = self.manifest['reviewers'][1]['role']
        self.check_reject('independent_review')

    def test_reviewer_denial_is_not_signoff(self):
        self.manifest['reviewers'][2]['decision'] = 'reject'
        self.check_reject('independent_review')

    def test_missing_gate_is_rejected(self):
        del self.manifest['gates']['G7']
        self.check_reject('operational_gates_missing')

    def test_unapproved_gate_is_rejected(self):
        self.manifest['gates']['G2']['decision'] = 'pending'
        self.check_reject('gate_not_approved')

    def test_missing_evidence_file_is_rejected(self):
        (self.root / 'review_0.txt').unlink()
        self.check_reject('evidence_missing_or_symlink')

    def test_tampered_evidence_is_rejected(self):
        (self.root / 'gate_4.txt').write_text('modified later')
        self.check_reject('evidence_hash_mismatch')

    def test_symlink_is_rejected(self):
        target = self.root / 'review_0.txt'
        fake = self.root / 'symlink.txt'
        try:
            fake.symlink_to(target)
        except OSError:
            self.skipTest('No symlink privilege on this OS')
        self.manifest['reviewers'][0]['evidence'] = [{'path': 'symlink.txt',
           'sha256': hashlib.sha256(target.read_bytes()).hexdigest()}]
        self.check_reject('evidence_missing_or_symlink')

    def test_traversal_is_rejected(self):
        self.manifest['gates']['G1']['evidence'][0]['path'] = '../secret.txt'
        self.check_reject('unsafe_evidence_path')

    def test_missing_declared_authorization_is_rejected(self):
        self.manifest['scope_authorized'] = False
        self.check_reject('declared_scope_unapproved')

    def test_recovery_target_missed_is_rejected(self):
        self.manifest['measured_recovery']['rto_measured_seconds'] = 61
        self.check_reject('recovery_targets_not_met')

    def test_recovery_metric_unknown_is_rejected(self):
        self.manifest['measured_recovery']['rpo_measured_seconds'] = 'unknown'
        self.check_reject('recovery_metrics_unmeasured')

    def test_no_nan_or_boolean_measurement_is_accepted(self):
        for value in (float('nan'), True, float('inf')):
            with self.subTest(value=value):
                self.manifest['measured_recovery']['rto_measured_seconds'] = value
                self.check_reject('recovery_metrics_unmeasured')

    def test_manifest_requires_exact_structure(self):
        self.manifest['other'] = 'not allowed'
        self.check_reject('manifest_schema')

    def test_absolute_file_is_rejected(self):
        self.manifest['reviewers'][0]['evidence'][0]['path'] = str(self.root / 'review_0.txt')
        self.check_reject('unsafe_evidence_path')

    def test_no_evidence_is_rejected(self):
        self.manifest['reviewers'][0]['evidence'] = []
        self.check_reject('evidence_required')


if __name__ == '__main__':
    unittest.main()