#!/usr/bin/env python3
from configparser import ConfigParser
from enum import Enum
from math import fmod
import re
import sys

if len(sys.argv) != 2:
    prog = __file__ if len(sys.argv) == 0 else sys.argv[0]
    raise ValueError(f"Usage: {prog} KRITA_COLOR_FILE >DRAWPILE_COLOR_FILE")

src = ConfigParser()
src.read(sys.argv[1])

rgb_re = re.compile(r"^\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*$")

def get_srgb(value):
    match = rgb_re.search(value)
    if match:
        r = int(match.group(1))
        g = int(match.group(2))
        b = int(match.group(3))
        return (r, g, b)
    else:
        raise ValueError(f"Can't parse rgb value '{rgb}'")

def srgb_to_hex(srgb):
    (r, g, b) = srgb
    return f"#{r:02x}{g:02x}{b:02x}"

def normalize(x):
    return max(0.0, min(1.0, x))

# Converted from KColorSpaces, MIT license
class HCY:
    yc = { 'r': 0.34375, 'g': 0.5, 'b': 0.15625 }
    @staticmethod
    def wrap(a, d = 1.0):
        r = fmod(a, d)
        return d + r if r < 0.0 else max(r, 0.0)

    @staticmethod
    def gamma(n):
        return n ** 2.2

    @staticmethod
    def igamma(n):
        return n ** (1.0 / 2.2)

    @staticmethod
    def luma(r, g, b):
        return r * HCY.yc['r'] + g * HCY.yc['g'] + b * HCY.yc['b']

    def __init__(self, srgb):
        r = HCY.gamma(srgb[0] / 255.0)
        g = HCY.gamma(srgb[1] / 255.0)
        b = HCY.gamma(srgb[2] / 255.0)

        # luma component
        y = HCY.luma(r, g, b)

        # hue component
        p = max(r, g, b)
        n = min(r, g, b)
        d = 6.0 * (p - n)

        if n == p:
            h = 0.0
        elif r == p:
            h = ((g - b) / d)
        elif g == p:
            h = ((b - r) / d) + (1.0 / 3.0)
        else:
            h = ((r - g) / d) + (2.0 / 3.0)

        # chroma component
        if y == 0.0 or y == 1.0:
            c = 0.0
        else:
            c = max((y - n) / y, (p - y) / (1 - y))

        self.h = h
        self.c = c
        self.y = y

    def to_srgb(self):
        # start with sane component values
        h = HCY.wrap(self.h)
        c = normalize(self.c)
        y = normalize(self.y)
        hs = h * 6.0

        # calculate some needed variables
        if hs < 1.0:
            th = hs
            tm = HCY.yc['r'] + HCY.yc['g'] * th
        elif hs < 2.0:
            th = 2.0 - hs
            tm = HCY.yc['g'] + HCY.yc['r'] * th
        elif hs < 3.0:
            th = hs - 2.0
            tm = HCY.yc['g'] + HCY.yc['b'] * th
        elif hs < 4.0:
            th = 4.0 - hs
            tm = HCY.yc['b'] + HCY.yc['g'] * th
        elif hs < 5.0:
            th = hs - 4.0
            tm = HCY.yc['b'] + HCY.yc['r'] * th
        else:
            th = 6.0 - hs
            tm = HCY.yc['r'] + HCY.yc['b'] * th

        # calculate RGB channels in sorted order
        if tm >= y:
            tp = y + y * c * (1.0 - tm) / tm
            to = y + y * c * (th - tm) / tm
            tn = y - (y * c)
        else:
            tp = y + (1.0 - y) * c
            to = y + (1.0 - y) * c * (th - tm) / (1.0 - tm)
            tn = y - (1.0 - y) * c * tm / (1.0 - tm)

        # return RGB channels in appropriate order
        if hs < 1.0:
            (r, g, b) = (HCY.igamma(tp), HCY.igamma(to), HCY.igamma(tn))
        elif hs < 2.0:
            (r, g, b) = (HCY.igamma(to), HCY.igamma(tp), HCY.igamma(tn))
        elif hs < 3.0:
            (r, g, b) = (HCY.igamma(tn), HCY.igamma(tp), HCY.igamma(to))
        elif hs < 4.0:
            (r, g, b) = (HCY.igamma(tn), HCY.igamma(to), HCY.igamma(tp))
        elif hs < 5.0:
            (r, g, b) = (HCY.igamma(to), HCY.igamma(tn), HCY.igamma(tp))
        else:
            (r, g, b) = (HCY.igamma(tp), HCY.igamma(tn), HCY.igamma(to))

        return (int(r * 255), int(g * 255), int(b * 255))

# Converted from KColorUtils, MIT license
def mix_channel(a, b, bias):
    return a + (b - a) * bias

# Converted from KColorUtils, MIT license
def mix(c1, c2, bias):
    if bias <= 0.0:
        return c1
    elif bias >= 1.0:
        return c2

    return (
        int(mix_channel(c1[0] / 255.0, c2[0] / 255.0, bias) * 255),
        int(mix_channel(c1[1] / 255.0, c2[1] / 255.0, bias) * 255),
        int(mix_channel(c1[2] / 255.0, c2[2] / 255.0, bias) * 255)
    )

# Converted from KColorUtils, MIT license
def tint_helper(base, color, amount):
    result = HCY(mix(base, color, amount ** 0.3))
    result.y = mix_channel(HCY(base).y, result.y, amount)
    return result.to_srgb()

# Converted from KColorUtils, MIT license
def contrast_ratio(c1, c2):
    y1 = HCY(c1).y
    y2 = HCY(c2).y
    if y1 > y2:
        return (y1 + 0.05) / (y2 + 0.05)
    else:
        return (y2 + 0.05) / (y1 + 0.05)

# Converted from KColorUtils, MIT license
def tint(base, color, amount):
    if amount <= 0.0:
        return base
    elif amount >= 1.0:
        return color

    ri = contrast_ratio(base, color)
    rg = 1.0 + ((ri + 1.0) * amount * amount * amount)
    u = 1.0
    l = 0.0
    for i in range(12):
        a = 0.5 * (l + u)
        result = tint_helper(base, color, a)
        ra = contrast_ratio(base, result)
        if ra > rg:
            u = a
        else:
            l = a
    return result

# Converted from KColorUtils, MIT license
def darken(srgb, ky = 0.5, kc = 1.0):
    c = HCY(srgb)
    c.y = normalize(c.y * (1.0 - ky))
    c.c = normalize(c.c * kc)
    return c.to_srgb()

# Converted from KColorUtils, MIT license
def shade(srgb, ky, kc = 0.0):
    c = HCY(srgb)
    c.y = normalize(c.y + ky)
    c.c = normalize(c.c + kc)
    return c.to_srgb()

default_contrast = int(src["KDE"]["contrast"]) / 10.0

# Converted from Krita KColorScheme, LGPL-2.0-or-later
KritaShade = Enum('KritaShade', [ 'LIGHT', 'MIDLIGHT', 'MID', 'DARK', 'SHADOW'])
def krita_shade(srgb, role, contrast = default_contrast, chroma_adjust = 0.0):
    contrast = max(-1.0, min(1.0, contrast))
    y = HCY(srgb).y
    yi = 1.0 - y

    # handle very dark colors (base, mid, dark, shadow == midlight, light)
    if y < 0.006:
        if role == KritaShade.LIGHT:
            return shade(srgb, 0.05 * 0.95 * contrast, chroma_adjust)
        elif role == KritaShade.MID:
            return shade(srgb, 0.01 * 0.20 * contrast, chroma_adjust)
        elif role == KritaShade.DARK:
            return shade(srgb, 0.02 * 0.40 * contrast, chroma_adjust)
        else:
            return shade(srgb, 0.03 * 0.60 * contrast, chroma_adjust)

    # handle very light colors (base, midlight, light == mid, dark, shadow)
    if y > 0.93:
        if role == KritaShade.MIDLIGHT:
            return shade(srgb, -0.02 - 0.20 * contrast, chroma_adjust)
        elif role == KritaShade.DARK:
            return shade(srgb, -0.06 - 0.60 * contrast, chroma_adjust)
        elif role == KritaShade.SHADOW:
            return shade(srgb, -0.10 - 0.90 * contrast, chroma_adjust)
        else:
            return shade(srgb, -0.04 - 0.40 * contrast, chroma_adjust)

    # handle everything else
    light_amount = (0.05 + y * 0.55) * (0.25 + contrast * 0.75)
    dark_amount = (-y) * (0.55 + contrast * 0.35)
    if role == KritaShade.LIGHT:
        return shade(srgb, light_amount, chroma_adjust)
    elif role == KritaShade.MIDLIGHT:
        return shade(srgb, (0.15 + 0.35 * yi) * light_amount, chroma_adjust)
    elif role == KritaShade.MID:
        return shade(srgb, (0.35 + 0.15 * y) * dark_amount, chroma_adjust)
    elif role == KritaShade.DARK:
        return shade(srgb, dark_amount, chroma_adjust)
    elif role == KritaShade.SHADOW:
        return darken(shade(srgb, dark_amount, chroma_adjust), 0.5 + 0.3 * y)
    else:
        raise ValueError("Invalid role")

# Converted from Krita KColorScheme, LGPL-2.0-or-later
def effect_brush_fg(group, bg, fg):
    fx = int(group["ContrastEffect"])
    if fx == 2:
        fg = mix(fg, bg, float(group["ContrastAmount"]))
    elif fx == 1:
        pass
        fg = tint(fg, bg, float(group["ContrastAmount"]))
    elif fx != 0:
        raise ValueError(f"Unsupported contrast effect {fx}")

    return effect_brush(group, fg)

# Converted from Krita KColorScheme, LGPL-2.0-or-later
def effect_brush(group, srgb):
    fx = int(group["IntensityEffect"])
    if fx == 2:
        srgb = darken(srgb, float(group["IntensityAmount"]))
    elif fx != 0:
        raise ValueError(f"Unsupported intensity effect {fx}")

    fx = int(group["ColorEffect"])
    if fx == 2:
        srgb = mix(srgb, get_srgb(group["Color"]), float(group["ColorAmount"]))
    elif fx != 0:
        raise ValueError(f"Unsupported color effect {fx}")

    return srgb

def print_transformed(section, key, out, role = -1):
    if role != -1:
        srgb = krita_shade(get_srgb(src[section][key]), role)
    else:
        srgb = get_srgb(src[section][key])

    print(f"{out}={srgb_to_hex(srgb)}")

def print_transformed_effect(effect_group, cg_name, name_in, name_out, role = -1, bg_tint = None):
    color_group = src[cg_name]

    srgb = get_srgb(color_group[name_in])
    if role != -1:
        srgb = krita_shade(srgb, role)

    if name_in.startswith("Foreground"):
        bg = get_srgb(color_group["BackgroundNormal"])
        if bg_tint is not None:
            bg = tint(bg, bg_tint, 0.4)
        if role != -1:
            bg = krita_shade(bg, role)
        out = effect_brush_fg(effect_group, bg, srgb)
    else:
        out = effect_brush(effect_group, srgb)

    print(f"{name_out}={srgb_to_hex(out)}")

def print_inactive(cg_name, *args):
    effect_group = src["ColorEffects:Inactive"]
    bg_tint = None
    if cg_name == "Colors:Selection":
        if effect_group.get("ChangeSelectionColor", effect_group.get("Enable", "true")) != "false":
            cg_name = "Colors:Window"
            bg_tint = get_srgb(src["Colors:Selection"]["BackgroundNormal"])

    if effect_group.get("Enable", "false") == "false":
        print_transformed(cg_name, *args)
    else:
        print_transformed_effect(effect_group, cg_name, *args, bg_tint = bg_tint)

def print_disabled(*args):
    effect_group = src["ColorEffects:Disabled"]
    if effect_group.get("Enable", "true") == "false":
        print_transformed(*args)
    else:
        print_transformed_effect(effect_group, *args)

# Krita’s shade function does not invert the light/dark roles
if HCY(get_srgb(src["Colors:View"]["BackgroundNormal"])).y < 0.5:
    LIGHT = KritaShade.DARK
    MIDLIGHT = KritaShade.MID
    MID = KritaShade.MIDLIGHT
    DARK = KritaShade.LIGHT
else:
    LIGHT = KritaShade.LIGHT
    MIDLIGHT = KritaShade.MIDLIGHT
    MID = KritaShade.MID
    DARK = KritaShade.DARK

SHADOW = KritaShade.SHADOW

for (section, print_color) in [
    ("Active", print_transformed),
    ("Disabled", print_disabled),
    ("Inactive", print_inactive)
]:
    print(f"[{section}]")
    print_color("Colors:View", "BackgroundAlternate", "AlternateBase")
    print_color("Colors:View", "BackgroundNormal", "Base")
    print_color("Colors:View", "ForegroundLink", "BrightText")
    print_color("Colors:Button", "BackgroundNormal", "Button")
    print_color("Colors:Button", "ForegroundNormal", "ButtonText")
    print_color("Colors:Window", "BackgroundNormal", "Dark", DARK)
    print_color("Colors:Selection", "BackgroundNormal", "Highlight")
    print_color("Colors:Selection", "ForegroundNormal", "HighlightedText")
    print_color("Colors:Window", "BackgroundNormal", "Light", LIGHT)
    print_color("Colors:View", "ForegroundLink", "Link")
    print_color("Colors:View", "ForegroundVisited", "LinkVisited")
    print_color("Colors:Window", "BackgroundNormal", "Mid", MID)
    print_color("Colors:Window", "BackgroundNormal", "Midlight", MIDLIGHT)
    print_color("WM", "inactiveForeground", "PlaceholderText")
    print_color("Colors:Window", "BackgroundNormal", "Shadow", SHADOW)
    print_color("Colors:View", "ForegroundNormal", "Text")
    print_color("Colors:Tooltip", "BackgroundNormal", "ToolTipBase")
    print_color("Colors:Tooltip", "ForegroundNormal", "ToolTipText")
    print_color("Colors:Window", "BackgroundNormal", "Window")
    print_color("Colors:Window", "ForegroundNormal", "WindowText")
    if section != "Inactive":
        print("")
