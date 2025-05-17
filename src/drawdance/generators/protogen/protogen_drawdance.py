#!/usr/bin/env python3
# Copyright (c) 2022 askmeaboutloom
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
import jinja2
import os
import protogen
import re
import sys


class DrawdanceFieldType:
    TYPES = {}

    def __init__(self, key, is_array):
        self.key = key
        self.is_array = is_array

    @classmethod
    def declare(cls, key, **kwargs):
        cls.TYPES[key] = cls(key=key, **kwargs)

    def access(self, f, subject):
        return f"{subject}->{f.field_name}"

    def access_size(self, f, subject):
        return f"{subject}->{f.name}_{f.array_size_name}"

    def deserialize_field_size(self, f):
        return None

    def deserialize_field_divisor(self, f):
        return 0

    def deserialize_field_count(self, f):
        return None

    def deserialize_field_check_remaining(self, f):
        return None

    def parse_field_count(self, f):
        return None

    def accessor_out_param(self, f):
        return None

    def static_payload_length(self, f):
        return 0

    def dynamic_payload_length(self, f):
        return None

    def local_match_length(self, f):
        return 0

    def array_declaration(self, f):
        return None

    def array_assign(self, f, subject):
        return None

    def array_check(self, f, subject):
        return None

    def array_size_name(self, f):
        return None

    def array_size_type(self, f):
        return None

    @staticmethod
    def _get_min_max(f):
        min_max = f.type.min_max
        if min_max is None:
            prefix = f.type.base_type.replace("_t", "").upper()
            return (f"{prefix}_MIN", f"{prefix}_MAX")
        else:
            return min_max

    @staticmethod
    def _get_flag_args(f):
        names = []
        values = []
        for flag in f.flags:
            names.append(f'"{flag.name}"')
            values.append(f"{flag.define_name}")
        names_arg = "(const char *[]){" + ", ".join(names) + "}"
        values_arg = "(unsigned int []){" + ", ".join(values) + "}"
        return (names_arg, values_arg, len(f.flags))

    def parse_text_int(self, f):
        fmt = f.format
        min_value, max_value = self._get_min_max(f)
        if not fmt:
            return f'({f.type.base_type})DP_text_reader_get_long(reader, "{f.name}", {min_value}, {max_value})'
        elif fmt == "div4":
            return f'({f.type.base_type})DP_text_reader_get_decimal(reader, "{f.name}", 4.0, {min_value}, {max_value})'
        else:
            raise RuntimeError(f"Unknown int format '{fmt}'")

    def parse_text_uint(self, f):
        fmt = f.format
        min_value, max_value = self._get_min_max(f)
        if not fmt:
            return f'({f.type.base_type})DP_text_reader_get_ulong(reader, "{f.name}", {max_value})'
        elif fmt == "div256":
            return f'({f.type.base_type})DP_text_reader_get_decimal(reader, "{f.name}", 256.0, 0, {max_value})'
        elif fmt == "flags":
            names_arg, values_arg, count = self._get_flag_args(f)
            return f'({f.type.base_type})DP_text_reader_get_flags(reader, "{f.name}", {count}, {names_arg}, {values_arg})'
        else:
            raise RuntimeError(f"Unknown uint format '{fmt}'")

    def parse_text_uint16_list(self, f):
        fmt = f.format
        if not fmt:
            return "DP_text_reader_parse_uint16_array"
        else:
            raise RuntimeError(f"Unknown uint16 list format '{fmt}'")

    def parse_text_uint24_list(self, f):
        fmt = f.format
        if not fmt:
            return "DP_text_reader_parse_uint24_array"
        else:
            raise RuntimeError(f"Unknown uint24 list format '{fmt}'")

    def parse_text_uint32_list(self, f):
        fmt = f.format
        if not fmt:
            return "DP_text_reader_parse_uint32_array"
        else:
            raise RuntimeError(f"Unknown uint32 list format '{fmt}'")

    def parse_text_int32_list(self, f):
        fmt = f.format
        if not fmt:
            return "DP_text_reader_parse_int32_array"
        else:
            raise RuntimeError(f"Unknown int32 list format '{fmt}'")

    def parse_subfield_int(self, f, index):
        fmt = f.format
        min_value, max_value = self._get_min_max(f)
        if not fmt:
            return f'DP_text_reader_get_subfield_long(reader, i, "{f.field_name}", {min_value}, {max_value})'
        elif fmt == "div4":
            return f'DP_text_reader_get_subfield_decimal(reader, i, "{f.field_name}", 4.0, {min_value}, {max_value})'
        else:
            raise RuntimeError(f"Unknown int subfield format '{fmt}'")

    def parse_subfield_uint(self, f, index):
        fmt = f.format
        min_value, max_value = self._get_min_max(f)
        if not fmt:
            return f'DP_text_reader_get_subfield_ulong(reader, i, "{f.field_name}", {max_value})'
        elif fmt == "div256":
            return f'DP_text_reader_get_subfield_decimal(reader, i, "{f.field_name}", 256.0, 0, {max_value})'
        else:
            raise RuntimeError(f"Unknown uint subfield format '{fmt}'")

    def parse_subfield_rgb24(self, f, index):
        fmt = f.format
        if not fmt:
            return f'DP_text_reader_get_subfield_rgb_color(reader, i, "{f.field_name}")'
        else:
            raise RuntimeError(f"Unknown rgb24 subfield format '{fmt}'")

    def write_text_raw(self, f, subject, fn):
        fmt = f.format
        if fmt:
            raise RuntimeError(f"Unknown raw format '{fmt}'")
        a = self.access(f, subject)
        return f'{fn}(writer, "{f.name}", {a})'

    def write_text_int(self, f, subject):
        a = self.access(f, subject)
        fmt = f.format
        if not fmt:
            return f'DP_text_writer_write_int(writer, "{f.name}", {a})'
        elif fmt == "div4":
            return (
                f'DP_text_writer_write_decimal(writer, "{f.name}", (double){a} / 4.0)'
            )
        else:
            raise RuntimeError(f"Unknown int format '{fmt}'")

    def write_text_uint(self, f, subject):
        a = self.access(f, subject)
        fmt = f.format
        if not fmt:
            return f'DP_text_writer_write_uint(writer, "{f.name}", {a})'
        elif fmt == "div256":
            return (
                f'DP_text_writer_write_decimal(writer, "{f.name}", (double){a} / 256.0)'
            )
        elif fmt == "flags":
            names_arg, values_arg, count = self._get_flag_args(f)
            return f'DP_text_writer_write_flags(writer, "{f.name}", {a}, {count}, {names_arg}, {values_arg})'
        else:
            raise RuntimeError(f"Unknown uint format '{fmt}'")

    def write_text_uint16_list(self, f, a, a_count):
        return f'DP_text_writer_write_uint16_list(writer, "{f.name}", {a}, {a_count})'

    def write_text_uint24_list(self, f, a, a_count):
        return f'DP_text_writer_write_uint24_list(writer, "{f.name}", {a}, {a_count})'

    def write_text_uint32_list(self, f, a, a_count):
        return f'DP_text_writer_write_uint32_list(writer, "{f.name}", {a}, {a_count})'

    def write_text_int32_list(self, f, a, a_count):
        return f'DP_text_writer_write_int32_list(writer, "{f.name}", {a}, {a_count})'

    def write_subfield_text_int(self, f, subject):
        a = self.access(f, subject)
        fmt = f.format
        if not fmt:
            return f"DP_text_writer_write_subfield_int(writer, \"{f.field_name}\", {a})"
        elif fmt == "div4":
            return f"DP_text_writer_write_subfield_decimal(writer, \"{f.field_name}\", (double){a} / 4.0)"
        else:
            raise RuntimeError(f"Unknown int subfield format '{fmt}'")

    def write_subfield_text_uint(self, f, subject):
        a = self.access(f, subject)
        fmt = f.format
        if not fmt:
            return f"DP_text_writer_write_subfield_uint(writer, \"{f.field_name}\", {a})"
        elif fmt == "div256":
            return f"DP_text_writer_write_subfield_decimal(writer, \"{f.field_name}\", (double){a} / 256.0)"
        else:
            raise RuntimeError(f"Unknown uint subfield format '{fmt}'")

    def write_subfield_text_rgb24(self, f, subject):
        a = self.access(f, subject)
        fmt = f.format
        if not fmt:
            return f"DP_text_writer_write_subfield_rgb_color(writer, \"{f.field_name}\", {a})"
        else:
            raise RuntimeError(f"Unknown rgb24 subfield format '{fmt}'")


class DrawdancePlainFieldType(DrawdanceFieldType):
    def __init__(
        self,
        key,
        base_type,
        payload_length,
        serialize_payload_fn,
        write_payload_text_fn,
        deserialize_payload_fn,
        parse_field_fn,
        parse_subfield_fn=None,
        write_subfield_payload_text_fn=None,
        min_max=None,
    ):
        super().__init__(key, False)
        self.base_type = base_type
        self.payload_length = payload_length
        self.serialize_payload_fn = serialize_payload_fn
        self.write_payload_text_fn = write_payload_text_fn
        self.deserialize_payload_fn = deserialize_payload_fn
        self.parse_field_fn = parse_field_fn
        self.parse_subfield_fn = parse_subfield_fn
        self.write_subfield_payload_text_fn = write_subfield_payload_text_fn
        self.min_max = min_max

    def accessor_return_type(self, f):
        return self.base_type

    def constructor_param(self, f):
        return f"{self.base_type} {f.name}"

    def deserialize_field(self, f):
        return f"{self.base_type} {f.name} = {self.deserialize_payload_fn}(buffer + read, &read);"

    def deserialize_constructor_arg(self, f):
        if not f.convert:
            return f.name
        elif f.convert == "layer_id":
            return f"deserialize_layer_id_compat({f.name})"
        elif f.convert in ("annotation_id", "track_id"):
            return f"convert_other_id_compat({f.name})"
        elif f.convert == "key_frame_set_source_id":
            return f"source == DP_MSG_KEY_FRAME_SET_SOURCE_LAYER ? deserialize_layer_id_compat({f.name}) : convert_other_id_compat({f.name})"
        elif f.convert == "u24_to_16":
            return f"DP_uint16_to_uint32({f.name})"
        elif f.convert == "u16_to_8":
            return f"DP_uint8_to_uint16({f.name})"
        elif f.convert == "mask_u8":
            return f"{f.name} & (uint8_t){f.convert_args}"
        elif f.convert == "old_blend_mode":
            return f"DP_blend_mode_to_compatible({f.name})"
        else:
            raise RuntimeError(f"Unknown conversion {f.convert}")

    def parse_field(self, f):
        fn = self.parse_field_fn
        if isinstance(fn, str):
            assign = f'{fn}(reader, "{f.name}")'
        else:
            assign = fn(self, f)
        return f"{self.base_type} {f.name} = {assign};"

    def parse_subfield(self, f, index):
        fn = self.parse_subfield_fn
        if fn:
            return (
                f"{self.base_type} {f.name} = ({self.base_type}){fn(self, f, index)};"
            )
        else:
            raise RuntimeError(f"No parse subfield fn for {self.key}")

    def parse_constructor_arg(self, f):
        return f.name

    def static_payload_length(self, f):
        return self.payload_length

    def local_match_length(self, f):
        return self.payload_length

    def struct_declaration(self, f):
        return f"{self.base_type} {f.name}"

    def equals(self, f):
        return f"a->{f.field_name} == b->{f.field_name}"

    def serialize_payload(self, f, subject, dst):
        a = self.access(f, subject)
        if not f.convert:
            pass
        elif f.convert == "layer_id":
            a = f"serialize_layer_id_compat({a})"
        elif f.convert in ("annotation_id", "track_id"):
            a = f"convert_other_id_compat({a})"
        elif f.convert == "key_frame_set_source_id":
            a = f"mkfs->source == DP_MSG_KEY_FRAME_SET_SOURCE_LAYER ? serialize_layer_id_compat({a}) : convert_other_id_compat({a})"
        elif f.convert == "u24_to_16":
            a = f"DP_uint32_to_uint16({a})"
        elif f.convert == "u16_to_8":
            a = f"DP_uint16_to_uint8({a})"
        elif f.convert == "mask_u8":
            a = f"{a} & (uint8_t){f.convert_args}"
        elif f.convert == "old_blend_mode":
            a = f"DP_blend_mode_to_compatible({a})"
        else:
            raise RuntimeError(f"Unknown conversion {f.convert}")
        return f"{self.serialize_payload_fn}({a}, {dst})"

    def serialize_payload_compat(self, f, subject, dst):
        return self.serialize_payload(f, subject, dst)

    def serialize_local_match(self, f, subject, dst):
        return self.serialize_payload(f, subject, dst)

    def match_local_match(self, f, subject):
        return f"{self.deserialize_payload_fn}(buffer + read, &read) == {subject}->{f.field_name}"

    def write_payload_text(self, f, subject):
        fn = self.write_payload_text_fn
        if isinstance(fn, str):
            return self.write_text_raw(f, subject, fn)
        else:
            return fn(self, f, subject)

    def write_subfield_payload_text(self, f, subject):
        fn = self.write_subfield_payload_text_fn
        if fn:
            return fn(self, f, subject)
        else:
            raise RuntimeError(f"No subfield payload text fn for {self.key}")

    def assign(self, f, subject):
        a = self.access(f, subject)
        return f"{a} = {f.name}"


class DrawdanceArrayFieldType(DrawdanceFieldType):
    def __init__(
        self,
        key,
        base_type,
        size_type,
        payload_length,
        serialize_payload_fn,
        write_payload_text_fn,
        deserialize_payload_fn,
        parse_get_field_fn,
        parse_constructor_fn,
    ):
        super().__init__(key, True)
        self.base_type = base_type
        self.size_type = size_type
        self.payload_length = payload_length
        self.multiplier = (
            "" if self.payload_length == 1 else f" * {self.payload_length}"
        )
        self.divisor = "" if self.payload_length == 1 else f" / {self.payload_length}"
        self.serialize_payload_fn = serialize_payload_fn
        self.write_payload_text_fn = write_payload_text_fn
        self.deserialize_payload_fn = deserialize_payload_fn
        self.parse_get_field_fn = parse_get_field_fn
        self.parse_constructor_fn = parse_constructor_fn

    def _to_size(self, expr):
        if self.size_type == "size_t":
            return expr
        elif self.size_type == "int":
            return f"DP_int_to_size({expr})"
        else:
            raise NotImplemented(f'Unknown size type "{self.size_type}"')

    def _to_uint16(self, expr):
        if self.size_type == "size_t":
            return f"DP_size_to_uint16({expr})"
        elif self.size_type == "int":
            return f"DP_int_to_uint16({expr})"
        else:
            raise NotImplemented(f'Unknown size type "{self.size_type}"')

    def access(self, f, subject):
        if f.offset_field:
            offset_expr = f.offset_field.offset_expr(subject)
            return f"{subject}->{f.field_name} + {offset_expr}"
        else:
            return super().access(f, subject)

    def accessor_return_type(self, f):
        return f"const {self.base_type} *"

    def accessor_out_param(self, f):
        return f"{self.size_type} *out_{self.array_size_name(f)}"

    def accessor_out_param_condition(self, f):
        return f"out_{self.array_size_name(f)}"

    def accessor_out_param_assign(self, f, subject):
        return f"*out_{self.array_size_name(f)} = {subject}->{f.name}_{self.array_size_name(f)}"

    def constructor_param(self, f):
        return (
            f"void (*set_{f.name})({self.size_type}, {self.base_type} *, void *), "
            + f"{self.size_type} {f.name}_{self.array_size_name(f)}, void *{f.name}_user"
        )

    def deserialize_field_size(self, f):
        return "length - read"

    def deserialize_field_divisor(self, f):
        return self.payload_length

    def deserialize_field_count(self, f):
        return f"uint16_t {f.name}_{self.array_size_name(f)} = DP_size_to_uint16({f.name}_bytes{self.divisor});"

    def deserialize_field(self, f):
        return f"void *{f.name}_user = (void *)(buffer + read);"

    def deserialize_constructor_arg(self, f):
        if not f.convert:
            return f"{self.deserialize_payload_fn}, {f.name}_{self.array_size_name(f)}, {f.name}_user"
        elif f.convert == "track_order_tracks":
            return f"read_track_ids_compat, {f.name}_{self.array_size_name(f)}, {f.name}_user"
        elif f.convert == "key_frame_layer_attributes_layer_flags":
            return f"read_key_frame_layer_flags_compat, {f.name}_{self.array_size_name(f)} / 2, {f.name}_user"
        else:
            raise RuntimeError(f"Unknown conversion {f.convert}")

    def parse_field_count(self, f):
        return f"{self.size_type} {f.name}_{self.array_size_name(f)};"

    def parse_field(self, f):
        return f'DP_TextReaderParseParams {f.name}_params = {self.parse_get_field_fn}(reader, "{f.name}", &{f.name}_{self.array_size_name(f)});'

    def parse_constructor_arg(self, f):
        fn = self.parse_constructor_fn
        if isinstance(fn, str):
            parse_constructor_fn = fn
        else:
            parse_constructor_fn = fn(self, f)
        return f"{parse_constructor_fn}, {f.name}_{self.array_size_name(f)}, &{f.name}_params"

    def dynamic_payload_length(self, f):
        base = self._to_size(
            f"{f.message.param_name}->{f.name}_{self.array_size_name(f)}"
        )
        return base + self.multiplier

    def local_match_length(self, f):
        return 2

    def struct_declaration(self, f):
        return f"uint16_t {f.name}_{self.array_size_name(f)}"

    def array_declaration(self, f):
        return f"{self.base_type} {f.name}[]"

    def array_field_size(self, f):
        base = self._to_size(f"{f.func_name}_{self.array_size_name(f)}")
        return base + self.multiplier

    def equals(self, f):
        try:
            a, b = self.access(f, "a"), self.access(f, "b")
            size_name = self.array_size_name(f)
            a_count, b_count = f"a->{f.name}_{size_name}", f"b->{f.name}_{size_name}"
            size = f"DP_uint16_to_size({a_count}){self.multiplier}"
            return f"{a_count} == {b_count} && memcmp({a}, {b}, {size}) == 0"
        except Exception as e:
            raise Exception("a") from e

    def serialize_payload(self, f, subject, dst):
        a = self.access(f, subject)
        a_count = f"{subject}->{f.name}_{self.array_size_name(f)}"
        if not f.convert:
            return f"{self.serialize_payload_fn}({a}, {a_count}, {dst})"
        elif f.convert == "track_order_tracks":
            return f"write_track_ids_compat({a}, {a_count}, {dst})"
        elif f.convert == "key_frame_layer_attributes_layer_flags":
            return f"write_key_frame_layer_flags_compat({a}, {a_count}, {dst})"
        else:
            raise RuntimeError(f"Unknown conversion {f.convert}")

    def serialize_payload_compat(self, f, subject, dst):
        return self.serialize_payload(f, subject, dst)

    def serialize_local_match(self, f, subject, dst):
        a_count = f"{subject}->{f.name}_{self.array_size_name(f)}"
        return f"DP_write_bigendian_uint16({a_count}, {dst})"

    def match_local_match(self, f, subject):
        a_count = f"{subject}->{f.name}_{self.array_size_name(f)}"
        return f"read_uint16(buffer + read, &read) == {a_count}"

    def write_payload_text(self, f, subject):
        a = self.access(f, subject)
        a_count = f"{subject}->{f.name}_{self.array_size_name(f)}"
        fmt = f.format or ""
        fn = self.write_payload_text_fn
        if isinstance(fn, str):
            if fmt:
                raise RuntimeError(f"Unknown {self.base_type} array format '{fmt}'")
            else:
                return f'{fn}(writer, "{f.name}", {a}, {a_count})'
        else:
            return fn(self, f, a, a_count)

    def assign(self, f, subject):
        size_name = self.array_size_name(f)
        a_count = f"{subject}->{f.name}_{size_name}"
        value = self._to_uint16(f"{f.name}_{size_name}")
        return f"{a_count} = {value}"

    def array_assign(self, f, subject):
        a = self.access(f, subject)
        a_count = f"{subject}->{f.name}_{self.array_size_name(f)}"
        return f"set_{f.name}({a_count}, {a}, {f.name}_user)"

    def array_check(self, f, subject):
        return f"set_{f.name}"

    def array_size_name(self, f):
        return "size" if self.size_type == "size_t" else "count"

    def array_size_type(self, f):
        return self.size_type


class DrawdanceStringFieldType(DrawdanceFieldType):
    def __init__(self, key):
        super().__init__(key, True)

    def accessor_return_type(self, f):
        return "const char *"

    def accessor_out_param(self, f):
        return "size_t *out_len"

    def accessor_out_param_condition(self, f):
        return "out_len"

    def accessor_out_param_assign(self, f, subject):
        return f"*out_len = {subject}->{f.name}_len"

    def constructor_param(self, f):
        return f"const char *{f.name}_value, size_t {f.name}_len"

    def deserialize_field_size(self, f):
        return "length - read"

    def deserialize_field_divisor(self, f):
        return 1

    def deserialize_field_count(self, f):
        return f"uint16_t {f.name}_len = DP_size_to_uint16({f.name}_bytes);"

    def deserialize_field(self, f):
        return f"const char *{f.name} = (const char *)buffer + read;"

    def deserialize_constructor_arg(self, f):
        return f"{f.name}, {f.name}_len"

    def parse_field_count(self, f):
        return f"uint16_t {f.name}_len;"

    def parse_field(self, f):
        return f'const char *{f.name} = DP_text_reader_get_string(reader, "{f.name}", &{f.name}_len);'

    def parse_constructor_arg(self, f):
        return f"{f.name}, {f.name}_len"

    def dynamic_payload_length(self, f):
        return f"DP_uint16_to_size({f.message.param_name}->{f.name}_len)"

    def local_match_length(self, f):
        # No messages with strings are matched locally.
        raise NotImplementedError()

    def struct_declaration(self, f):
        return f"uint16_t {f.name}_len"

    def array_declaration(self, f):
        return f"char {f.name}[]"

    def array_field_size(self, f):
        return f"{f.func_name}_len + 1"

    def equals(self, f):
        a, b = self.access(f, "a"), self.access(f, "b")
        a_len, b_len = f"a->{f.name}_len", f"b->{f.name}_len"
        return f"{a_len} == {b_len} && memcmp({a}, {b}, {a_len}) == 0"

    def serialize_payload(self, f, subject, dst):
        a = self.access(f, subject)
        a_len = f"{subject}->{f.name}_len"
        return f"DP_write_bytes({a}, 1, {a_len}, {dst})"

    def serialize_payload_compat(self, f, subject, dst):
        return self.serialize_payload(f, subject, dst)

    def write_payload_text(self, f, subject):
        fmt = f.format
        if fmt:
            raise RuntimeError(f"Unknown string format '{fmt}'")
        a = self.access(f, subject)
        return f'DP_text_writer_write_string(writer, "{f.name}", {a})'

    def assign(self, f, subject):
        a_count = f"{subject}->{f.name}_len"
        return f"{a_count} = DP_size_to_uint16({f.name}_len)"

    def array_assign(self, f, subject):
        a = self.access(f, subject)
        a_len = f"{subject}->{f.name}_len"
        return f"assign_string({a}, {f.name}_value, {a_len})"

    def offset_expr(self, f, subject):
        return f"{subject}->{f.name}_len + 1"

    def array_size_name(self, f):
        return "len"

    def array_size_type(self, f):
        return "size_t"


class DrawdanceStringWithLengthFieldType(DrawdanceStringFieldType):
    def __init__(self, key):
        super().__init__(key)

    def access(self, f, subject):
        return f"((char *){subject}->{f.field_name})"

    def deserialize_field_size(self, f):
        return "read_uint8(buffer + read, &read)"

    def deserialize_field(self, f):
        return f"const char *{f.name} = read_string_with_length(buffer + read, {f.name}_len, &read);"

    def deserialize_field_check_remaining(self, f):
        return True

    def static_payload_length(self, f):
        return 1

    def serialize_payload(self, f, subject, dst):
        a = self.access(f, subject)
        a_len = f"{subject}->{f.name}_len"
        return f"write_string_with_length({a}, {a_len}, {dst})"

    def serialize_payload_compat(self, f, subject, dst):
        return self.serialize_payload(f, subject, dst)


class DrawdanceStructFieldType(DrawdanceFieldType):
    def __init__(self, key):
        super().__init__(key, True)

    def accessor_return_type(self, f):
        return f"const {f.sub.struct_name} *"

    def accessor_out_param(self, f):
        return "int *out_count"

    def accessor_out_param_condition(self, f):
        return "out_count"

    def accessor_out_param_assign(self, f, subject):
        return f"*out_count = {subject}->{f.name}_count"

    def constructor_param(self, f):
        return (
            f"void (*set_{f.name})(int, {f.sub.struct_name} *, void *), "
            + f"int {f.name}_count, void *{f.name}_user"
        )

    def deserialize_field_size(self, f):
        return "length - read"

    def deserialize_field_divisor(self, f):
        return f.sub.item_length

    def deserialize_field_count(self, f):
        return f"int {f.name}_count = DP_size_to_int({f.name}_bytes) / {f.sub.item_length};"

    def deserialize_field(self, f):
        return f"void *{f.name}_user = (void *)(buffer + read);"

    def deserialize_constructor_arg(self, f):
        return f"{f.sub.func_name}_deserialize{"_compat" if f.message.is_compat else ""}, {f.name}_count, {f.name}_user"

    def parse_field_count(self, f):
        return f'int {f.name}_count = DP_text_reader_get_sub_count(reader);'

    def parse_field(self, f):
        return f"void *{f.name}_user = reader;"

    def parse_constructor_arg(self, f):
        return f"{f.sub.func_name}_parse, {f.name}_count, {f.name}_user"

    def dynamic_payload_length(self, f):
        payload_length = sum([sf.static_payload_length for sf in f.sub.fields])
        multiplier = "" if payload_length == 1 else f" * {payload_length}"
        return f"DP_int_to_size({f.message.param_name}->{f.name}_count){multiplier}"

    def local_match_length(self, f):
        return 2

    def struct_declaration(self, f):
        return f"uint16_t {f.name}_count"

    def array_declaration(self, f):
        return f"{f.sub.struct_name} {f.name}[]"

    def array_field_size(self, f):
        return f"DP_int_to_size({f.func_name}_count) * sizeof({f.sub.struct_name})"

    def equals(self, f):
        a, b = self.access(f, "a"), self.access(f, "b")
        a_count, b_count = f"a->{f.name}_count", f"b->{f.name}_count"
        return (
            f"{a_count} == {b_count} && {f.sub.func_name}s_equal({a}, {b}, {a_count})"
        )

    def serialize_payload(self, f, subject, dst):
        a = self.access(f, subject)
        a_count = f"{subject}->{f.name}_count"
        return f"{f.sub.func_name}_serialize_payloads({a}, {a_count}, {dst})"

    def serialize_payload_compat(self, f, subject, dst):
        a = self.access(f, subject)
        a_count = f"{subject}->{f.name}_count"
        return f"{f.sub.func_name}_serialize_payloads_compat({a}, {a_count}, {dst})"

    def serialize_local_match(self, f, subject, dst):
        a_count = f"{subject}->{f.name}_{self.array_size_name(f)}"
        return f"DP_write_bigendian_uint16({a_count}, {dst})"

    def match_local_match(self, f, subject):
        a_count = f"{subject}->{f.name}_{self.array_size_name(f)}"
        return f"read_uint16(buffer + read, &read) == {a_count}"

    def write_payload_text(self, f, subject):
        fmt = f.format
        if fmt:
            raise RuntimeError(f"Unknown struct format '{fmt}'")
        a = self.access(f, subject)
        a_count = f"{subject}->{f.name}_count"
        return f"{f.sub.func_name}_write_payload_texts({a}, {a_count}, writer)"

    def assign(self, f, subject):
        a_count = f"{subject}->{f.name}_count"
        return f"{a_count} = DP_int_to_uint16({f.name}_count)"

    def array_assign(self, f, subject):
        a = self.access(f, subject)
        a_count = f"{subject}->{f.name}_count"
        return f"set_{f.name}({a_count}, {a}, {f.name}_user)"

    def array_size_name(self, f):
        return "count"

    def array_size_type(self, f):
        return "int"


DrawdancePlainFieldType.declare(
    key="blendmode",
    base_type="uint8_t",
    payload_length=1,
    serialize_payload_fn="DP_write_bigendian_uint8",
    write_payload_text_fn="DP_text_writer_write_blend_mode",
    deserialize_payload_fn="read_uint8",
    parse_field_fn="DP_text_reader_get_blend_mode",
)

DrawdancePlainFieldType.declare(
    key="rgb24",
    base_type="uint32_t",
    payload_length=3,
    serialize_payload_fn="DP_write_bigendian_uint24",
    write_payload_text_fn="DP_text_writer_write_rgb_color",
    write_subfield_payload_text_fn=DrawdanceFieldType.write_subfield_text_rgb24,
    deserialize_payload_fn="read_uint24",
    parse_field_fn="DP_text_reader_get_rgb_color",
    parse_subfield_fn=DrawdanceFieldType.parse_subfield_rgb24,
)

DrawdancePlainFieldType.declare(
    key="argb32",
    base_type="uint32_t",
    payload_length=4,
    serialize_payload_fn="DP_write_bigendian_uint32",
    write_payload_text_fn="DP_text_writer_write_argb_color",
    deserialize_payload_fn="read_uint32",
    parse_field_fn="DP_text_reader_get_argb_color",
)

DrawdancePlainFieldType.declare(
    key="bool",
    base_type="bool",
    payload_length=1,
    serialize_payload_fn="DP_write_bigendian_uint8",
    write_payload_text_fn="DP_text_writer_write_bool",
    deserialize_payload_fn="read_bool",
    parse_field_fn="DP_text_reader_get_bool",
)

DrawdancePlainFieldType.declare(
    key="i8",
    base_type="int8_t",
    payload_length=1,
    serialize_payload_fn="DP_write_bigendian_int8",
    write_payload_text_fn=DrawdanceFieldType.write_text_int,
    write_subfield_payload_text_fn=DrawdanceFieldType.write_subfield_text_int,
    deserialize_payload_fn="read_int8",
    parse_field_fn=DrawdanceFieldType.parse_text_int,
    parse_subfield_fn=DrawdanceFieldType.parse_subfield_int,
)

DrawdancePlainFieldType.declare(
    key="i16",
    base_type="int16_t",
    payload_length=2,
    serialize_payload_fn="DP_write_bigendian_int16",
    write_payload_text_fn=DrawdanceFieldType.write_text_int,
    write_subfield_payload_text_fn=DrawdanceFieldType.write_subfield_text_int,
    deserialize_payload_fn="read_int16",
    parse_field_fn=DrawdanceFieldType.parse_text_int,
    parse_subfield_fn=DrawdanceFieldType.parse_subfield_int,
)

DrawdancePlainFieldType.declare(
    key="i24",
    base_type="int32_t",
    payload_length=3,
    serialize_payload_fn="DP_write_bigendian_int24",
    write_payload_text_fn=DrawdanceFieldType.write_text_int,
    write_subfield_payload_text_fn=DrawdanceFieldType.write_subfield_text_int,
    deserialize_payload_fn="read_int24",
    parse_field_fn=DrawdanceFieldType.parse_text_int,
    parse_subfield_fn=DrawdanceFieldType.parse_subfield_int,
)

DrawdancePlainFieldType.declare(
    key="i32",
    base_type="int32_t",
    payload_length=4,
    serialize_payload_fn="DP_write_bigendian_int32",
    write_payload_text_fn=DrawdanceFieldType.write_text_int,
    write_subfield_payload_text_fn=DrawdanceFieldType.write_subfield_text_int,
    deserialize_payload_fn="read_int32",
    parse_field_fn=DrawdanceFieldType.parse_text_int,
    parse_subfield_fn=DrawdanceFieldType.parse_subfield_int,
)

DrawdancePlainFieldType.declare(
    key="u8",
    base_type="uint8_t",
    payload_length=1,
    serialize_payload_fn="DP_write_bigendian_uint8",
    write_payload_text_fn=DrawdanceFieldType.write_text_uint,
    write_subfield_payload_text_fn=DrawdanceFieldType.write_subfield_text_uint,
    deserialize_payload_fn="read_uint8",
    parse_field_fn=DrawdanceFieldType.parse_text_uint,
    parse_subfield_fn=DrawdanceFieldType.parse_subfield_uint,
)

DrawdancePlainFieldType.declare(
    key="u16",
    base_type="uint16_t",
    payload_length=2,
    serialize_payload_fn="DP_write_bigendian_uint16",
    write_payload_text_fn=DrawdanceFieldType.write_text_uint,
    write_subfield_payload_text_fn=DrawdanceFieldType.write_subfield_text_uint,
    deserialize_payload_fn="read_uint16",
    parse_field_fn=DrawdanceFieldType.parse_text_uint,
    parse_subfield_fn=DrawdanceFieldType.parse_subfield_uint,
)

DrawdancePlainFieldType.declare(
    key="u24",
    base_type="uint32_t",
    payload_length=3,
    serialize_payload_fn="DP_write_bigendian_uint24",
    write_payload_text_fn=DrawdanceFieldType.write_text_uint,
    write_subfield_payload_text_fn=DrawdanceFieldType.write_subfield_text_uint,
    deserialize_payload_fn="read_uint24",
    parse_field_fn=DrawdanceFieldType.parse_text_uint,
    parse_subfield_fn=DrawdanceFieldType.parse_subfield_uint,
    min_max=("DP_UINT24_MIN", "DP_UINT24_MAX")
)

DrawdancePlainFieldType.declare(
    key="u32",
    base_type="uint32_t",
    payload_length=4,
    serialize_payload_fn="DP_write_bigendian_uint32",
    write_payload_text_fn=DrawdanceFieldType.write_text_uint,
    write_subfield_payload_text_fn=DrawdanceFieldType.write_subfield_text_uint,
    deserialize_payload_fn="read_uint32",
    parse_field_fn=DrawdanceFieldType.parse_text_uint,
    parse_subfield_fn=DrawdanceFieldType.parse_subfield_uint,
)

DrawdanceArrayFieldType.declare(
    key="Bytes",
    base_type="unsigned char",
    size_type="size_t",
    payload_length=1,
    serialize_payload_fn="write_bytes",
    write_payload_text_fn="DP_text_writer_write_base64",
    deserialize_payload_fn="read_bytes",
    parse_get_field_fn="DP_text_reader_get_base64_string",
    parse_constructor_fn="DP_text_reader_parse_base64",
)

DrawdanceArrayFieldType.declare(
    key="Vec<u8>",
    base_type="uint8_t",
    size_type="int",
    payload_length=1,
    serialize_payload_fn="DP_write_bigendian_uint8_array",
    write_payload_text_fn="DP_text_writer_write_uint8_list",
    deserialize_payload_fn="read_uint8_array",
    parse_get_field_fn="DP_text_reader_get_array",
    parse_constructor_fn="DP_text_reader_parse_uint8_array",
)

DrawdanceArrayFieldType.declare(
    key="Vec<u16>",
    base_type="uint16_t",
    size_type="int",
    payload_length=2,
    serialize_payload_fn="DP_write_bigendian_uint16_array",
    write_payload_text_fn=DrawdanceFieldType.write_text_uint16_list,
    deserialize_payload_fn="read_uint16_array",
    parse_get_field_fn="DP_text_reader_get_array",
    parse_constructor_fn=DrawdanceFieldType.parse_text_uint16_list,
)

DrawdanceArrayFieldType.declare(
    key="Vec<u24>",
    base_type="uint32_t",
    size_type="int",
    payload_length=3,
    serialize_payload_fn="DP_write_bigendian_uint24_array",
    write_payload_text_fn=DrawdanceFieldType.write_text_uint24_list,
    deserialize_payload_fn="read_uint24_array",
    parse_get_field_fn="DP_text_reader_get_array",
    parse_constructor_fn=DrawdanceFieldType.parse_text_uint24_list,
)

DrawdanceArrayFieldType.declare(
    key="Vec<u32>",
    base_type="uint32_t",
    size_type="int",
    payload_length=4,
    serialize_payload_fn="DP_write_bigendian_uint32_array",
    write_payload_text_fn=DrawdanceFieldType.write_text_uint32_list,
    deserialize_payload_fn="read_uint32_array",
    parse_get_field_fn="DP_text_reader_get_array",
    parse_constructor_fn=DrawdanceFieldType.parse_text_uint32_list,
)

DrawdanceArrayFieldType.declare(
    key="Vec<i32>",
    base_type="int32_t",
    size_type="int",
    payload_length=4,
    serialize_payload_fn="DP_write_bigendian_int32_array",
    write_payload_text_fn=DrawdanceFieldType.write_text_int32_list,
    deserialize_payload_fn="read_int32_array",
    parse_get_field_fn="DP_text_reader_get_array",
    parse_constructor_fn=DrawdanceFieldType.parse_text_int32_list,
)

DrawdanceStringFieldType.declare(
    key="String",
)

DrawdanceStringWithLengthFieldType.declare(
    key="StringWithLength",
)

DrawdanceStructFieldType.declare(
    key="struct",
)


# Special case for the UserJoin message having two array fields that need to be
# combined into a single flexible array member. Only this particular combination
# is implemented right now, others would need to consider alignment and such.
class StringBytesFieldCombination:
    def __init__(self, string_field, bytes_field):
        self.string_field = string_field
        self.bytes_field = bytes_field
        self.field_name = f"{self.string_field.name}_{self.bytes_field.name}"

    @property
    def array_declaration(self):
        return f"unsigned char {self.field_name}[]"

    @property
    def array_field_size(self):
        return f"{self.string_field.array_field_size} + {self.bytes_field.array_field_size}"


# For title-case sequences of uppercase characters, e.g. ACL => Acl.
PURE_NAME_RE = re.compile(r"([A-Z])([A-Z]+)")


def purify_name(s):
    return PURE_NAME_RE.sub(lambda m: f"{m.group(1)}{m.group(2).lower()}", s)


# Turn CamelCase into snake_case.
DECAMELIZE_RE = re.compile(r"([a-z])([A-Z])")

# Special case: don't turn MyPaint into my_paint, use mypaint instead.
# This is analogous to how libmypaint does it, they don't split it either.
MYPAINT_RE = re.compile(r"(^|_)my_paint($|_)")


def decamelize(s):
    return MYPAINT_RE.sub(
        lambda m: f"{m.group(1)}mypaint{m.group(2)}",
        DECAMELIZE_RE.sub(lambda m: f"{m.group(1)}_{m.group(2).lower()}", s).lower(),
    )


def param_name(s):
    return "".join(filter(lambda c: c.isupper(), s)).lower()


class DrawdanceCompat:
    def __init__(self, parent, compat):
        self.parent = parent
        self.fields = [DrawdanceField(self, f) for f in compat.fields]
        self.deserialize_fields = compat.deserialize_fields
        self.min_length = compat.min_len
        self.max_length = compat.max_len
        array_fields = [f for f in self.fields if f.type.is_array]
        if len(array_fields) == 0:
            self.array_field = None
        elif len(array_fields) == 1:
            self.array_field = array_fields[0]
        else:
            raise ValueError(f"Unhandled compat array field combination: {array_fields}")

        fields_with_subfields = [f for f in self.fields if f.sub]
        if len(fields_with_subfields) == 0:
            self.field_with_subfields = None
        elif len(fields_with_subfields) == 1:
            self.field_with_subfields = fields_with_subfields[0]
        else:
            raise ValueError(
                f"Multiple compat fields with subfields: {fields_with_subfields}"
            )

    @property
    def is_compat(self):
        return True

    @property
    def static_payload_length(self):
        return sum(f.static_payload_length for f in self.fields)

    @property
    def dynamic_payload_length(self):
        return " + ".join(filter(bool, [f.dynamic_payload_length for f in self.fields]))

    @property
    def param_name(self):
        return self.parent.param_name


class DrawdanceMessage:
    def __init__(self, message):
        self.id = message.id
        self.name = message.name
        self.alias = message.alias
        self.reserved = message.reserved
        self.local_match = message.local_match
        self.incompatible = message.incompatible
        self.dirties_canvas = message.dirties_canvas
        self.comment = "\n".join(
            f" * {c}".rstrip() for c in message.comment.strip().split("\n")
        )
        self.message_type = message.message_type
        self.cmd_name = message.cmd_name
        pure_name = purify_name(self.name)
        decamelized_name = decamelize(pure_name)
        self.enum_name = "DP_MSG_" + decamelized_name.upper()
        self.func_name = "msg_" + decamelized_name
        self.param_name = "m" + param_name(pure_name)
        self.fields = [DrawdanceField(self, f) for f in message.fields]
        if not self.alias:
            self.struct_name = "DP_Msg" + pure_name
            self.param = self.struct_name + " *" + self.param_name
            self.min_length = message.min_len
            self.max_length = message.max_len
            array_fields = [f for f in self.fields if f.type.is_array]
            if len(array_fields) == 0:
                self.array_field = None
            elif len(array_fields) == 1:
                self.array_field = array_fields[0]
            # Special case: user join message that has a string and a byte array.
            elif (
                len(array_fields) == 2
                and isinstance(array_fields[0].type, DrawdanceStringFieldType)
                and isinstance(array_fields[1].type, DrawdanceArrayFieldType)
                and array_fields[1].type.base_type == "unsigned char"
            ):
                array_fields[0].type = DrawdanceFieldType.TYPES["StringWithLength"]
                array_fields[1].offset_field = array_fields[0]
                self.array_field = StringBytesFieldCombination(*array_fields)
                array_fields[0].field_name = self.array_field.field_name
                array_fields[1].field_name = self.array_field.field_name
            else:
                raise ValueError(f"Unhandled array field combination: {array_fields}")

            fields_with_subfields = [f for f in self.fields if f.sub]
            if len(fields_with_subfields) == 0:
                self.field_with_subfields = None
            elif len(fields_with_subfields) == 1:
                self.field_with_subfields = fields_with_subfields[0]
            else:
                raise ValueError(
                    f"Multiple fields with subfields: {fields_with_subfields}"
                )

            self.compat = DrawdanceCompat(self, message.compat) if message.compat else None

    def init_alias(self, messages):
        if self.alias:
            for m in messages:
                if m.name == self.alias:
                    self.alias_message = m
                    self.struct_name = m.struct_name
                    self.param = self.struct_name + " *" + self.param_name
                    self.effective_fields = m.fields
                    self.compat = m.compat
                    self.min_length = m.min_length
                    self.max_length = m.max_length
                    self.array_field = m.array_field
                    self.field_with_subfields = m.field_with_subfields
                    break
        else:
            self.alias_message = None
            self.effective_fields = self.fields

    def local_match_condition(self, subject):
        checks = []
        for field in self.local_match.get("layer_ids", []):
            checks.append(f"local_layer_id({subject}->{field})")
        for field in self.local_match.get("selection_ids", []):
            checks.append(f"local_selection_id({subject}->{field})")
        return " || ".join(checks)

    @property
    def is_compat(self):
        return False

    @property
    def static_payload_length(self):
        return sum(f.static_payload_length for f in self.fields)

    @property
    def static_payload_length_compat(self):
       if self.compat:
           return self.compat.static_payload_length
       else:
            return self.static_payload_length

    @property
    def local_match_length(self):
        if self.local_match:
            return sum(f.local_match_length for f in self.fields)
        else:
            raise ValueError("Message local match is false")

    @property
    def dynamic_payload_length(self):
        return " + ".join(filter(bool, [f.dynamic_payload_length for f in self.fields]))

    @property
    def dynamic_payload_length_compat(self):
        if self.compat:
            return self.compat.dynamic_payload_length
        else:
            return self.dynamic_payload_length

    @property
    def array_declaration(self):
        return self.array_field.array_declaration

    @property
    def array_field_name(self):
        return self.array_field.field_name

    @property
    def array_field_size(self):
        return self.array_field.array_field_size


class DrawdanceField:
    def __init__(self, message, field):
        self.message = message
        self.name = field.name
        self.field_name = self.name
        self.func_name = self.name
        self.type = DrawdanceFieldType.TYPES[field.field_type]
        self.offset_field = None
        self.convert = field.convert
        self.convert_args = field.convert_args

        if hasattr(field, "format") and field.format:
            self.format = field.format
        else:
            self.format = None

        if hasattr(field, "flags") and field.flags:
            self.flags = [DrawdanceFlag(message, self, flag) for flag in field.flags]
            if self.format:
                raise RuntimeError(f"Conflicting format for flags: {self.format}")
            else:
                self.format = "flags"
        else:
            self.flags = None

        if hasattr(field, "variants") and field.variants:
            self.variants = [
                DrawdanceVariant(message, self, variant, value)
                for value, variant in enumerate(field.variants)
            ]
        else:
            self.variants = None

        if hasattr(field, "subfields") and field.subfields:
            self.sub = DrawdanceSubStruct(message, field)
        else:
            self.sub = None

        divisor = self.deserialize_field_divisor
        if divisor:
            self.min = field.min_len // divisor
            self.max = field.max_len // divisor
        else:
            self.min = field.min_len
            self.max = field.max_len

    def access(self, subject):
        return self.type.access(self, subject)

    def access_size(self, subject):
        return self.type.access_size(self, subject)

    @property
    def accessor_return_type(self):
        return self.type.accessor_return_type(self)

    @property
    def accessor_out_param(self):
        return self.type.accessor_out_param(self)

    @property
    def accessor_out_param_condition(self):
        return self.type.accessor_out_param_condition(self)

    def accessor_out_param_assign(self, subject):
        return self.type.accessor_out_param_assign(self, subject)

    @property
    def constructor_param(self):
        return self.type.constructor_param(self)

    @property
    def deserialize_field_size(self):
        return self.type.deserialize_field_size(self)

    @property
    def deserialize_field_divisor(self):
        return self.type.deserialize_field_divisor(self)

    @property
    def deserialize_field_count(self):
        return self.type.deserialize_field_count(self)

    @property
    def deserialize_field_check_remaining(self):
        return self.type.deserialize_field_check_remaining(self)

    @property
    def deserialize_field(self):
        return self.type.deserialize_field(self)

    @property
    def deserialize_constructor_arg(self):
        return self.type.deserialize_constructor_arg(self)

    @property
    def parse_field_count(self):
        return self.type.parse_field_count(self)

    @property
    def parse_field(self):
        return self.type.parse_field(self)

    def parse_subfield(self, index):
        return self.type.parse_subfield(self, index)

    @property
    def parse_constructor_arg(self):
        return self.type.parse_constructor_arg(self)

    @property
    def static_payload_length(self):
        return self.type.static_payload_length(self)

    @property
    def dynamic_payload_length(self):
        return self.type.dynamic_payload_length(self)

    @property
    def local_match_length(self):
        return self.type.local_match_length(self)

    @property
    def struct_declaration(self):
        return self.type.struct_declaration(self)

    @property
    def array_declaration(self):
        return self.type.array_declaration(self)

    @property
    def array_field_size(self):
        return self.type.array_field_size(self)

    @property
    def array_size_name(self):
        return self.type.array_size_name(self)

    @property
    def array_size_type(self):
        return self.type.array_size_type(self)

    @property
    def equals(self):
        return self.type.equals(self)

    def serialize_payload(self, subject, dst):
        return self.type.serialize_payload(self, subject, dst)

    def serialize_payload_compat(self, subject, dst):
        return self.type.serialize_payload_compat(self, subject, dst)

    def serialize_local_match(self, subject, dst):
        return self.type.serialize_local_match(self, subject, dst)

    def match_local_match(self, subject):
        return self.type.match_local_match(self, subject)

    def write_payload_text(self, subject):
        return self.type.write_payload_text(self, subject)

    def write_subfield_payload_text(self, subject):
        return self.type.write_subfield_payload_text(self, subject)

    def assign(self, subject):
        return self.type.assign(self, subject)

    def array_assign(self, subject):
        return self.type.array_assign(self, subject)

    def array_check(self, subject):
        return self.type.array_check(self, subject)

    def offset_expr(self, subject):
        return self.type.offset_expr(self, subject)


class DrawdanceFlag:
    def __init__(self, message, field, flag):
        (name, value) = flag
        self.name = name
        self.define_name = (
            message.enum_name + "_" + field.name.upper() + "_" + name.upper()
        )
        self.value = value


class DrawdanceVariant:
    def __init__(self, message, field, variant, value):
        self.name = variant
        self.define_name = (
            message.enum_name
            + "_"
            + field.name.upper()
            + "_"
            + decamelize(variant).upper()
        )
        self.value = value


class DrawdanceSubStruct:
    def __init__(self, message, field):
        self.fields = [DrawdanceField(message, f) for f in field.subfields]
        self.item_length = field.item_len
        self.min_length = field.min_len
        self.max_length = field.max_len
        self.max_items = field.max_items
        self.func_name = decamelize(purify_name(field.struct_name))
        self.struct_name = "DP_" + field.struct_name
        self.param_name = param_name(field.struct_name)
        self.param = self.struct_name + " *" + self.param_name


def load_template_file(template_file_name):
    with open(os.path.join(os.path.dirname(__file__), template_file_name)) as fh:
        return jinja2.Template(fh.read(), trim_blocks=True, lstrip_blocks=True, undefined=jinja2.StrictUndefined)


def parse_version(version_string):
    match = re.search(r"^(\w+):(\d+).(\d+).(\d+)$", version_string)
    if not match:
        raise ValueError(f"Invalid version: '{version_string}'")
    return {
        "string": version_string,
        "namespace": match.group(1),
        "server": int(match.group(2)),
        "major": int(match.group(3)),
        "minor": int(match.group(4)),
    }

def render(path, template_file_name, protocol, messages, version, compat_version):
    template = load_template_file(template_file_name)
    with open(path, "w") as fh:
        fh.write(
            template.render(
                messages=messages,
                message_types=protogen.MSG_TYPES,
                version=version,
                compat_version=compat_version,
                undo_depth=protocol["undo_depth"],
            )
        )
        fh.write("\n")


if __name__ == "__main__":
    argc = len(sys.argv)
    if argc < 2 or argc > 3:
        raise Exception(f"Usage: {sys.argv[0]} OUTPUT_DIR [PROTOCOL_YAML_PATH]")

    output_dir = sys.argv[1]
    protocol = protogen.load_protocol_definition(
        sys.argv[2]
        if argc >= 3
        else os.path.join(os.path.dirname(__file__), "protocol.yaml")
    )

    version = parse_version(protocol['version'])
    compat_version = parse_version(protocol['compat_version']) if 'compat_version' in protocol else None

    messages = [DrawdanceMessage(m) for m in protocol["messages"]]
    for m in messages:
        m.init_alias(messages)

    for target in ["messages.h", "messages.c"]:
        render(os.path.join(output_dir, target), f"{target}.jinja", protocol, messages, version, compat_version)
