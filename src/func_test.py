"""Hypothesis based functional chirp test."""

from hypothesis import settings, unlimited  # noqa
from hypothesis.strategies import tuples, sampled_from, just, lists, binary
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


class GenFunc(GenericStateMachine):
    """Test if the stays consistent."""

    def __init__(self):
        self.etest_ready = False
        self.echo_ready = False
        self.timeout_open = False
        self.init_etest_step = tuples(
            just("init_etest"), sampled_from(('0', '1'))
        )
        self.init_echo_step = tuples(just("init_echo"), just(0))
        self.x42_step = tuples(just("42"), just(0))
        self.check_step = tuples(just("check_messages"), just(0))
        self.send_message_step = tuples(
            just("send_message"),
            tuples(
                sampled_from((socket.AF_INET, socket.AF_INET6)),
                just(2997),
            )
        )
        self.send_message_step_bad_port = tuples(
            just("send_message_bad_port"),
            tuples(
                sampled_from((socket.AF_INET, socket.AF_INET6)),
                sampled_from((7, 2991)),
            )
        )
        self.fuzz_main_port_step = tuples(
            just("fuzz_main_port"), lists(binary(), max_size=4)
        )

    def listen_dead_socket(self):
        dead = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.dead = dead
        dead.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        dead.bind((socket.gethostname(), 2991))
        dead.listen(5)

    def init_echo(self):
        self.echo_ready = True
        args = ["./src/echo_etest", "2997", self.enc]
        self.echo = Popen(args, stdin=PIPE, stdout=PIPE)
        time.sleep(0.1)
        check = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        check.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        connected = False
        count = 0
        while not connected:
            try:
                check.connect(("127.0.0.1", 2997))
                connected = True
            except ConnectionRefusedError:
                count += 1
                if count > 4:
                    raise
                else:
                    time.sleep(0.2)
        check.close()
        # Make sure echo etest does not fail
        self.echo.poll()
        assert self.echo.returncode is None

    def init_etest(self):
        self.listen_dead_socket()
        self.etest_ready = True
        self.proc = mpipe.open(["./src/func_etest", "2998", self.enc])
        self.open_messages = set()

    def teardown(self):
        if self.etest_ready:
            self.dead.close()
            self.dead = None
            ret = 1
            proc = self.proc
            try:
                try:
                    self.check_messages()
                    mpipe.write(proc, (func_cleanup_e, 0))
                    ret = mpipe.read(proc)[0]
                finally:
                    mpipe.close(proc)
            finally:
                try:
                    if self.echo_ready:
                        echo = self.echo
                        close(echo)
                finally:
                    self.proc = None
                    self.echo = None
                    assert ret == 0
                    if self.echo_ready:
                        assert echo.returncode == 0
                    assert proc.returncode == 0

    def steps(self):
        steps = self.x42_step | self.send_message_step | self.check_step
        if not self.etest_ready:
            return self.init_etest_step
        if not self.echo_ready:
            # If echo is not initialized and a timeout is open we cannot send
            # another message
            if self.timeout_open:
                return (
                    self.x42_step | self.check_step | self.init_echo_step |
                    self.fuzz_main_port_step
                )
            else:
                steps = steps | self.init_echo_step | self.fuzz_main_port_step
        if not self.timeout_open:
            steps = steps | self.send_message_step_bad_port
        return steps

    def execute_step(self, step):
        def send_message():
            mpipe.write(
                self.proc,
                (func_send_message_e, value[0], value[1], int(self.echo_ready))
            )
            msg_id, ret = mpipe.read(self.proc)
            assert msg_id not in self.open_messages
            assert ret == 0
            self.open_messages.add(msg_id)
        action, value = step
        if action == 'init_etest':
            self.enc = str(value)
            self.init_etest()
        elif action == 'init_echo':
            self.init_echo()
        elif action == 'fuzz_main_port':
            self.fuzz_main_port(value)
        elif action == '42':
            mpipe.write(self.proc, (func_42_e, ))
            assert mpipe.read(self.proc) == [42]
        elif action == 'send_message':
            if not self.echo_ready:
                self.timeout_open = True
            send_message()
        elif action == 'send_message_bad_port':
            self.timeout_open = True
            send_message()
        elif action == 'check_messages':
            self.check_messages()
        else:
            assert False, "Unknown step"

    def fuzz_main_port(self, data):
        fuzz = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        fuzz.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        fuzz.bind((socket.gethostname(), 2997))
        connected = False
        count = 0
        while not connected:
            try:
                fuzz.connect(("127.0.0.1", 2998))
                connected = True
            except ConnectionRefusedError:
                count += 1
                if count > 4:
                    raise
                else:
                    time.sleep(0.2)
        try:
            for msg in data:
                fuzz.send(msg)
                time.sleep(0.1)
        except BrokenPipeError:
            pass
        fuzz.close()

    def check_messages(self):
        mpipe.write(self.proc, (func_check_messages_e, ))
        ret = mpipe.read(self.proc)
        self.open_messages -= set(ret)
        assert not self.open_messages
        self.timeout_open = False

# with settings(max_examples=10):


with settings(deadline=None, timeout=unlimited):
    TestFunc = GenFunc.TestCase
