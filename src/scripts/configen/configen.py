#!/usr/bin/env python
# SPDX-License-Identifier: GPL-3.0-or-later
# Formatted with black.
import jinja2
import os
import re
import sys
import yaml


def ucfirst(s):
    return s[:1].upper() + s[1:]


class Setting:
    def __init__(self, data):
        self.id = data["id"]
        self.type = data["type"]
        self.default = data["default"]
        self.qsettings = data.get("qsettings")
        self.condition = data.get("condition")

    @property
    def capitalized_id(self):
        return ucfirst(self.id)

    @property
    def is_basic_type(self):
        return not self.type[0].isupper()

    @property
    def is_literal_type(self):
        return self.is_basic_type or self.type == "QColor"

    @property
    def arg_prefix(self):
        if self.is_basic_type:
            return f"{self.type} "
        else:
            return f"const {self.type} &"

    @property
    def default_prefix(self):
        return f"{self.type} "

    @property
    def member_name(self):
        return f"m_{self.id}"

    @property
    def getter_name(self):
        return f"get{self.capitalized_id}"

    @property
    def qsettings_getter_name(self):
        return self.id

    def getter_signature(self, prefix=""):
        return f"{self.type} {prefix}get{self.capitalized_id}() const"

    @property
    def setter_name(self):
        return f"set{self.capitalized_id}"

    @property
    def qsettings_setter_name(self):
        return self.setter_name

    def setter_signature(self, prefix=""):
        return f"void {prefix}{self.setter_name}({self.arg_prefix}value)"

    @property
    def signal_name(self):
        return f"change{self.capitalized_id}"

    @property
    def qsettings_signal_name(self):
        return f"{self.id}Changed"

    def signal_signature(self):
        return f"void {self.signal_name}({self.arg_prefix}value)"

    @property
    def default_name(self):
        return f"default{self.capitalized_id}"

    def default_signature(self, prefix=""):
        return f"{self.default_prefix}{prefix}{self.default_name}()"


def parse_settings(path):
    with open(path) as fh:
        input_data = yaml.safe_load(fh)

    settings = []
    used_ids = set()
    used_qsettings_keys = set()
    for setting in input_data["settings"]:
        for key in ["id", "type", "default", "qsettings"]:
            if not setting.get(key):
                raise Exception(f"Missing '{key}' in {setting}")

        setting_id = setting["id"]
        if setting_id in used_ids:
            raise Exception(f"Duplicate id '{setting_id}'")

        qsettings_key = setting["qsettings"]
        if qsettings_key in used_qsettings_keys:
            raise Exception(f"Duplicate id '{qsettings_key}'")

        settings.append(Setting(setting))
        used_ids.add(setting_id)
        used_qsettings_keys.add(qsettings_key)

    settings.sort(key=lambda setting: setting.id)
    return settings


def load_template(template_path):
    with open(template_path) as fh:
        return jinja2.Template(
            fh.read(),
            trim_blocks=True,
            lstrip_blocks=True,
            undefined=jinja2.StrictUndefined,
        )


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise Exception(
            f"Usage: {sys.argv[0]} LIBCLIENT_OUTPUT_DIR "
            + "DESKTOP_OUTPUT_DIR INPUT_YAML_PATH"
        )

    script_dir = os.path.dirname(__file__)
    libclient_output_dir = sys.argv[1]
    desktop_output_dir = sys.argv[2]
    input_path = sys.argv[3]

    settings = parse_settings(input_path)

    targets = [
        (libclient_output_dir, "config.h"),
        (libclient_output_dir, "config.cpp"),
        (libclient_output_dir, "memoryconfig.h"),
        (libclient_output_dir, "memoryconfig.cpp"),
        (desktop_output_dir, "settingsconfig.h"),
        (desktop_output_dir, "settingsconfig.cpp"),
    ]

    for (output_dir, target) in targets:
        template = load_template(os.path.join(script_dir, f"{target}.jinja"))
        stream = template.stream(settings=settings)
        with open(os.path.join(output_dir, target), "w") as fh:
            stream.dump(fh)
            fh.write("\n")
