"""Injection tests for bounded local-process capture, portable Windows/Linux."""
from pathlib import Path
import sys
import threading
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from bounded_subprocess import run_bounded


class BoundedSubprocessTests(unittest.TestCase):
    def run_py(self, script, *, timeout=2.0, cap=65536):
        return run_bounded([sys.executable, '-c', script],
                           timeout_s=timeout, max_output=cap)

    def test_success_with_bounded_output(self):
        result = self.run_py("print('healthy')")
        self.assertEqual((result.returncode, result.stdout.strip(), result.reason),
                         (0, b'healthy', None))

    def test_exact_boundary_is_allowed(self):
        result = self.run_py('import sys;sys.stdout.write("x"*8192)', cap=8192)
        self.assertEqual((result.returncode, len(result.stdout)), (0, 8192))

    def test_one_byte_over_boundary_is_denied(self):
        result = self.run_py('import sys;sys.stdout.write("x"*8193)', cap=8192)
        self.assertEqual((result.reason, result.returncode, result.stdout),
                         ('output_limit', 125, b''))

    def test_flooding_stdout_is_killed(self):
        result = self.run_py('import os\nwhile True: os.write(1, b"x"*8192)', cap=8192)
        self.assertEqual(result.reason, 'output_limit')
        self.assertLess(result.elapsed_ms, 2000)

    def test_flooding_stderr_is_bounded_too(self):
        result = self.run_py('import os\nwhile True: os.write(2, b"x"*8192)', cap=8192)
        self.assertEqual(result.reason, 'output_limit')
        self.assertEqual(result.stdout, b'')

    def test_slow_trickle_cannot_reset_deadline(self):
        result = self.run_py('import sys,time\nwhile True: sys.stdout.write("x");sys.stdout.flush();time.sleep(0.02)', timeout=0.35)
        self.assertEqual(result.reason, 'timeout')
        self.assertLess(result.elapsed_ms, 2000)

    def test_blocked_process_is_killed(self):
        result = self.run_py('import time;time.sleep(30)', timeout=0.25)
        self.assertEqual((result.reason, result.stdout), ('timeout', b''))

    def test_native_nonzero_exit_is_preserved(self):
        result = self.run_py('import sys;print("denied");sys.exit(7)')
        self.assertEqual((result.returncode, result.stdout.strip(), result.reason),
                         (7, b'denied', None))

    def test_nonexistent_binary_denied_with_opaque_result(self):
        p = str(Path('__nonexistent_test_binary_0700__.exe'))
        result = run_bounded([p])
        self.assertEqual((result.returncode, result.stdout, result.reason),
                         (127, b'', 'spawn'))

    def test_invalid_limits_fail_before_spawn(self):
        for seconds, cap in [(0, 100), (61, 100), (1, 0), (1, 2**25)]:
            with self.subTest(seconds=seconds, cap=cap):
                with self.assertRaises(ValueError):
                    run_bounded([sys.executable], timeout_s=seconds, max_output=cap)

    def test_parallel_independent_invocations(self):
        results = []
        threads = [threading.Thread(target=lambda: results.append(self.run_py("print('ok')")))
                   for _ in range(6)]
        for thread in threads: thread.start()
        for thread in threads: thread.join()
        self.assertEqual(len(results), 6)
        self.assertTrue(all(result.stdout.strip() == b'ok' and result.reason is None
                            for result in results))


if __name__ == '__main__':
    unittest.main()
