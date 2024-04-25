#!/usr/bin/env python3

import sys
import subprocess
import os.path

name, *args = sys.argv

USAGE = f"""
Usage: {name} <file.vsl>

For each occurance of a VSL comment block starting with
//TESTCASE: <args>
The corresponding compiler executable file.out is executed with the given <args>.
Output is compared against the rest of the comment block.
If they are different, the difference is printed and the test fails.
""".strip()

TESTCASE_LINE = "//TESTCASE:"

def error(text, message=None):
    print(f"{name}: error: {text}")
    if message is not None:
        print(message)
    sys.exit(1)

if len(args) != 1:
    error("expected one input .vsl file", message=USAGE)

vsl_file, = args

if not os.path.isfile(vsl_file):
    error(f"file not found: {vsl_file}")

out_file = vsl_file[:vsl_file.rindex(".")] + ".out"

if not os.path.isfile(out_file):
    error(f"file not found: {out_file}")

with open(vsl_file, "r", encoding="utf-8") as vsl_fd:
    lines = vsl_fd.read().splitlines()

tests = []

i = 0
while i < len(lines):
    line = lines[i]
    if line.startswith(TESTCASE_LINE):
        args = line[len(TESTCASE_LINE):].split(" ")
        args = [arg for arg in args if len(arg)]

        expected_output = []
        i += 1
        while i < len(lines) and lines[i].startswith("//") and not lines[i].startswith(TESTCASE_LINE):
            expected_output.append(lines[i][2:])
            i += 1

        tests.append((args, expected_output))
    else:
        i += 1

print(f"Running {len(tests)} test cases for file {vsl_file}")

for args, expected_output in tests:
    print(f"  Running {out_file} {' '.join(args)}")
    proc = subprocess.run([out_file] + args, capture_output=True, text=True, check=False, timeout=5)
    result_lines = proc.stdout.strip().split('\n')

    if len(result_lines) != len(expected_output) or any(a != b for a, b in zip(result_lines, expected_output)):
        message = ["EXPECTED --------",
                   "\n".join(expected_output),
                   "ACTUAL ----------",
                   "\n".join(result_lines),
                   "-----------------"]
        error("actual output didn't match expected output", message="\n".join(message))
