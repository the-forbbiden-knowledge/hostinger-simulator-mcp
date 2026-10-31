#!/usr/bin/env python3
"""Read-only reference consumer for offline synthetic simulator output.

This is NOT a native patch, production authorization service, or API security gate.
It never interprets process success as authorization; it accepts only explicitly
validated *simulation-only* preview results. No access to real external systems.
"""
from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import hashlib
import re
import json
from pathlib import Path
from bounded_capture import capture
import sys
from typing import Any

MAX_REPLY = 65536
MAX_STDERR = 8192
MAX_ARGS = 12


class DuplicateKey(ValueError):
    pass


def no_duplicates(items: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for k, v in items:
        if k in result:
            raise DuplicateKey('duplicate_json_key')
        result[k] = v
    return result


def invalid_constant(value: str) -> None:
    raise ValueError('nonfinite_json')


@dataclass(frozen=True)
class Classification:
    policy_version: str
    intent: str
    disposition: str
    preview_accepted: bool
    production_authorized: bool
    rationale: str


def classification(intent: str, disposition: str, rationale: str) -> Classification:
    return Classification(
        policy_version='offline-fixture-consumer-v1.1', intent=intent,
        disposition=disposition,
        preview_accepted=disposition == 'ACCEPT_SYNTHETIC_PREVIEW',
        production_authorized=False, rationale=rationale,
    )


def parse_response(raw: bytes, exit_code: int, *, channel: str,
                   intent: str, approved_tool_names: frozenset[str] = frozenset(),
                   expected_scenario: str = '', expected_tool: str = '',
                   expected_resource: str = '') -> Classification:
    """Interpret only the declared intent; never infer it from JSON fields.

    `operation` means an access/preview attempt. `scenario` means a negative
    scenario simulation. An `ok:true, decision:deny` is inconsistent for an
    access operation, but can be a successful negative-scenario simulation.
    """
    if channel not in {'agent', 'a2a', 'mcp', 'rag'} or intent not in {'operation', 'scenario'}:
        return classification(intent, 'REJECT_UNSUPPORTED_ROUTE', 'invalid_channel_or_intent')
    if not isinstance(raw, bytes) or len(raw) > MAX_REPLY:
        return classification(intent, 'REJECT_INVALID_ENVELOPE', 'unbounded_output')
    if not isinstance(exit_code, int) or exit_code != 0:
        return classification(intent, 'REJECT_PROCESS_FAILURE', 'command_not_completed')
    try:
        data = json.loads(raw.decode('utf-8', 'strict'), object_pairs_hook=no_duplicates,
                          parse_constant=invalid_constant)
    except (UnicodeError, ValueError, TypeError):
        return classification(intent, 'REJECT_INVALID_ENVELOPE', 'malformed_or_duplicated_json')
    if not isinstance(data, dict):
        return classification(intent, 'REJECT_INVALID_ENVELOPE', 'non_object_reply')
    if data.get('mode') != 'simulation_only':
        return classification(intent, 'REJECT_MISSING_BOUNDARY', 'simulation_mode_not_proven')
    if data.get('side_effects') is not False:
        return classification(intent, 'REJECT_EFFECTS', 'side_effect_contract_missing')
    # The native agent call route historically omits network_access. Never
    # silently default this evidence field for MCP/A2A/RAG operations.
    if channel == 'agent':
        if 'network_access' in data and data['network_access'] is not False:
            return classification(intent, 'REJECT_EFFECTS', 'agent_network_contract_contradiction')
    elif data.get('network_access') is not False:
        return classification(intent, 'REJECT_EFFECTS', 'network_contract_missing')
    ok, decision = data.get('ok'), data.get('decision')
    if not isinstance(ok, bool):
        return classification(intent, 'REJECT_INVALID_ENVELOPE', 'non_boolean_ok')

    if intent == 'scenario':
        if not expected_scenario or data.get('scenario') != expected_scenario:
            return classification(intent, 'REJECT_SCENARIO_ID', 'scenario_identity_unconfirmed')
        if ok and decision in {'deny', 'no_go', 'hold'}:
            return classification(intent, 'SCENARIO_RECORDED', 'negative_simulation_completed_not_access')
        return classification(intent, 'REJECT_SCENARIO_RESULT', 'unexpected_scenario_output')

    if decision == 'deny':
        if ok:
            return classification(intent, 'REJECT_INCONSISTENT_DENIAL', 'ok_true_with_denial')
        return classification(intent, 'REJECT_DENIED_ACCESS', 'explicit_policy_denial')
    if decision == 'allow_preview':
        if not ok:
            return classification(intent, 'REJECT_INCONSISTENT_ALLOW', 'ok_false_with_preview')
        if channel == 'agent':
            return classification(intent, 'REJECT_SCHEMA_DRIFT', 'agent_allow_requires_tool_reply')
        return classification(intent, 'ACCEPT_SYNTHETIC_PREVIEW', 'read_only_preview_no_live_authority')
    if channel == 'agent' and decision is None and ok is True:
        tool_name = data.get('tool')
        if not isinstance(tool_name, str) or tool_name not in approved_tool_names:
            return classification(intent, 'REJECT_UNLISTED_TOOL', 'native_allowlist_not_matched')
        if (not expected_tool or not expected_resource
                or tool_name != expected_tool or data.get('resource') != expected_resource):
            return classification(intent, 'REJECT_RESOURCE_BINDING', 'reply_mismatches_requested_tool_resource')
        return classification(intent, 'ACCEPT_SYNTHETIC_PREVIEW', 'allowlisted_agent_tool_preview')
    return classification(intent, 'REJECT_UNKNOWN_DECISION', 'no_explicit_preview_authority')


def _route_matches(argv: list[str], channel: str, intent: str,
                   expected_scenario: str) -> bool:
    routes = {
        ('agent', 'operation'): ('security', 'call', 8),
        ('a2a', 'operation'): ('a2a', 'delegate', 7),
        ('mcp', 'operation'): ('mcp', 'evaluate', 8),
        ('rag', 'operation'): ('rag', 'retrieve', 6),
        ('a2a', 'scenario'): ('a2a', 'simulate', 4),
        ('mcp', 'scenario'): ('mcp', 'simulate', 4),
        ('rag', 'scenario'): ('rag', 'simulate', 4),
    }
    rule = routes.get((channel, intent))
    return bool(rule and len(argv) == rule[2] and argv[:2] == list(rule[:2])
                and (intent != 'scenario' or
                     (expected_scenario and argv[2] == expected_scenario)))


def invoke_fixture(executable: Path, argv: list[str], *, channel: str, intent: str,
                   approved_tool_names: frozenset[str] = frozenset(),
                   expected_scenario: str = '',
                   binary_sha256: str = '') -> Classification:
    """Run only pinned executable with route matching declared intent."""
    if not executable.is_file() or not argv or len(argv) > MAX_ARGS:
        return classification(intent, 'REJECT_INVALID_INVOCATION', 'missing_binary_or_args')
    if any(not isinstance(a, str) or len(a) > 256 or '\0' in a for a in argv):
        return classification(intent, 'REJECT_INVALID_INVOCATION', 'unbounded_input')
    if not _route_matches(argv, channel, intent, expected_scenario):
        return classification(intent, 'REJECT_ROUTE_MISMATCH', 'channel_intent_argv_mismatch')
    if not re.fullmatch(r'[0-9a-f]{64}', binary_sha256):
        return classification(intent, 'REJECT_UNPINNED_BINARY', 'binary_digest_required')
    try:
        if executable.is_symlink() or executable.stat().st_size > 50_000_000:
            return classification(intent, 'REJECT_UNPINNED_BINARY', 'untrusted_binary_file')
        digest = hashlib.sha256()
        with executable.open('rb') as binary:
            for block in iter(lambda: binary.read(65536), b''):
                digest.update(block)
        if digest.hexdigest() != binary_sha256:
            return classification(intent, 'REJECT_BINARY_MISMATCH', 'binary_digest_does_not_match')
    except OSError:
        return classification(intent, 'REJECT_UNPINNED_BINARY', 'unavailable_binary')
    try:
        result = capture([str(executable), *argv], timeout_s=5,
                         stdout_limit=MAX_REPLY, stderr_limit=MAX_STDERR)
    except (OSError, ValueError):
        return classification(intent, 'REJECT_PROCESS_FAILURE', 'execution_failed')
    if result.failure:
        return classification(intent, 'REJECT_CAPTURE_FAILURE', result.failure)
    expected_tool = argv[-2] if channel == 'agent' else ''
    expected_resource = argv[-1] if channel == 'agent' else ''
    return parse_response(result.stdout, result.returncode, channel=channel,
                          intent=intent, approved_tool_names=approved_tool_names,
                          expected_scenario=expected_scenario,
                          expected_tool=expected_tool,
                          expected_resource=expected_resource)


def main() -> int:
    parser = argparse.ArgumentParser(description='Offline contract consumer: no deployment authority')
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--binary-sha256', required=True, help='exact SHA-256 of offline binary')
    parser.add_argument('--channel', choices=['agent', 'a2a', 'mcp', 'rag'], required=True)
    parser.add_argument('--intent', choices=['operation', 'scenario'], required=True)
    parser.add_argument('--expected-scenario', default='')
    parser.add_argument('--allow-tool', action='append', default=[])
    parser.add_argument('args', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    result = invoke_fixture(args.binary, args.args, channel=args.channel,
                            intent=args.intent, approved_tool_names=frozenset(args.allow_tool),
                            expected_scenario=args.expected_scenario,
                            binary_sha256=args.binary_sha256)
    print(json.dumps(asdict(result), sort_keys=True))
    return 0 if result.disposition in {'ACCEPT_SYNTHETIC_PREVIEW', 'SCENARIO_RECORDED'} else 2


if __name__ == '__main__':
    sys.exit(main())
