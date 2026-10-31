#!/usr/bin/env python3
"""Replay synthetic 232-case fixture matrix through the strict consumer.

Produces redacted category counts only. Does not publish or edit the simulator.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import subprocess

from bounded_capture import capture
import sys

from authorization_consumer import parse_response
from contract_matrix_audit import cases


def audit(executable: Path, sha256: str) -> dict:
    if hashlib.sha256(executable.read_bytes()).hexdigest() != sha256:
        raise ValueError('binary_sha_mismatch')
    results = defaultdict(Counter)
    for module, argv, expected_allow in cases():
        cmd = capture([str(executable), *argv], timeout_s=5)
        if cmd.failure:
            raise ValueError('capture_failed')
        c = parse_response(cmd.stdout, cmd.returncode, channel=module,
                           intent='operation', approved_tool_names=frozenset({'monitoring.health'}),
                           expected_tool=(argv[-2] if module=='agent' else ''),
                           expected_resource=(argv[-1] if module=='agent' else ''))
        results[module]['cases'] += 1
        results[module]['expected_allowed' if expected_allow else 'expected_denied'] += 1
        results[module][c.disposition] += 1
        # A safe consumer may reject valid preview when the reply lacks agreed
        # metadata. It MUST NOT accept an expected-denied request.
        if not expected_allow and c.preview_accepted:
            results[module]['unsafe_acceptances'] += 1
        if expected_allow and not c.preview_accepted:
            results[module]['valid_preview_rejected'] += 1
        if c.production_authorized:
            results[module]['incorrect_production_authorization'] += 1
    scenario_routes = [
        ('a2a', ['a2a', 'simulate', 'untrusted_agent_card', 'reviewer'], 'untrusted_agent_card'),
        ('mcp', ['mcp', 'simulate', 'description_instruction_poisoning', 'reviewer'], 'description_instruction_poisoning'),
        ('rag', ['rag', 'simulate', 'retrieval_context_hijack', 'reviewer'], 'retrieval_context_hijack'),
    ]
    scenario_checks = {}
    for module, argv, ident in scenario_routes:
        cmd = capture([str(executable), *argv], timeout_s=5)
        if cmd.failure:
            raise ValueError('scenario_capture_failed')
        out = parse_response(cmd.stdout, cmd.returncode, channel=module,
                             intent='scenario', expected_scenario=ident)
        scenario_checks[module] = out.disposition
    counts={k:dict(sorted(v.items())) for k,v in sorted(results.items())}
    total=sum(v['cases'] for v in results.values())
    unsafe=sum(v.get('unsafe_acceptances',0) for v in results.values())
    rejected_valid=sum(v.get('valid_preview_rejected',0) for v in results.values())
    return {
        'version':'offline-fixture-consumer-v1.1',
        'source':'known-clean-public-repository_commit_only',
        'binary_sha256':sha256,
        'cases':total,
        'modules':counts,
        'unsafe_acceptances':unsafe,
        'valid_preview_rejected':rejected_valid,
        'scenario_checks':scenario_checks,
        'security_gate_go': unsafe==0 and rejected_valid==0 and all(
            v == 'SCENARIO_RECORDED' for v in scenario_checks.values()),
        'production_authorized':False,
        'network_connections_attempted':False,
        'bounded_process_capture':True,
        'repo_modified':False,
    }


def main()->int:
    p=argparse.ArgumentParser()
    p.add_argument('--binary',type=Path,required=True)
    p.add_argument('--binary-sha256',required=True)
    p.add_argument('--report',type=Path,required=True)
    a=p.parse_args()
    try:
        out=audit(a.binary,a.binary_sha256)
    except (OSError,ValueError,subprocess.SubprocessError) as e:
        print(json.dumps({'decision':'NO_GO','error_type':type(e).__name__,'production_authorized':False}))
        return 3
    text=json.dumps(out,indent=2,sort_keys=True)+'\n'
    a.report.write_text(text,encoding='utf-8')
    print(text)
    return 0 if out['security_gate_go'] else 2


if __name__=='__main__':
    sys.exit(main())
