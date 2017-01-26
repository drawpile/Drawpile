#!/usr/bin/python3
"""
A tool for generating inline images for Drawpile recording templates.

Example:
1. First, generate the inline image: ./make-inline-image.py my_picture.png
2. Copy&paste the output into your template file.
3. Use with putimage:

    ? putimage layer=? x=? y=? w=64 h=64 {
        img=...
    }

    (Replace the ?s with appropriate values)

"""

from PIL import Image

import sys
import zlib
import base64
import textwrap
import struct

class EncodeError(Exception):
    pass

def encode_inline_image(img):
    out = ['? putimage layer=? x=? y=? w=%d h=%d {' % (img.width, img.height)]

    img = img.convert(mode="RGBA")

    imgbytes = img.tobytes(encoder_name='raw')
    data = zlib.compress(imgbytes, 9)

    if len(data) > (2**16 - 19 - 4):
        raise EncodeError("Image does not fit into a single message!")

    data = struct.pack('>I', len(imgbytes)) + data

    out += ['\timg=' + x for x in textwrap.wrap(base64.b64encode(data).decode('ascii'), 70)]

    out.append('}')

    return '\n'.join(out)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print ("Usage: make-inlineimage.py <image file>", file=sys.stderr)
        sys.exit(1)

    img = Image.open(sys.argv[1])

    try:
        # TODO split into multiple images if one doesn't fit into a message
        print (encode_inline_image(img))
    except EncodeError as e:
        print (e, file=sys.stderr)
        sys.exit(1)

