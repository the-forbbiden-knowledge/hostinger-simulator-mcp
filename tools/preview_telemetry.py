"""Offline regression telemetry for the local, synthetic native simulator.

Outputs aggregate fixture-test data only; not a production SLI or uptime monitor.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import re
import subprocess
import time
from typing import Callable

MAX_RESPONSE_BYTES = 65_536
EXPECTED_TOOLS = 17
MAX_PROBE_SECONDS = 5
SYNTHETIC_RESOURCE = 'telemetry_fixture'


@dataclass(frozen=True)
class Probe:
    exit_code: int
    stdout: bytes
    elapsed_ms: float


def native_runner(executable: Path) -> Callable[[tuple[str, ...]], Probe]:
    """Wrap only the locally built executable; never use a shell."""
    def call(args: tuple[str, ...]) -> Probe:
        started = time.perf_counter()
        try:
            result = subprocess.run(
                [str(executable), *args], capture_output=True,
                timeout=MAX_PROBE_SECONDS, check=False,
            )
            return Probe(result.returncode, result.stdout,
                         (time.perf_counter() - started) * 1000)
        except subprocess.TimeoutExpired:
            return Probe(124, b'', (time.perf_counter() - started) * 1000)
        except OSError:
            return Probe(127, b'', (time.perf_counter() - started) * 1000)
    return call


def payload(probe: Probe) -> dict:
    if not 0 < len(probe.stdout) <= MAX_RESPONSE_BYTES:
        raise ValueError('size')
    try:
        parsed = json.loads(probe.stdout.decode('utf-8', errors='strict'))
    except (UnicodeError, ValueError) as exc:
        raise ValueError('json') from exc
    if not isinstance(parsed, dict):
        raise ValueError('shape')
    return parsed


def percentile(values: list[float], percentile_value: int) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    pos = (len(ordered) - 1) * percentile_value / 100
    lower = int(pos)
    upper = min(lower + 1, len(ordered) - 1)
    return round(ordered[lower] + (ordered[upper] - ordered[lower]) * (pos - lower), 3)


def collect(runner: Callable[[tuple[str, ...]], Probe], rounds: int = 3,
            expected_tools: int = EXPECTED_TOOLS) -> dict:
    if not (1 <= rounds <= 5 and 1 <= expected_tools <= 64):
        raise ValueError('invalid_fixture_parameters')
    failures: list[str] = []
    latency: list[float] = []
    good = 0
    expected_denials = 0
    total = 0
    names: list[tuple[str, str]] = []

    first = runner(('list',))
    try:
        inventory = payload(first)
        entries = inventory['tools']
        if not isinstance(entries, list) or len(entries) != expected_tools:
            raise ValueError('inventory_count')
        if inventory.get('mode') != 'simulation_only':
            raise ValueError('inventory_mode')
        seen: set[str] = set()
        for entry in entries:
            if not isinstance(entry, dict):
                raise ValueError('inventory_shape')
            name, domain = entry.get('name'), entry.get('domain')
            if not isinstance(name, str) or not re.fullmatch(r'[a-z][a-z0-9_.-]{1,127}', name):
                raise ValueError('inventory_name')
            if not isinstance(domain, str) or not re.fullmatch(r'[a-z][a-z0-9_.-]{0,63}', domain):
                raise ValueError('inventory_domain')
            if name in seen:
                raise ValueError('inventory_duplicate')
            seen.add(name)
            names.append((name, domain))
        if first.exit_code != 0 or first.elapsed_ms > MAX_PROBE_SECONDS * 1000:
            raise ValueError('inventory_execution')
    except (KeyError, TypeError, ValueError):
        failures.append('inventory_invalid')
        names = []

    for _ in range(rounds):
        for name, domain in names:
            probe = runner(('call', name, SYNTHETIC_RESOURCE))
            total += 1
            try:
                value = payload(probe)
                if probe.exit_code != 0 or probe.elapsed_ms > MAX_PROBE_SECONDS * 1000:
                    raise ValueError('execution')
                if not (value.get('ok') is True and value.get('mode') == 'simulation_only'
                        and value.get('side_effects') is False and value.get('tool') == name
                        and value.get('domain') == domain
                        and value.get('resource') == SYNTHETIC_RESOURCE):
                    raise ValueError('contract')
            except ValueError:
                failures.append('unexpected_native_response')
            else:
                good += 1
                latency.append(probe.elapsed_ms)

    negatives = [
        (('call', 'unknown.telemetry_tool', SYNTHETIC_RESOURCE), 'unknown_tool'),
        (('call', 'monitoring.health', 'bad space'), 'invalid_resource'),
        (('call', 'monitoring.health', 'path/escape'), 'invalid_resource'),
        (('call', 'monitoring.health', 'x' * 257), 'invalid_resource'),
    ]
    if names:
        for args, expected_error in negatives:
            probe = runner(args)
            try:
                obj = payload(probe)
                if not (probe.exit_code == 2 and obj.get('ok') is False
                        and obj.get('error') == expected_error
                        and probe.elapsed_ms <= MAX_PROBE_SECONDS * 1000):
                    raise ValueError('denial')
            except ValueError:
                failures.append('negative_contract_failed')
            else:
                expected_denials += 1

    return {
        'schema_version': 1,
        'event': 'synthetic_native_regression_telemetry',
        'scope': 'offline_built_executable',
        'decision': 'go' if not failures and total == expected_tools * rounds
                    and expected_denials == len(negatives) else 'no_go',
        'tool_count': len(names),
        'calls_tested': total,
        'successful_previews': good,
        'expected_denials': expected_denials,
        'unexpected_failures': len(failures),
        'failure_classes': sorted(set(failures)),
        'latency_ms': {
            'p50': percentile(latency, 50),
            'p95': percentile(latency, 95),
            'max': round(max(latency), 3) if latency else 0.0,
        },
        'production_sli_measured': False,
        'production_slo_measured': False,
        'availability_claim': False,
        'external_side_effects': False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description='Local synthetic native telemetry gate')
    parser.add_argument('--simulator', type=Path, required=True)
    parser.add_argument('--rounds', type=int, default=3)
    args = parser.parse_args()
    if not args.simulator.is_file():
        print(json.dumps({'event': 'synthetic_native_regression_telemetry',
                          'decision': 'no_go', 'failure_classes': ['executable_missing']}))
        return 2
    try:
        result = collect(native_runner(args.simulator), rounds=args.rounds)
    except ValueError:
        print(json.dumps({'event': 'synthetic_native_regression_telemetry',
                          'decision': 'no_go', 'failure_classes': ['invalid_parameters']}))
        return 2
    print(json.dumps(result, sort_keys=True))
    return 0 if result['decision'] == 'go' else 2


if __name__ == '__main__':
    raise SystemExit(main())
