"" # noqa
"""
mpipe 0.6
=========

Send message-pack messages to subprocess.

Mini RPC mostly used for testinng using hypothesis.
"""

import ctypes
from subprocess import Popen, PIPE, TimeoutExpired
import sys
import os
import signal
import time
try:
    import umsgpack
except ImportError:
    import msgpack as umsgpack
from contextlib import contextmanager

mc = os.environ.get("MPP_MC")


@contextmanager
def open_and_close(args : list):
    """Open a subprocess for sending message-pack messages in a context.

    After the context it will send a close message: (0,).
    """
    proc = open(args)
    yield proc
    close(proc)


def open(args : list) -> Popen:
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
            ] + args,
            stdin=PIPE,
            stdout=PIPE,
            preexec_fn=os.setsid
        )
    else:
        proc = Popen(args, stdin=PIPE, stdout=PIPE, preexec_fn=os.setsid)
    proc._mpipe_last = None
    return proc


def close(proc : Popen):
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


def write(proc : Popen, data):
    """Write message to the process."""
    if proc._mpipe_last == "write":
        raise RuntimeError("Consecutive write not allowed in rpc_mode")
    proc._mpipe_last = "write"
    pack = umsgpack.dumps(data)
    size = bytes(ctypes.c_size_t(len(pack)))
    proc.stdin.write(size)
    proc.stdin.write(pack)
    proc.stdin.flush()


def read(proc : Popen):
    """Read message from the process, returns None on failure."""
    if proc._mpipe_last == "read":
        raise RuntimeError("Consecutive read not allowed in rpc_mode")
    proc._mpipe_last = "read"
    size = proc.stdout.read(ctypes.sizeof(ctypes.c_size_t))
    size = int.from_bytes(size,  sys.byteorder)
    pack = proc.stdout.read(size)
    try:
        return umsgpack.loads(pack)
    except umsgpack.InsufficientDataException as e:
        if proc.poll() != 0:
            raise RuntimeError(
                "The process returned %d." % proc.returncode
            ) from e
        else:
            raise


# MIT License
#
# Copyright (c) 2017 Jean-Louis Fuchs
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
