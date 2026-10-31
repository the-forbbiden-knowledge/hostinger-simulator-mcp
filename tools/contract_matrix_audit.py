#!/usr/bin/env python3
"""Offline-only contract audit for an existing native simulator executable.

READ ONLY: no source mutation, network, publication or production operations.
Checks responses from synthetic fixture routes, reports observed decision/ok/exit
inconsistencies without assuming the exit code is an authorization decision.
"""
from __future__ import annotations

import argparse
import hashlib
import itertools
import json
from pathlib import Path
import subprocess

from bounded_capture import capture
import sys
from collections import Counter


def cases():
    for role, tenant, session, capability, approval in itertools.product(
        ('reviewer','operator','viewer','unknown'),
        ('same_tenant','other_tenant'),
        ('same_session','other_session'),
        ('read_only','privileged'),
        ('approved','none'),
    ):
        allowed=(role in ('reviewer','operator') and tenant=='same_tenant'
                 and session=='same_session' and capability=='read_only'
                 and approval=='approved')
        yield 'a2a', ['a2a','delegate',role,tenant,session,capability,approval],allowed
    for role,tenant,source,registry,mutation,approval in itertools.product(
        ('reviewer','operator','viewer'),
        ('same_tenant','other_tenant'),
        ('trusted_application','tool_result'),
        ('pinned_registry','unverified_registry'),
        ('none','production_write'),
        ('approved','none'),
    ):
        allowed=(role=='reviewer' and tenant=='same_tenant' and source=='trusted_application'
                 and registry=='pinned_registry' and mutation=='none' and approval=='approved')
        yield 'mcp',['mcp','evaluate',role,tenant,source,registry,mutation,approval],allowed
    for role,tenant,classification,purpose in itertools.product(
        ('reviewer','operator','viewer'),
        ('same_tenant','other_tenant'),
        ('public','confidential'),
        ('knowledge_preview','export'),
    ):
        allowed=(role=='reviewer' and tenant=='same_tenant' and classification=='public'
                 and purpose=='knowledge_preview')
        yield 'rag',['rag','retrieve',role,tenant,classification,purpose],allowed
    for source,role,session,approval in itertools.product(
        ('trusted_application','web_page','tool_output','user_input'),
        ('reviewer','operator','viewer'),
        ('same_session','cross_session'),
        ('approved','none'),
    ):
        allowed=(source=='trusted_application' and role in ('reviewer','operator')
                 and session=='same_session' and approval=='approved')
        yield 'agent',['security','call',source,role,session,approval,'monitoring.health','demo'],allowed


def run(exe: Path, expected_sha: str) -> dict:
    digest=hashlib.sha256()
    if exe.stat().st_size > 50_000_000:
        raise ValueError('executable_too_large')
    with exe.open('rb') as stream:
        for chunk in iter(lambda: stream.read(65536),b''):
            digest.update(chunk)
    binary_sha=digest.hexdigest()
    if expected_sha and binary_sha.lower()!=expected_sha.lower():
        raise ValueError('executable_hash_mismatch')
    per_module={}
    for module in ('a2a','mcp','rag','agent'):
        per_module[module]=Counter()
    samples=[]
    for module, argv, allowed in cases():
        result=capture([str(exe),*argv], timeout_s=5)
        if result.failure:
            raise ValueError('bounded_capture_failed')
        data=json.loads(result.stdout)
        if not isinstance(data,dict):raise ValueError('unexpected_response_type')
        c=per_module[module]
        c['cases']+=1
        if allowed:c['expected_allow']+=1
        else:c['expected_deny']+=1
        decision=data.get('decision')
        if allowed:
            observed_allow=(decision=='allow_preview' or (module=='agent' and data.get('tool')=='monitoring.health'))
        else:
            observed_allow=decision=='deny'
        if not observed_allow:
            c['policy_decision_mismatch']+=1
            if len(samples)<8:samples.append({'module':module,'issue':'decision_mismatch','case_no':c['cases']})
        if data.get('ok') is not allowed:
            c['ok_boolean_mismatch']+=1
            if len(samples)<8:samples.append({'module':module,'issue':'ok_boolean_mismatch','case_no':c['cases']})
        if not allowed and result.returncode==0:c['denial_exit_zero']+=1
        if data.get('side_effects') is not False:c['side_effect_contract_missing']+=1
        if module!='agent' and data.get('network_access') is not False:c['offline_contract_missing']+=1
    report={
      'version':'offline-fixture-contract-audit-v1.1',
      'mode':'offline_existing_native_binary_read_only',
      'binary_sha256':binary_sha,
      'cases_total':sum(c['cases'] for c in per_module.values()),
      'per_module':{k:dict(sorted(v.items())) for k,v in per_module.items()},
      'examples_redacted':samples,
      'access_contract_go':all(c['policy_decision_mismatch']==0 and c['ok_boolean_mismatch']==0 for c in per_module.values()),
      'production_authorized':False,
      'simulator_mutated':False,
      'no_live_access_tested':True,
    }
    return report


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--binary',required=True,type=Path)
    parser.add_argument('--binary-sha256',default='')
    parser.add_argument('--report',type=Path)
    args=parser.parse_args()
    try:
        report=run(args.binary,args.binary_sha256)
        out=json.dumps(report,indent=2,sort_keys=True)
        if args.report:
            args.report.write_text(out+'\n',encoding='utf-8')
        print(out)
        return 0 if report['access_contract_go'] else 2
    except (OSError,ValueError,TypeError,subprocess.SubprocessError,json.JSONDecodeError) as exc:
        print(json.dumps({'decision':'NO_GO','reason_class':type(exc).__name__,
                          'production_authorized':False}))
        return 3

if __name__=='__main__':raise SystemExit(main())
