#!/usr/bin/env python3
"""Draw rectangles at regular size increments for testing flood fill
"""

from dptxt import *

WIDTH = 2000
HEIGHT = 650
OFFSET = 10

def rect(x, y, w, h):
    return (
        DpMessage(1, 'penmove', (
            (x,y,1),
            (x+w, y,1),
            (x+w, y+h, 1),
            (x, y+h, 1),
            (x,y,1)
        )),
        DpMessage(1, 'penup', None),
   )

# Setup
msgs = [
    DpMessage(1, 'resize', {'right': WIDTH, 'bottom': HEIGHT}),
    DpMessage(1, 'newlayer', {'id': 1, 'fill': "#ffffff", 'title': "Background"}),
    DpMessage(1, 'brush', {'color': '#000000', 'layer': 1, 'mode': "inc", 'hard': 100})
]

# Draw rectangles

x = OFFSET
y = OFFSET
for i in range(1, 30):
    s = i * 10
    if y + s > (HEIGHT-OFFSET):
        y = OFFSET
        x += s

    msgs += rect(x, OFFSET + y, s, s)
    y += s + OFFSET

# Print out the recording
print('!version=dp:4.20.1')
for m in msgs:
    print (m)

