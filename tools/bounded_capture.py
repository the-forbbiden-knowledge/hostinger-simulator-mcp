"""Bounded native-fixture capture: no shell and no live network calls.

This reduces in-memory capture and limits elapsed time for ordinary child process
families. It does NOT provide a complete malicious-process sandbox, OS-level
memory cap, or containment for deliberately detached descendants.
"""
from __future__ import annotations

from dataclasses import dataclass
import os
import signal
import subprocess
import threading
import time
from typing import Sequence


@dataclass(frozen=True)
class Capture:
    stdout: bytes
    returncode: int
    failure: str | None
    stderr_bytes: int


def _terminate(process: subprocess.Popen[bytes]) -> None:
    if os.name == 'nt':
        try:
            subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'],
                           stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, timeout=2, check=False)
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


def capture(argv: Sequence[str], *, timeout_s: float = 5.0,
            stdout_limit: int = 65536, stderr_limit: int = 8192) -> Capture:
    if (not argv or not (0 < timeout_s <= 60)
            or not (0 < stdout_limit <= 1048576)
            or not (0 < stderr_limit <= 1048576)):
        raise ValueError('invalid_capture_bounds')
    if any(not isinstance(part, str) or '\0' in part for part in argv):
        raise ValueError('invalid_argv')
    try:
        process: subprocess.Popen[bytes] = subprocess.Popen(
            list(argv), stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            shell=False, bufsize=0, start_new_session=(os.name != 'nt'),
            creationflags=(subprocess.CREATE_NEW_PROCESS_GROUP if os.name == 'nt' else 0),
        )
    except OSError:
        return Capture(b'', 127, 'spawn', 0)
    buffers: dict[str, bytearray] = {'stdout': bytearray(), 'stderr': bytearray()}
    lock = threading.Lock()
    exceeded: list[str] = []

    def drain(name: str, limit: int) -> None:
        stream = getattr(process, name)
        assert stream is not None
        try:
            while True:
                block = stream.read(4096)
                if not block:
                    break
                with lock:
                    if len(buffers[name]) + len(block) > limit:
                        exceeded.append(name)
                        break
                    buffers[name].extend(block)
                if exceeded:
                    break
        except (OSError, ValueError):
            with lock:
                exceeded.append(name)
        if exceeded:
            _terminate(process)

    readers = [
        threading.Thread(target=drain, args=('stdout', stdout_limit), daemon=True),
        threading.Thread(target=drain, args=('stderr', stderr_limit), daemon=True),
    ]
    for worker in readers:
        worker.start()
    timed_out = False
    try:
        process.wait(timeout=timeout_s)
    except subprocess.TimeoutExpired:
        timed_out = True
        _terminate(process)
    try:
        process.wait(timeout=2)
    except subprocess.TimeoutExpired:
        timed_out = True
        _terminate(process)
    for worker in readers:
        worker.join(timeout=2)
    if any(worker.is_alive() for worker in readers):
        timed_out = True
        _terminate(process)
    for stream in (process.stdout, process.stderr):
        if stream is not None and not any(worker.is_alive() for worker in readers):
            stream.close()
    if exceeded:
        return Capture(b'', 125, 'output_limit' if 'stdout' in exceeded else 'stderr_limit',
                       len(buffers['stderr']))
    if timed_out or process.poll() is None:
        return Capture(b'', 124, 'timeout', len(buffers['stderr']))
    return Capture(bytes(buffers['stdout']), int(process.returncode), None,
                   len(buffers['stderr']))
