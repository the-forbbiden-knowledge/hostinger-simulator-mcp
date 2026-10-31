"""Synthetic adversarial tests for bounded subprocess output and reply binding.

No external targets or live network access. Direct Python subprocess fixtures only.
"""
from __future__ import annotations

import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from authorization_consumer import invoke_fixture, parse_response
from bounded_capture import capture


def response(**more: object) -> bytes:
    return json.dumps({
        'ok': True, 'mode': 'simulation_only', 'side_effects': False,
        'tool': 'monitoring.health', 'resource': 'fixture-1', **more,
    }).encode('utf-8')


class CaptureNegativeTests(unittest.TestCase):
    def test_small_output_is_preserved(self):
        r = capture([sys.executable, '-c', 'print("demo")'], timeout_s=2)
        self.assertIsNone(r.failure)
        self.assertEqual(r.returncode, 0)
        self.assertEqual(r.stdout.replace(b'\r\n', b'\n'), b'demo\n')

    def test_stdout_spam_is_stopped_before_unbounded_capture(self):
        r = capture([sys.executable, '-c',
                     'import sys;sys.stdout.buffer.write(b"a"*2500000)'],
                    timeout_s=3, stdout_limit=8192)
        self.assertEqual(r.failure, 'output_limit')
        self.assertEqual(r.stdout, b'')

    def test_stderr_spam_is_stopped_and_not_exposed(self):
        r = capture([sys.executable, '-c',
                     'import sys;sys.stderr.buffer.write(b"SECRET"*900000)'],
                    timeout_s=3, stderr_limit=4096)
        self.assertEqual(r.failure, 'stderr_limit')
        self.assertNotIn(b'SECRET', r.stdout)
        self.assertLessEqual(r.stderr_bytes, 4096)

    def test_timeout_stops_trickling_child(self):
        r = capture([sys.executable, '-c',
                     'import sys,time;sys.stdout.write("a");sys.stdout.flush();time.sleep(2)'],
                    timeout_s=.2)
        self.assertEqual(r.failure, 'timeout')
        self.assertEqual(r.returncode, 124)

    def test_process_nonzero_exit_is_preserved(self):
        r = capture([sys.executable, '-c', 'import sys;sys.exit(9)'], timeout_s=2)
        self.assertIsNone(r.failure)
        self.assertEqual(r.returncode, 9)

    def test_invalid_limits_and_nul_arguments_rejected(self):
        for kwargs in ({'stdout_limit': 0}, {'stderr_limit': 0}, {'timeout_s': 0}):
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                capture([sys.executable, '-V'], **kwargs)
        with self.assertRaises(ValueError):
            capture([sys.executable, 'arg\0break'])

    def test_separate_stream_capture_prevents_stderr_in_json(self):
        r = capture([sys.executable, '-c',
                     'import sys;sys.stderr.write("fixture warning");'
                     'print("{\\\"ok\\\":true}")'], timeout_s=2)
        self.assertIsNone(r.failure)
        self.assertEqual(json.loads(r.stdout), {'ok': True})
        self.assertGreater(r.stderr_bytes, 0)


class BindingNegativeTests(unittest.TestCase):
    def check(self, raw: bytes, *, tool='monitoring.health', resource='fixture-1'):
        return parse_response(raw, 0, channel='agent', intent='operation',
                              approved_tool_names=frozenset({'monitoring.health'}),
                              expected_tool=tool, expected_resource=resource)

    def test_matching_resource_and_tool_accepted_for_fixture_only(self):
        result = self.check(response())
        self.assertEqual(result.disposition, 'ACCEPT_SYNTHETIC_PREVIEW')
        self.assertFalse(result.production_authorized)

    def test_other_resource_is_rejected_despite_allowed_tool(self):
        result = self.check(response(resource='other-fixture'))
        self.assertEqual(result.disposition, 'REJECT_RESOURCE_BINDING')
        self.assertFalse(result.preview_accepted)

    def test_missing_expected_resource_is_not_accepted(self):
        result = self.check(response(), resource='')
        self.assertEqual(result.disposition, 'REJECT_RESOURCE_BINDING')

    def test_wrong_expected_tool_is_not_accepted(self):
        result = self.check(response(), tool='vps.list')
        self.assertEqual(result.disposition, 'REJECT_RESOURCE_BINDING')

    def test_unlisted_response_tool_denied(self):
        result = self.check(response(tool='other.tool'))
        self.assertEqual(result.disposition, 'REJECT_UNLISTED_TOOL')

    def test_channel_claim_must_match_command_route(self):
        result = invoke_fixture(Path(sys.executable),
                                ['mcp','evaluate','reviewer','same_tenant',
                                 'trusted_application','pinned_registry','none','approved'],
                                channel='a2a', intent='operation')
        self.assertEqual(result.disposition, 'REJECT_ROUTE_MISMATCH')

    def test_intent_claim_must_match_command_route(self):
        result = invoke_fixture(Path(sys.executable),
                                ['a2a', 'simulate', 'untrusted_agent_card', 'reviewer'],
                                channel='a2a', intent='operation')
        self.assertEqual(result.disposition, 'REJECT_ROUTE_MISMATCH')

    def test_scenario_id_must_match_command_route(self):
        result = invoke_fixture(Path(sys.executable),
                                ['a2a', 'simulate', 'untrusted_agent_card', 'reviewer'],
                                channel='a2a', intent='scenario',
                                expected_scenario='different_case')
        self.assertEqual(result.disposition, 'REJECT_ROUTE_MISMATCH')


if __name__ == '__main__':
    unittest.main()

# A digest pins the local fixture file's bytes before execution. This is not a
# cryptographic signature, nor an absolute guarantee against TOCTOU replacement.
class PinnedBinaryTests(unittest.TestCase):
    def test_unpinned_valid_route_does_not_start(self):
        args=['a2a','delegate','operator','same_tenant','same_session','read_only','approved']
        result=invoke_fixture(Path(sys.executable),args,channel='a2a',intent='operation')
        self.assertEqual(result.disposition,'REJECT_UNPINNED_BINARY')

    def test_wrong_sha256_does_not_start(self):
        args=['a2a','delegate','operator','same_tenant','same_session','read_only','approved']
        result=invoke_fixture(Path(sys.executable),args,channel='a2a',intent='operation',
                              binary_sha256='0'*64)
        self.assertEqual(result.disposition,'REJECT_BINARY_MISMATCH')
