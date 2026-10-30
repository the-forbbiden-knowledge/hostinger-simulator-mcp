"""Validate evidence package completeness for human release review, never approve production.

All evidence files belong to an explicitly supplied private directory. Validation
proves local byte integrity, not signer identity, operational truth or independence.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re

ROLES = ('architecture_security', 'reliability_operations',
         'testing_supply_chain', 'governance_audit')
GATES = tuple(f'G{i}' for i in range(8))
MAX_EVIDENCE_BYTES = 4_000_000
MAX_MANIFEST_BYTES = 200_000
SHA256 = re.compile(r'[0-9a-f]{64}\Z')
COMMIT_ID = re.compile(r'(?:[0-9a-f]{40}|[0-9a-f]{64})\Z')


class ValidationProblem(Exception):
    pass


def _check_attachment(root: Path, item: object) -> None:
    if not isinstance(item, dict) or set(item) != {'path', 'sha256'}:
        raise ValidationProblem('attachment_schema')
    name, expected = item['path'], item['sha256']
    if not isinstance(name, str) or not isinstance(expected, str) or not SHA256.fullmatch(expected):
        raise ValidationProblem('attachment_schema')
    if not name or '\\' in name or ':' in name or name.startswith('/'):
        raise ValidationProblem('unsafe_evidence_path')
    relative = PurePosixPath(name)
    if any(x in ('', '.', '..') for x in relative.parts):
        raise ValidationProblem('unsafe_evidence_path')
    candidate = root.joinpath(*relative.parts)
    try:
        if candidate.is_symlink() or not candidate.is_file():
            raise ValidationProblem('evidence_missing_or_symlink')
        full = candidate.resolve(strict=True)
        if not full.is_relative_to(root):
            raise ValidationProblem('unsafe_evidence_path')
        if full.stat().st_size > MAX_EVIDENCE_BYTES or full.stat().st_size == 0:
            raise ValidationProblem('evidence_size_rejected')
        with full.open('rb') as handle:
            data = handle.read(MAX_EVIDENCE_BYTES + 1)
    except OSError as exc:
        raise ValidationProblem('evidence_unreadable') from exc
    if not data or len(data) > MAX_EVIDENCE_BYTES:
        raise ValidationProblem('evidence_size_rejected')
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValidationProblem('evidence_hash_mismatch')


def _attachments(root: Path, obj: object) -> int:
    if not isinstance(obj, list) or not 1 <= len(obj) <= 10:
        raise ValidationProblem('evidence_required')
    for entry in obj:
        _check_attachment(root, entry)
    return len(obj)


def assess(manifest: object, evidence_root: Path) -> dict:
    """Structural readiness only. An actual GO requires independent external review."""
    root = evidence_root.resolve(strict=True)
    if not root.is_dir():
        raise ValidationProblem('evidence_root_invalid')
    if not isinstance(manifest, dict) or set(manifest) != {
        'schema_version', 'release_commit', 'environment', 'scope_authorized',
        'reviewers', 'gates', 'measured_recovery'}:
        raise ValidationProblem('manifest_schema')
    if type(manifest['schema_version']) is not int or manifest['schema_version'] != 1:
        raise ValidationProblem('manifest_version')
    if not isinstance(manifest['release_commit'], str) or not COMMIT_ID.fullmatch(manifest['release_commit']):
        raise ValidationProblem('commit_missing')
    if manifest['environment'] not in ('authorized_preproduction', 'authorized_production') \
            or manifest['scope_authorized'] is not True:
        raise ValidationProblem('declared_scope_unapproved')

    reviewers = manifest['reviewers']
    if not isinstance(reviewers, list) or len(reviewers) != len(ROLES):
        raise ValidationProblem('four_reviewer_records_required')
    identities: set[str] = set()
    roles: set[str] = set()
    files_checked = 0
    for reviewer in reviewers:
        if not isinstance(reviewer, dict) or set(reviewer) != {
            'role', 'reviewer_id', 'decision', 'evidence'}:
            raise ValidationProblem('reviewer_schema')
        role, identity = reviewer['role'], reviewer['reviewer_id']
        if role not in ROLES or role in roles or not isinstance(identity, str) \
                or not re.fullmatch(r'[A-Za-z0-9_.-]{3,80}', identity) \
                or identity.casefold() in identities or reviewer['decision'] != 'approve':
            raise ValidationProblem('independent_review_records_incomplete')
        roles.add(role)
        identities.add(identity.casefold())
        files_checked += _attachments(root, reviewer['evidence'])
    if roles != set(ROLES):
        raise ValidationProblem('reviewer_roles_missing')

    gates = manifest['gates']
    if not isinstance(gates, dict) or set(gates) != set(GATES):
        raise ValidationProblem('operational_gates_missing')
    for gate in GATES:
        record = gates[gate]
        if not isinstance(record, dict) or set(record) != {'decision', 'evidence'} \
                or record['decision'] != 'pass':
            raise ValidationProblem('gate_not_approved')
        files_checked += _attachments(root, record['evidence'])

    metrics = manifest['measured_recovery']
    required = ('rto_target_seconds', 'rto_measured_seconds',
                'rpo_target_seconds', 'rpo_measured_seconds')
    if not isinstance(metrics, dict) or set(metrics) != set(required):
        raise ValidationProblem('recovery_metrics_missing')
    for metric in required:
        value = metrics[metric]
        if type(value) not in (int, float) or not 0 <= value < 10**9:
            raise ValidationProblem('recovery_metrics_unmeasured')
    if (metrics['rto_measured_seconds'] > metrics['rto_target_seconds'] or
            metrics['rpo_measured_seconds'] > metrics['rpo_target_seconds']):
        raise ValidationProblem('recovery_targets_not_met')

    return {
        'event': 'operational_release_evidence_structure',
        'evidence_structure': 'READY_FOR_EXTERNAL_REVIEW',
        'production_decision': 'NO_GO_UNTIL_INDEPENDENT_AUTHORIZATION',
        'four_reviewer_records_present': True,
        'reviewer_independence_verified': False,
        'signature_authenticity_verified': False,
        'real_service_validated': False,
        'gates_with_evidence': len(GATES),
        'evidence_attachments_hashed': files_checked,
        'local_file_paths_logged': False,
        'source_content_logged': False,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description='Private offline evidence-structure preflight (never a production GO)')
    ap.add_argument('--manifest', type=Path, required=True)
    ap.add_argument('--evidence-root', type=Path, required=True)
    args = ap.parse_args()
    try:
        if not 0 < args.manifest.stat().st_size <= MAX_MANIFEST_BYTES:
            raise ValidationProblem('manifest_size')
        manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
        result = assess(manifest, args.evidence_root)
        exit_code = 0  # Successful structural verification only.
    except (OSError, ValueError, UnicodeError, ValidationProblem) as exc:
        category = str(exc) if isinstance(exc, ValidationProblem) else 'input_invalid'
        result = {
            'event': 'operational_release_evidence_structure',
            'evidence_structure': 'NO_GO',
            'production_decision': 'NO_GO',
            'failure_class': category,
            'reviewer_independence_verified': False,
            'real_service_validated': False,
            'local_file_paths_logged': False,
            'source_content_logged': False,
        }
        exit_code = 2
    print(json.dumps(result, sort_keys=True))
    return exit_code


if __name__ == '__main__':
    raise SystemExit(main())