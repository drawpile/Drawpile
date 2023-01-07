#!/usr/bin/env python3
from configparser import ConfigParser
import re
import sys

if len(sys.argv) != 3:
    prog = __file__ if len(sys.argv) == 0 else sys.argv[0]
    raise ValueError(f"Usage: {prog} light|dark KRITA_COLOR_FILE >DRAWPILE_COLOR_FILE")

if sys.argv[2] == "light":
    dark = False
elif sys.argv[2] == "dark":
    dark = True
else:
    raise ValueError(f"Expected dark or light, got '{sys.argv[2]}'")

src = ConfigParser()
src.read(sys.argv[1])

rgb_re = re.compile(r"^\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*$")

def rgb_to_hex(rgb):
    match = rgb_re.search(rgb)
    if match:
        r = int(match.group(1))
        g = int(match.group(2))
        b = int(match.group(3))
        return f"#{r:02x}{g:02x}{b:02x}"
    else:
        raise ValueError(f"Can't parse rgb value '{rgb}'")

def get_hex(section, key):
    rgb = src[section][key]
    return rgb_to_hex(rgb)

def print_transformed(section, key, out):
    print(f"{out}={get_hex(section, key)}")

print("[Active]")
print_transformed("Colors:View", "BackgroundAlternate", "AlternateBase")
print_transformed("Colors:View", "BackgroundNormal", "Base")
print_transformed("Colors:View", "ForegroundLink", "BrightText")
print_transformed("Colors:Button", "BackgroundNormal", "Button")
print_transformed("Colors:Button", "ForegroundNormal", "ButtonText")
print_transformed("Colors:Button", "BackgroundNormal" if dark else "ForegroundNormal", "Dark")
print_transformed("Colors:Selection", "BackgroundNormal", "Highlight")
print_transformed("Colors:Selection", "ForegroundNormal", "HighlightedText")
print_transformed("Colors:Button", "ForegroundNormal" if dark else "BackgroundNormal", "Light")
print_transformed("Colors:View", "ForegroundLink", "Link")
print_transformed("Colors:View", "ForegroundLink", "LinkVisited")
print_transformed("WM", "inactiveForeground", "PlaceholderText")
print_transformed("Colors:View", "ForegroundNormal", "Text")
print_transformed("Colors:Tooltip", "BackgroundNormal", "ToolTipBase")
print_transformed("Colors:Tooltip", "ForegroundNormal", "ToolTipText")
print_transformed("Colors:Window", "BackgroundNormal", "Window")
print_transformed("Colors:Window", "ForegroundNormal", "WindowText")
print("")
print("[Disabled]")
print_transformed("WM", "inactiveBackground", "AlternateBase")
print_transformed("WM", "inactiveBackground", "Base")
print_transformed("WM", "inactiveForeground", "BrightText")
print_transformed("WM", "inactiveBackground", "Button")
print_transformed("WM", "inactiveForeground", "ButtonText")
print_transformed("WM", "inactiveForeground", "Dark")
print_transformed("WM", "inactiveBackground", "Highlight")
print_transformed("WM", "inactiveForeground", "HighlightedText")
print_transformed("WM", "inactiveBackground", "Light")
print_transformed("WM", "inactiveForeground", "Link")
print_transformed("WM", "inactiveForeground", "LinkVisited")
print_transformed("WM", "inactiveForeground", "PlaceholderText")
print_transformed("WM", "inactiveForeground", "Text")
print_transformed("WM", "inactiveBackground", "ToolTipBase")
print_transformed("WM", "inactiveForeground", "ToolTipText")
print_transformed("WM", "inactiveBackground", "Window")
print_transformed("WM", "inactiveForeground", "WindowText")
print("")
print("[Inactive]")
print_transformed("Colors:View", "BackgroundAlternate", "AlternateBase")
print_transformed("Colors:View", "BackgroundNormal", "Base")
print_transformed("Colors:View", "ForegroundLink", "BrightText")
print_transformed("Colors:Button", "BackgroundNormal", "Button")
print_transformed("Colors:Button", "ForegroundNormal", "ButtonText")
print_transformed("Colors:Button", "BackgroundNormal" if dark else "ForegroundNormal", "Dark")
print_transformed("Colors:Selection", "BackgroundNormal", "Highlight")
print_transformed("Colors:Selection", "ForegroundNormal", "HighlightedText")
print_transformed("Colors:Button", "ForegroundNormal" if dark else "BackgroundNormal", "Light")
print_transformed("Colors:View", "ForegroundLink", "Link")
print_transformed("Colors:View", "ForegroundLink", "LinkVisited")
print_transformed("WM", "inactiveForeground", "PlaceholderText")
print_transformed("Colors:View", "ForegroundNormal", "Text")
print_transformed("Colors:Tooltip", "BackgroundNormal", "ToolTipBase")
print_transformed("Colors:Tooltip", "ForegroundNormal", "ToolTipText")
print_transformed("Colors:Window", "BackgroundNormal", "Window")
print_transformed("Colors:Window", "ForegroundNormal", "WindowText")
