from __future__ import annotations

import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from authorization_consumer import invoke_fixture, parse_response


def encode(**fields) -> bytes:
    return json.dumps({'mode': 'simulation_only', 'network_access': False,
                       'side_effects': False, **fields}).encode('utf-8')


class OfflineConsumerTests(unittest.TestCase):
    def check(self, raw, *, channel='a2a', intent='operation', exit_code=0,
              approved=frozenset(), scenario='', tool='', resource='', outcome):
        actual = parse_response(raw, exit_code, channel=channel, intent=intent,
                                approved_tool_names=approved,
                                expected_scenario=scenario, expected_tool=tool,
                                expected_resource=resource)
        self.assertEqual(actual.disposition, outcome)
        self.assertFalse(actual.production_authorized)
        return actual

    def test_each_operation_access_denied(self):
        for channel in ('a2a', 'mcp', 'rag', 'agent'):
            with self.subTest(channel=channel):
                self.check(encode(ok=False, decision='deny'), channel=channel,
                           outcome='REJECT_DENIED_ACCESS')

    def test_prior_real_bug_denial_must_fail_closed(self):
        for channel in ('a2a', 'agent'):
            self.check(encode(ok=True, decision='deny'), channel=channel,
                       outcome='REJECT_INCONSISTENT_DENIAL')

    def test_exit_zero_never_authorizes_denial(self):
        self.check(encode(ok=False, decision='deny'), outcome='REJECT_DENIED_ACCESS')
        self.check(encode(ok=True, decision='deny'), outcome='REJECT_INCONSISTENT_DENIAL')

    def test_any_nonzero_exit_rejects(self):
        self.check(encode(ok=True, decision='allow_preview'), exit_code=2,
                   outcome='REJECT_PROCESS_FAILURE')

    def test_mcp_a2a_rag_valid_preview(self):
        for channel in ('a2a', 'mcp', 'rag'):
            result = self.check(encode(ok=True, decision='allow_preview'),
                                channel=channel, outcome='ACCEPT_SYNTHETIC_PREVIEW')
            self.assertTrue(result.preview_accepted)

    def test_negative_simulation_is_not_access(self):
        raw = encode(ok=True, decision='deny', scenario='untrusted_agent_card')
        scenario = self.check(raw, intent='scenario', scenario='untrusted_agent_card',
                              outcome='SCENARIO_RECORDED')
        self.assertFalse(scenario.preview_accepted)
        self.check(raw, intent='operation', outcome='REJECT_INCONSISTENT_DENIAL')

    def test_scenario_must_identify_expected_case(self):
        raw = encode(ok=True, decision='deny', scenario='untrusted_agent_card')
        self.check(raw, intent='scenario', scenario='different', outcome='REJECT_SCENARIO_ID')
        self.check(raw, intent='scenario', scenario='', outcome='REJECT_SCENARIO_ID')

    def test_scenario_must_not_create_allowed_access(self):
        self.check(encode(ok=True, decision='allow_preview', scenario='case-a'),
                   intent='scenario', scenario='case-a', outcome='REJECT_SCENARIO_RESULT')

    def test_agent_requires_explicit_allowlisted_tool(self):
        raw = encode(ok=True, tool='monitoring.health', resource='demo')
        self.check(raw, channel='agent', outcome='REJECT_UNLISTED_TOOL')
        self.check(raw, channel='agent', approved=frozenset({'monitoring.health'}), tool='monitoring.health',
                   resource='demo', outcome='ACCEPT_SYNTHETIC_PREVIEW')
        self.check(encode(ok=True, tool='unknown.tool', resource='demo'),channel='agent',
                   approved=frozenset({'monitoring.health'}),outcome='REJECT_UNLISTED_TOOL')

    def test_agent_allow_preview_without_tool_not_authorized(self):
        self.check(encode(ok=True, decision='allow_preview'),channel='agent',
                   outcome='REJECT_SCHEMA_DRIFT')

    def test_rag_missing_mode_is_denied_even_when_preview(self):
        raw=json.dumps({'ok': True, 'decision':'allow_preview', 'side_effects': False,
                        'network_access':False}).encode()
        self.check(raw, channel='rag', outcome='REJECT_MISSING_BOUNDARY')

    def test_duplicate_keys_rejected(self):
        self.check(b'{"mode":"simulation_only","ok":true,"ok":false}',
                   outcome='REJECT_INVALID_ENVELOPE')

    def test_unexpected_or_missing_fields_rejected(self):
        self.check(encode(ok=True, decision='unknown'), outcome='REJECT_UNKNOWN_DECISION')
        self.check(encode(ok='true',decision='allow_preview'), outcome='REJECT_INVALID_ENVELOPE')
        self.check(encode(ok=True,decision='allow_preview',side_effects=True), outcome='REJECT_EFFECTS')
        self.check(encode(ok=True,decision='allow_preview',network_access=True), outcome='REJECT_EFFECTS')
        self.check(encode(ok=True,decision='allow_preview',mode='unknown'), outcome='REJECT_MISSING_BOUNDARY')

    def test_invalid_outputs_fail_closed(self):
        self.check(b'not-json', outcome='REJECT_INVALID_ENVELOPE')
        self.check(b'[]', outcome='REJECT_INVALID_ENVELOPE')
        self.check(b'\xff', outcome='REJECT_INVALID_ENVELOPE')
        self.check(b'x' * 65537, outcome='REJECT_INVALID_ENVELOPE')
        self.check(b'{"ok":NaN}', outcome='REJECT_INVALID_ENVELOPE')

    def test_missing_executable_fail_closed(self):
        decision = invoke_fixture(Path('/nonexistent/simulator'), ['a2a'],
                                  channel='a2a', intent='operation')
        self.assertEqual(decision.disposition,'REJECT_INVALID_INVOCATION')
        self.assertFalse(decision.production_authorized)


if __name__ == '__main__':
    unittest.main()
