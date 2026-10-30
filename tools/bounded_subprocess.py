"""Bounded, deadline-aware stdout/stderr capture for local test executables.

This is a release-test guard, not a sandbox or OS-level memory/CPU quota.
Stderr is intentionally merged with stdout so it cannot block independently.
"""
from __future__ import annotations

from dataclasses import dataclass
import subprocess
import threading
import time
from collections.abc import Sequence


@dataclass(frozen=True)
class BoundedResult:
    returncode: int
    stdout: bytes
    elapsed_ms: float
    reason: str | None


def run_bounded(argv: Sequence[str], *, timeout_s: float = 5.0,
                max_output: int = 65_536) -> BoundedResult:
    """Capture no more than max_output bytes, killing the direct child if exceeded.

    No shell is invoked. Errors return opaque reason codes rather than untrusted
    executable output. Does not guarantee termination of descendant processes.
    """
    if not argv or not (0 < timeout_s <= 60) or not (0 < max_output <= 1_048_576):
        raise ValueError('invalid_process_limits')
    start = time.monotonic()
    try:
        process = subprocess.Popen(
            list(argv), stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, shell=False,
        )
    except OSError:
        return BoundedResult(127, b'', (time.monotonic() - start) * 1000, 'spawn')

    chunks = bytearray()
    exceeded = threading.Event()

    def drain() -> None:
        assert process.stdout is not None
        try:
            while True:
                block = process.stdout.read(4096)
                if not block:
                    break
                if len(chunks) + len(block) > max_output:
                    exceeded.set()
                    try:
                        process.kill()
                    except OSError:
                        pass  # It may have exited while the pipe was draining.
                    break
                chunks.extend(block)
        except (OSError, ValueError):
            exceeded.set()

    worker = threading.Thread(target=drain, daemon=True, name='bounded-output-reader')
    worker.start()
    timed_out = False
    try:
        process.wait(timeout=timeout_s)
    except subprocess.TimeoutExpired:
        timed_out = True
        try:
            process.kill()
        except OSError:
            pass
        try:
            process.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            pass
    worker.join(timeout=2.0)
    if not worker.is_alive() and process.stdout is not None:
        process.stdout.close()
    duration_ms = (time.monotonic() - start) * 1000
    if exceeded.is_set():
        return BoundedResult(125, b'', duration_ms, 'output_limit')
    if timed_out or worker.is_alive() or process.poll() is None:
        return BoundedResult(124, b'', duration_ms, 'timeout')
    return BoundedResult(int(process.returncode), bytes(chunks), duration_ms, None)
