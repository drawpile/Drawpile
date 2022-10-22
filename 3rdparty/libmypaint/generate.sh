#!/bin/bash
set -e
cd "$(dirname "$0")"
python generate.py mypaint-brush-settings-gen.h brushsettings-gen.h
