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
        self.legacy_package = data["legacy_package"]
        self.legacy_version = data.get("legacy_version", 0)
        self.legacy_get = data.get("legacy_get")
        self.legacy_set = data.get("legacy_set")
        self.legacy_notify = data.get("legacy_notify")
        self.condition = data.get("condition")
        if self.legacy_package not in ("desktop", "libclient"):
            raise ValueError(f"{self.id}: unknown legacy_package '{self.legacy_package}'")

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

    @property
    def has_legacy_get_set(self):
        return self.legacy_get is not None or self.legacy_set is not None

    @property
    def legacy_get_ref(self):
        if self.legacy_get is None:
            return "&any::get"
        else:
            return f"&{self.legacy_get}"

    @property
    def legacy_set_ref(self):
        if self.legacy_set is None:
            return "&any::set"
        else:
            return f"&{self.legacy_set}"

    @property
    def legacy_notify_ref(self):
        return f"&{self.legacy_notify}"


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
        (
            "config.h.jinja",
            os.path.join(libclient_output_dir, "config/config.h"),
        ),
        (
            "config.cpp.jinja",
            os.path.join(libclient_output_dir, "config/config.cpp"),
        ),
        (
            "memoryconfig.h.jinja",
            os.path.join(libclient_output_dir, "config/memoryconfig.h"),
        ),
        (
            "memoryconfig.cpp.jinja",
            os.path.join(libclient_output_dir, "config/memoryconfig.cpp"),
        ),
        (
            "libclient_settings_table.h.jinja",
            os.path.join(libclient_output_dir, "settings_table.h"),
        ),
        (
            "settingsconfig.h.jinja",
            os.path.join(desktop_output_dir, "config/settingsconfig.h"),
        ),
        (
            "settingsconfig.cpp.jinja",
            os.path.join(desktop_output_dir, "config/settingsconfig.cpp"),
        ),
        (
            "desktop_settings_table.h.jinja",
            os.path.join(desktop_output_dir, "settings_table.h"),
        ),
    ]

    for template_file, output_path in targets:
        template = load_template(os.path.join(script_dir, template_file))
        stream = template.stream(settings=settings)
        with open(output_path, "w") as fh:
            stream.dump(fh)
            fh.write("\n")
