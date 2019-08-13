
# SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
#
# SPDX-License-Identifier: AGPL-3.0-or-later


"""Test chirp buffers."""
import mpipe

from hypothesis.strategies import tuples, sampled_from, just, integers
from hypothesis.stateful import GenericStateMachine

func_init_e = 1
func_acquire_e = 2
func_release_e = 3
func_cleanup_e = 4


class GenBuffer(GenericStateMachine):
    """Test if the stays consistent."""

    def __init__(self):
        self.reinit()
        self.acquired = set()
        self.size = 0

        self.init_step = tuples(
            just("init"),
            integers(
                min_value=1,
                max_value=32
            )
        )
        self.acquire_step = tuples(just("acquire"), just(0))

    def reinit(self):
        self.proc = mpipe.open(["src/buffer_etest"])
        self.initialized = False

    def teardown(self):
        if self.initialized:
            mpipe.write(self.proc, (func_cleanup_e, 0))
            assert mpipe.read(self.proc) == [0]
        mpipe.close(self.proc)
        self.proc = None  # Hypothesis seems to keep GSM objects

    def steps(self):
        if not self.initialized:
            self.initialized = True
            return self.init_step
        else:
            if self.acquired:
                release_step = tuples(just(
                    "release"
                ), sampled_from(sorted(self.acquired)))
            else:
                release_step = tuples(just(
                    "release"
                ), integers(min_value=0, max_value=self.size - 1))
            return release_step | self.acquire_step

    def execute_step(self, step):
        action, value = step
        if action == 'init':
            self.size = value
            mpipe.write(
                self.proc,
                (func_init_e, self.size)
            )
            assert mpipe.read(self.proc) == [0]
        elif action == 'acquire':
            mpipe.write(
                self.proc,
                (func_acquire_e, 0)
            )
            id_ = mpipe.read(self.proc)[0]
            if len(self.acquired) > self.size - 1:
                assert id_ == -1
            if id_ != -1:
                self.acquired.add(id_)
        elif action == 'release':
            if value in self.acquired:
                mpipe.write(
                    self.proc,
                    (func_release_e, value)
                )
                assert mpipe.read(self.proc) == [0]
                self.acquired.remove(value)
            else:
                try:
                    mpipe.write(
                        self.proc,
                        (func_release_e, value)
                    )
                    assert mpipe.read(self.proc) == [0]
                except Exception:
                    self.reinit()
                else:
                    assert False, "Test binary has to assert"
        else:
            assert False, "Unknown step"


TestBuffer = GenBuffer.TestCase
