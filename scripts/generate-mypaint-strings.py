#!/usr/bin/env python
# Copyright (c) 2023 askmeaboutloom
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
import json
import sys


def generate_function(
    function_name, param_name, disambiguation, key, comment_key, values, default_return
):
    print(f"QString BrushSettingsDialog::{function_name}(int {param_name})")
    print("{")
    print(f"\tswitch({param_name}) {{")
    for i, value in enumerate(values):
        print(f"\tcase {i}:")
        comment = value.get(comment_key)
        if comment:
            for comment_line in comment.splitlines():
                print(f"\t\t//: {comment_line}")
        print(f"\t\treturn tr({json.dumps(value[key])}, {json.dumps(disambiguation)});")
    print("\tdefault:")
    print(f"\t\treturn {default_return};")
    print("\t}")
    print("}")
    print()


if len(sys.argv) != 2:
    progname = __file__ if len(sys.argv) < 1 else sys.argv[0]
    sys.stderr.write(f"Usage: {progname} MYPAINT_BRUSHSETTINGS_JSON_FILE\n")
    sys.exit(2)

with open(sys.argv[1]) as f:
    brushsettings = json.load(f)

print("// This file is automatically generated by scripts/generate-mypaint-strings.py")
print("// Do not edit it manually, run the script again instead to regenerate it.")
print()
print('#include "brushsettingsdialog.h"')
print()
print("namespace dialogs {")
print()

generate_function(
    "getMyPaintInputTitle",
    "input",
    "mypaintinput",
    "displayed_name",
    "tcomment_name",
    brushsettings["inputs"],
    'tr("Unknown Input", "mypaintinput")',
)

generate_function(
    "getMyPaintInputDescription",
    "input",
    "mypaintinput",
    "tooltip",
    "tcomment_tooltip",
    brushsettings["inputs"],
    "QString{}",
)

generate_function(
    "getMyPaintSettingTitle",
    "setting",
    "mypaintsetting",
    "displayed_name",
    "tcomment_name",
    brushsettings["settings"],
    'tr("Unknown Setting", "mypaintsetting")',
)

generate_function(
    "getMyPaintSettingDescription",
    "setting",
    "mypaintsetting",
    "tooltip",
    "tcomment_tooltip",
    brushsettings["settings"],
    "QString{}",
)

print()
print("}")
