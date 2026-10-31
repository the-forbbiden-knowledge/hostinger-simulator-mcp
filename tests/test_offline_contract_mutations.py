"""Deterministic negative metamorphic tests, no external I/O."""
from __future__ import annotations

import json
from pathlib import Path
import random
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from authorization_consumer import parse_response


class ContractMutations(unittest.TestCase):
    def test_6000_modified_approval_envelopes_fail_closed(self):
        rng = random.Random(20261030)
        allowed = {'mode': 'simulation_only', 'ok': True,
                   'decision': 'allow_preview', 'side_effects': False,
                   'network_access': False}
        mutants = {
            'mode': [None, '', 'live', 0, False],
            'ok': [None, False, 1, 'true', [], {}],
            'decision': [None, 'deny', 'allow', 'approved', False, 0],
            'side_effects': [None, True, 0, 'false', [], {}],
            'network_access': [None, True, 0, 'false', [], {}],
        }
        for i in range(6000):
            key = rng.choice(sorted(mutants))
            data = dict(allowed)
            if rng.randrange(5) == 0:
                del data[key]
            else:
                data[key] = rng.choice(mutants[key])
            result = parse_response(json.dumps(data).encode('utf-8'), 0,
                                    channel='mcp', intent='operation')
            self.assertFalse(result.preview_accepted, (i, key, result.disposition))
            self.assertFalse(result.production_authorized)

    def test_2000_spoofed_simulation_scenarios_do_not_authorize(self):
        rng = random.Random(421)
        for i in range(2000):
            actual = f'case-{rng.randrange(1_000_000):06d}'
            expected = actual + '-other'
            data = {'mode': 'simulation_only', 'ok': True,
                    'decision': 'deny', 'scenario': actual,
                    'side_effects': False, 'network_access': False}
            raw = json.dumps(data).encode()
            verdict = parse_response(raw, 0, channel='a2a', intent='scenario',
                                     expected_scenario=expected)
            self.assertEqual(verdict.disposition, 'REJECT_SCENARIO_ID')
            self.assertFalse(verdict.preview_accepted)
            verdict2 = parse_response(raw, 0, channel='a2a', intent='operation')
            self.assertEqual(verdict2.disposition, 'REJECT_INCONSISTENT_DENIAL')
            self.assertFalse(verdict2.preview_accepted)


if __name__ == '__main__':
    unittest.main()
