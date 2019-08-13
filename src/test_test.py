
# SPDX-FileCopyrightText: 2019 Jean-Louis Fuchs <ganwell@fangorn.ch>
#
# SPDX-License-Identifier: AGPL-3.0-or-later


"""Common testing functions."""


def fix_func_enum(globs):
    """Remove tuples from func_* globals."""
    for glob in globs.keys():
        if glob.startswith("func_"):
            globs[glob] = globs[glob][0]
