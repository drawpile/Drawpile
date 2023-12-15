#!/usr/bin/env python
# SPDX-License-Identifier: MIT
# This script generates a CMakePresets.json file in various permutations.
# Prints to stdout, pipe the result into the appropriate file.
import copy
import itertools
import json
import sys

configs = {
    "base": {
        "CLANG_TIDY": "OFF",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "UPDATE_TRANSLATIONS": "OFF",
        "USE_GENERATORS": "OFF",
    },
    "linux": {
        "+name": "linux",
        "+displayName": "Linux",
    },
    "windows": {
        "+name": "windows",
        "+displayName": "Windows",
        "CMAKE_INSTALL_PREFIX": "install",
        "CMAKE_MSVC_RUNTIME_LIBRARY": "MultiThreaded$<$<CONFIG:Debug>:Debug>",
        "CMAKE_TOOLCHAIN_FILE": "C:/vcpkg/scripts/buildsystems/vcpkg.cmake",
        "X_VCPKG_APPLOCAL_DEPS_INSTALL": "ON",
    },
    "windows_x64": {
        "+name": "x64",
        "+displayName": "64 Bit",
        "VCPKG_TARGET_TRIPLET": "x64-windows",
    },
    "windows_x86": {
        "+name": "x86",
        "+displayName": "32 Bit",
        "VCPKG_TARGET_TRIPLET": "x86-windows",
    },
    "debug": {
        "+name": "debug",
        "+displayName": "debug build",
        "CMAKE_BUILD_TYPE": "Debug",
    },
    "release": {
        "+name": "release",
        "+displayName": "release build",
        "ADDRESS_SANITIZER": "OFF",
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_INTERPROCEDURAL_OPTIMIZATION": "ON",
        "ENABLE_ARCH_NATIVE": "ON",
        "LEAK_SANITIZER": "OFF",
        "MEMORY_SANITIZER": "OFF",
        "THREAD_SANITIZER": "OFF",
        "UNDEFINED_SANITIZER": "OFF",
        "UPDATE_TRANSLATIONS": "OFF",
        "USE_STRICT_ALIASING": "ON",
    },
    "qt5": {
        "+name": "qt5",
        "+displayName": "Qt5",
        "QT_VERSION": "5",
    },
    "qt6": {
        "+name": "qt6",
        "+displayName": "Qt6",
        "QT_VERSION": "6",
    },
    "all": {
        "+name": "all",
        "+displayName": "all components",
        "BUILTINSERVER": "ON",
        "CLIENT": "ON",
        "SERVER": "ON",
        "SERVERGUI": "ON",
        "TESTS": "ON",
        "TOOLS": "ON",
    },
    "client": {
        "+name": "client",
        "+displayName": "only client",
        "BUILTINSERVER": "ON",
        "CLIENT": "ON",
        "SERVER": "OFF",
        "SERVERGUI": "OFF",
        "TESTS": "OFF",
        "TOOLS": "OFF",
    },
    "server": {
        "+name": "server",
        "+displayName": "only headless server",
        "BUILTINSERVER": "OFF",
        "CLIENT": "OFF",
        "SERVER": "ON",
        "SERVERGUI": "OFF",
        "TESTS": "OFF",
        "TOOLS": "OFF",
    },
    "make": {
        "+name": "make",
        "+displayName": "make",
        "@generator": "Unix Makefiles",
    },
    "ninja": {
        "+name": "ninja",
        "+displayName": "ninja",
        "@generator": "Ninja",
    },
}


def build_preset_step(parent, steps):
    if len(steps) == 0:
        return [parent]
    else:
        step = steps[0]
        presets = []
        for c in [step] if isinstance(step, str) else step:
            preset = copy.deepcopy(parent)
            for key, value in configs[c].items():
                if key == "+name":
                    preset["name"].append(value)
                elif key == "+displayName":
                    preset["displayName"].append(value)
                elif key == "@generator":
                    preset["generator"] = value
                else:
                    preset["cacheVariables"][key] = value
            for p in build_preset_step(preset, steps[1:]):
                presets.append(p)
        return presets


def build_preset(steps):
    base_preset = {
        "name": [],
        "displayName": [],
        "binaryDir": "build",
        "generator": None,
        "cacheVariables": {},
    }
    presets = build_preset_step(base_preset, steps)
    for preset in presets:
        preset["name"] = "-".join(preset["name"])
        preset["displayName"] = ", ".join(preset["displayName"])
    return presets


def generate_presets(recipes):
    cmake_presets = {
        "version": 1,
        "cmakeMinimumRequired": {
            "major": 3,
            "minor": 18,
            "patch": 0,
        },
        "configurePresets": list(
            itertools.chain.from_iterable(build_preset(r) for r in recipes)
        ),
    }
    json.dump(cmake_presets, sys.stdout, sort_keys=True, indent=4)
    sys.stdout.write("\n")


generate_presets(
    [
        [
            "base",
            "linux",
            ["debug", "release"],
            ["qt5", "qt6"],
            ["all", "client", "server"],
            ["make", "ninja"],
        ],
        [
            "base",
            "windows",
            ["windows_x64", "windows_x86"],
            ["debug", "release"],
            ["qt5", "qt6"],
            ["all", "client"],
            "ninja",
        ],
    ]
)
