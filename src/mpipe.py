# SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
#
# SPDX-License-Identifier: AGPL-3.0-or-later


"""
mpipe for chirp
===============

Send message-pack messages to subprocess.

Mini RPC mostly used for testinng using hypothesis.
"""

import ctypes
import os
import signal
import sys
import time
from contextlib import contextmanager
from subprocess import PIPE, Popen, TimeoutExpired

try:
    import umsgpack
except ImportError:
    import msgpack as umsgpack

mc = os.environ.get("MPP_MC")


@contextmanager
def open_and_close(args: list):
    """Open a subprocess for sending message-pack messages in a context.

    After the context it will send a close message: (0,).
    """
    proc = open(args)
    yield proc
    close(proc)


def open(args: list) -> Popen:
    """Open a subprocess for sending message-pack messages."""
    if os.environ.get("MPP_RR") == "True":
        proc = Popen(["rr"] + args, stdin=PIPE, stdout=PIPE)
    elif mc:
        proc = Popen(
            [
                "valgrind",
                "--tool=memcheck",
                "--leak-check=full",
                "--show-leak-kinds=all",
                "--errors-for-leak-kinds=all",
                "--error-exitcode=1",
                "--suppressions=%s" % mc,
            ]
            + args,
            stdin=PIPE,
            stdout=PIPE,
            preexec_fn=os.setsid,
        )
    else:
        proc = Popen(args, stdin=PIPE, stdout=PIPE, preexec_fn=os.setsid)
    proc._mpipe_last = None
    return proc


def close(proc: Popen):
    """Close the subprocess."""
    write(proc, (0,))
    try:
        if mc:
            proc.wait(6)
        else:
            proc.wait(3)
    except TimeoutExpired:
        # Kill process group because we sometimes attach valgrind or rr
        os.killpg(os.getpgid(proc.pid), signal.SIGINT)
        time.sleep(0.2)  # Allow the process to cleanup
        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        raise  # Its a bug when the process doesn't complete


def write(proc: Popen, data):
    """Write message to the process."""
    if proc._mpipe_last == "write":
        raise RuntimeError("Consecutive write not allowed in rpc_mode")
    proc._mpipe_last = "write"
    pack = umsgpack.dumps(data)
    size = bytes(ctypes.c_size_t(len(pack)))
    proc.stdin.write(size)
    proc.stdin.write(pack)
    proc.stdin.flush()


def read(proc: Popen):
    """Read message from the process, returns None on failure."""
    if proc._mpipe_last == "read":
        raise RuntimeError("Consecutive read not allowed in rpc_mode")
    proc._mpipe_last = "read"
    size = proc.stdout.read(ctypes.sizeof(ctypes.c_size_t))
    size = int.from_bytes(size, sys.byteorder)
    pack = proc.stdout.read(size)
    try:
        return umsgpack.loads(pack)
    except umsgpack.InsufficientDataException as e:
        if proc.poll() != 0:
            raise RuntimeError("The process returned %d." % proc.returncode) from e
        else:
            raise
