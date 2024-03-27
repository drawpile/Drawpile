#!/usr/bin/env python
# SPDX-License-Identifier: MIT
# Packs all of Drawpile's assets into a single bundle file so that downloading
# them in the browser version doesn't open a new connection for each tiny file.
# Code formatted with Black.
import os
import struct
import sys

assets_dir = os.path.abspath(sys.argv[1])
output_path = os.path.abspath(sys.argv[2])
rel_dir = os.path.dirname(assets_dir)


def slurp_bytes(path):
    with open(path, "rb") as fh:
        return fh.read()


with open(output_path, "wb") as fh:
    for root, dirs, files in os.walk(assets_dir, followlinks=True):
        for file in files:
            path = os.path.join(root, file)
            name_bytes = ("/" + os.path.relpath(path, rel_dir)).encode("UTF-8")
            fh.write(struct.pack("<I", len(name_bytes)))
            fh.write(name_bytes)
            content_bytes = slurp_bytes(path)
            fh.write(struct.pack("<I", len(content_bytes)))
            fh.write(content_bytes)
