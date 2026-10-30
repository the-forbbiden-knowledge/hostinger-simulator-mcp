"""Bounded, deadline-aware local process capture with child-tree termination.

A release-test guard, not an OS quota, sandbox, or defense against processes
that deliberately escape their process group. It writes no customer data.
"""
from __future__ import annotations

from collections.abc import Sequence
from dataclasses import dataclass
import os
import signal
import subprocess
import threading
import time


@dataclass(frozen=True)
class BoundedResult:
    returncode: int
    stdout: bytes
    elapsed_ms: float
    reason: str | None


def _kill_process_tree(process: subprocess.Popen) -> None:
    """Kill the process's own group/tree; never signal the caller's group."""
    if os.name == 'nt':
        try:
            subprocess.run(
                ['taskkill', '/PID', str(process.pid), '/T', '/F'],
                stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL, check=False, timeout=2,
            )
        except (OSError, subprocess.TimeoutExpired):
            pass
    else:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    try:
        process.kill()
    except (OSError, ProcessLookupError):
        pass


def run_bounded(argv: Sequence[str], *, timeout_s: float = 5.0,
                max_output: int = 65_536) -> BoundedResult:
    """Bound output and elapsed time, stopping ordinary direct descendants.

    Programs may escape process groups or create children after an early
    parent exit; neither OS-wide process containment nor CPU/RAM quota is
    implied. No shell execution occurs and untrusted error output is omitted.
    """
    if not argv or not (0 < timeout_s <= 60) or not (0 < max_output <= 1_048_576):
        raise ValueError('invalid_process_limits')
    started = time.monotonic()
    try:
        process = subprocess.Popen(
            list(argv), stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, shell=False,
            start_new_session=(os.name != 'nt'),
            creationflags=(subprocess.CREATE_NEW_PROCESS_GROUP if os.name == 'nt' else 0),
        )
    except OSError:
        return BoundedResult(127, b'', (time.monotonic() - started) * 1000, 'spawn')

    chunks = bytearray()
    exceeded = threading.Event()

    def drain() -> None:
        assert process.stdout is not None
        try:
            while True:
                block = process.stdout.read1(4096)
                if not block:
                    break
                if len(chunks) + len(block) > max_output:
                    exceeded.set()
                    _kill_process_tree(process)
                    break
                chunks.extend(block)
        except (OSError, ValueError):
            exceeded.set()
            _kill_process_tree(process)

    worker = threading.Thread(target=drain, daemon=True, name='bounded-output-reader')
    worker.start()
    timed_out = False
    try:
        process.wait(timeout=timeout_s)
    except subprocess.TimeoutExpired:
        timed_out = True
        _kill_process_tree(process)
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            pass
    worker.join(timeout=2)
    if worker.is_alive():
        _kill_process_tree(process)
        worker.join(timeout=1)
    if not worker.is_alive() and process.stdout is not None:
        process.stdout.close()
    elapsed = (time.monotonic() - started) * 1000
    if exceeded.is_set():
        return BoundedResult(125, b'', elapsed, 'output_limit')
    if timed_out or worker.is_alive() or process.poll() is None:
        return BoundedResult(124, b'', elapsed, 'timeout')
    return BoundedResult(int(process.returncode), bytes(chunks), elapsed, None)
