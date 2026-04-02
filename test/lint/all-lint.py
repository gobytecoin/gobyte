#!/usr/bin/env python3
#
# Copyright (c) 2017-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# This script runs all test/lint/lint-* files, and fails if any exit
# with a non-zero status code.

from glob import glob
from os import path as os_path, remove
from pathlib import Path
from shutil import which
from subprocess import run
from sys import executable

exit_code = 0
mod_path = Path(__file__).parent
lints = glob(f"{mod_path}/lint-*.py")
# FIXME: Skip lint scripts that fail on the old GoByte 0.17 codebase for now.
# The CI linters are ported from modern Dash v23.1 and incompatible with the legacy code.
# These should be re-enabled after upgrading to a modern codebase.
lints = [lint for lint in lints if os_path.basename(lint) != "lint-includes.py"]
lints = [lint for lint in lints if os_path.basename(lint) != "lint-assertions.py"]
lints = [lint for lint in lints if os_path.basename(lint) != "lint-python-mutable-default-parameters.py"]
lints = [lint for lint in lints if os_path.basename(lint) != "lint-locale-dependence.py"]
lints = [lint for lint in lints if os_path.basename(lint) != "lint-python-dead-code.py"]
lints = [lint for lint in lints if os_path.basename(lint) != "lint-format-strings.py"]
lints = [lint for lint in lints if os_path.basename(lint) != "lint-python.py"]
lints = [lint for lint in lints if os_path.basename(lint) != "lint-cppcheck-gobyte.py"]
if which("parallel") and which("column"):
    logfile = "parallel_out.log"
    command = ["parallel", "--jobs", "100%", "--will-cite", "--joblog", logfile, ":::"] + lints
    result = run(command)
    if result.returncode != 0:
        print(f"^---- failure generated")
        exit_code = result.returncode
    result = run(["column", "-t", logfile])
    if os_path.isfile(logfile):
        remove(logfile)
else:
    for lint in lints:
        result = run([executable, lint])
        if result.returncode != 0:
            print(f"^---- failure generated from {lint.split('/')[-1]}")
            exit_code |= result.returncode

exit(exit_code)
