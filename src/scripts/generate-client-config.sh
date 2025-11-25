#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Sets up and calls configen.py with the default paths.
set -e
cd "$(dirname "$0")"

status() {
    echo 1>&2
    echo "=> $@" 1>&2
    echo 1>&2
}

generator_dir="$(pwd)/configen"
env_dir="$generator_dir/env"
activate_file="$env_dir/bin/activate"
requirements_file="$generator_dir/requirements.txt"
settings_file="$generator_dir/settings.yaml"

if [ -e "$env_dir" ]; then
    status "Activating existing Python virtual environment at $env_dir"
    . "$activate_file"
else
    status "Setting up Python virtual environment at $env_dir"
    python -m venv "$env_dir"

    status "Activating new Python virtual environment at $env_dir"
    . "$activate_file"

    status "Installing Python dependencies from $requirements_file"
    pip install -r "$requirements_file"
fi

PYTHONPATH="$generator_dir"
export PYTHONPATH

status "Generating code from $settings_file"
python "$generator_dir/configen.py" '../libclient/config' '../desktop/config' "$settings_file"

status "Formatting generated code"
if ! clang-format -i ../{libclient/config/{,memory}config,desktop/config/settingsconfig}.{cpp,h}; then
    echo 2>&1
    echo '### Code formatting failed!' 2>&1
    echo '###' 2>&1
    echo '### Install clang-format to make this work or format them yourself.' 2>&1
    echo '###' 2>&1
    echo 2>&1
    exit 1
fi
