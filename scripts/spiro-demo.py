#!/usr/bin/env python3
"""Drawpile recording generation demo
"""

from math import sin, cos, pi
from fractions import gcd
from dptxt import *

SIZE=450

def M(ctx, name, **kwargs):
    return DpMessage(ctx, name, kwargs)

def P(ctx, *points):
    return DpMessage(ctx, 'penmove', points)

def hypotrochoid(x, y, a, b, h, offset, ctx, color):
    yield M(ctx, 'brush', color=color, layer=1, size=3, mode="inc,soft",
            hard=100, blend=9)

    offset = offset / 180 * pi

    for i in range(0, 360 * int(b/gcd(a,b) + 0.5), 3):
        theta = i / 180 * pi
        yield P(ctx, (
            x + (a-b)*cos(theta)+h*cos((a-b)/b*(theta+offset)),
            y + (a-b)*sin(theta)-h*sin((a-b)/b*(theta+offset)),
            1))
    yield M(ctx, 'penup')

# Canvas setup
msgs = [
    M(1, 'resize', right=SIZE, bottom=SIZE),
    M(1, 'newlayer', id=1, fill="#333333", title="Background"),
]

# Drawing
for m in zip(
        hypotrochoid(SIZE/2, SIZE/2, a=150, b=90, h=150, ctx=1, offset=0, color="#ff0000"),
        hypotrochoid(SIZE/2, SIZE/2, a=150, b=90, h=150, ctx=2, offset=45, color="#00ff00"),
        hypotrochoid(SIZE/2, SIZE/2, a=150, b=90, h=150, ctx=3, offset=90, color="#0000ff"),
        ):
    msgs += m

# Print out the recording
print('!version=dp:4.20.1')
for m in msgs:
    print (m)

