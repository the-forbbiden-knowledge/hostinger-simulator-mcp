"""Deterministic fault injection for local-only synthetic telemetry gates."""
import json
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from preview_telemetry import Probe, collect, payload, percentile


def make_runner(*, inject=None):
    entries = [{'name': 'monitoring.health', 'domain': 'monitoring'},
               {'name': 'backup.restore_preview', 'domain': 'backup'}]
    def runner(args):
        if inject is not None:
            special = inject(args)
            if special is not None:
                return special
        if args == ('list',):
            return Probe(0, json.dumps({'mode': 'simulation_only', 'tools': entries}).encode(), 1)
        if args[0] == 'call' and args[1] == 'unknown.telemetry_tool':
            return Probe(2, b'{"ok":false,"error":"unknown_tool"}', 1)
        if args[0] == 'call' and args[2] != 'telemetry_fixture':
            return Probe(2, b'{"ok":false,"error":"invalid_resource"}', 1)
        entry = next(e for e in entries if e['name'] == args[1])
        return Probe(0, json.dumps({
            'ok': True, 'mode': 'simulation_only', 'tool': args[1],
            'domain': entry['domain'], 'resource': args[2], 'side_effects': False
        }).encode(), 2)
    return runner


class TelemetryTests(unittest.TestCase):
    def test_valid_fixture_gate_and_metrics(self):
        report = collect(make_runner(), rounds=3, expected_tools=2)
        self.assertEqual(report['decision'], 'go')
        self.assertEqual(report['calls_tested'], 6)
        self.assertEqual(report['expected_denials'], 4)
        self.assertFalse(report['production_slo_measured'])
        self.assertEqual(report['latency_ms']['p95'], 2)

    def test_unexpected_write_claim_denied(self):
        def inject(args):
            if args[:2] == ('call', 'monitoring.health') and args[2] == 'telemetry_fixture':
                return Probe(0, json.dumps({'ok': True, 'mode': 'simulation_only',
                    'side_effects': True}).encode(), 1)
        report = collect(make_runner(inject=inject), expected_tools=2)
        self.assertEqual(report['decision'], 'no_go')
        self.assertIn('unexpected_native_response', report['failure_classes'])

    def test_timeout_classified_without_exposing_paths(self):
        def inject(args):
            if args[:2] == ('call', 'backup.restore_preview'):
                return Probe(124, b'', 5010)
        report = collect(make_runner(inject=inject), expected_tools=2)
        self.assertEqual(report['decision'], 'no_go')
        self.assertNotIn('backup.restore_preview', json.dumps(report))

    def test_invalid_json_and_nonobject_denied(self):
        for raw in (b'{bad', b'[]', b'', b'x' * 65537):
            with self.subTest(raw_size=len(raw)):
                with self.assertRaises(ValueError):
                    payload(Probe(0, raw, 1))

    def test_mismatch_of_tool_identity_denied(self):
        def inject(args):
            if args[:2] == ('call', 'monitoring.health') and args[2] == 'telemetry_fixture':
                return Probe(0, json.dumps({'ok': True, 'mode': 'simulation_only',
                    'side_effects': False, 'tool': 'some.other_tool',
                    'domain': 'monitoring', 'resource': 'telemetry_fixture'}).encode(), 2)
        self.assertEqual(collect(make_runner(inject=inject), expected_tools=2)['decision'], 'no_go')

    def test_unexpected_denial_failure(self):
        def inject(args):
            if args[0] == 'call' and args[1] == 'unknown.telemetry_tool':
                return Probe(0, b'{"ok":true,"error":"unknown_tool"}', 1)
        self.assertEqual(collect(make_runner(inject=inject), expected_tools=2)['decision'], 'no_go')

    def test_inventory_mismatch_denied(self):
        self.assertEqual(collect(make_runner(), expected_tools=3)['failure_classes'], ['inventory_invalid'])

    def test_duplicate_inventory_denied(self):
        dup = json.dumps({'mode': 'simulation_only', 'tools': [
            {'name':'monitoring.health', 'domain':'monitoring'},
            {'name':'monitoring.health', 'domain':'monitoring'}]}).encode()
        def inject(args):
            if args == ('list',):
                return Probe(0, dup, 1)
        self.assertEqual(collect(make_runner(inject=inject), expected_tools=2)['decision'], 'no_go')

    def test_limits_fail_closed(self):
        for rounds in (0, 6):
            with self.assertRaises(ValueError):
                collect(make_runner(), rounds=rounds, expected_tools=2)
        with self.assertRaises(ValueError):
            collect(make_runner(), expected_tools=0)

    def test_percentiles_do_not_invent_data(self):
        self.assertEqual(percentile([], 95), 0)
        self.assertEqual(percentile([1, 2, 3], 50), 2)
        self.assertEqual(percentile([1, 2, 3], 95), 2.9)


if __name__ == '__main__':
    unittest.main()
