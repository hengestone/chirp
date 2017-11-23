"""Hypothesis based functional chirp test."""

from hypothesis import settings  # noqa
from hypothesis.strategies import tuples, sampled_from, just
from hypothesis.stateful import GenericStateMachine
import mpipe
import socket
from subprocess import Popen, PIPE, TimeoutExpired
import time

func_42_e             = 1
func_cleanup_e        = 2
func_send_message_e   = 3
func_check_messages_e = 4


def close(proc : Popen):
    """Close the subprocess."""
    try:
        proc.terminate()
        proc.wait(1)
    except TimeoutExpired:
        print("Doing kill")
        proc.kill()
        raise  # Its a bug when the process doesn't complete


class GenBuffer(GenericStateMachine):
    """Test if the stays consistent."""

    def __init__(self):
        self.initialzed = False
        self.init_step = tuples(just("init"), sampled_from(('0', '1')))
        self.x42_step = tuples(just("42"), just(0))
        self.check_step = tuples(just("check_messages"), just(0))
        self.send_message_step = tuples(
            just("send_message"),
            tuples(
                sampled_from((socket.AF_INET, socket.AF_INET6)),
                just(2997),
            )
        )

    def reinit(self, enc):
        args = ["./src/echo_etest", "2997", enc]
        self.echo = Popen(args, stdin=PIPE, stdout=PIPE)
        time.sleep(0.1)  # Wait for echo to be ready
        self.proc = mpipe.open(["./src/func_etest", "2998", enc])
        self.initialzed = True
        self.open_messages = set()

    def teardown(self):
        if self.initialzed:
            ret = 1
            proc = self.proc
            echo = self.echo
            try:
                try:
                    self.check_messages()
                    mpipe.write(proc, (func_cleanup_e, 0))
                    ret = mpipe.read(proc)[0]
                finally:
                    mpipe.close(proc)
            finally:
                try:
                    close(echo)
                finally:
                    self.proc = None
                    self.echo = None
                    assert ret == 0
                    assert echo.returncode == 0
                    assert proc.returncode == 0

    def steps(self):
        if not self.initialzed:
            return self.init_step
        return self.x42_step | self.send_message_step | self.check_step

    def execute_step(self, step):
        action, value = step
        if action == 'init':
            self.reinit(str(value))
        elif action == '42':
            mpipe.write(self.proc, (func_42_e, ))
            assert mpipe.read(self.proc) == [42]
        elif action == 'send_message':
            mpipe.write(
                self.proc,
                (func_send_message_e, value[0], value[1])
            )
            msg_id, ret = mpipe.read(self.proc)
            assert msg_id not in self.open_messages
            assert ret == 0
            self.open_messages.add(msg_id)
        elif action == 'check_messages':
            self.check_messages()
        else:
            assert False, "Unknown step"

    def check_messages(self):
        mpipe.write(self.proc, (func_check_messages_e, ))
        ret = mpipe.read(self.proc)
        self.open_messages -= set(ret)
        assert not self.open_messages

# with settings(max_examples=10):


TestBuffer = GenBuffer.TestCase
