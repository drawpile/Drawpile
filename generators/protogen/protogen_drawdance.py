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

    def accessor_out_param(self, f):
        return None

    def static_payload_length(self, f):
        return 0

    def dynamic_payload_length(self, f):
        return None

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

    def write_text_uint(self, f, subject):
        a = self.access(f, subject)
        fmt = f.format
        if not fmt:
            return f'DP_text_writer_write_uint(writer, "{f.name}", {a}, false)'
        elif fmt == "hex":
            return f'DP_text_writer_write_uint(writer, "{f.name}", {a}, true)'
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
        fmt = f.format
        if not fmt:
            hex_arg = "false"
        elif fmt == "hex":
            hex_arg = "true"
        else:
            raise RuntimeError(f"Unknown uint16 list format '{fmt}'")
        return f'DP_text_writer_write_uint16_list(writer, "{f.name}", {a}, {a_count}, {hex_arg})'

    def write_subfield_text_int(self, f, subject):
        a = self.access(f, subject)
        fmt = f.format
        if not fmt:
            return f"DP_text_writer_write_subfield_int(writer, {a})"
        elif fmt == "div4":
            return f"DP_text_writer_write_subfield_decimal(writer, (double){a} / 4.0)"
        else:
            raise RuntimeError(f"Unknown int subfield format '{fmt}'")

    def write_subfield_text_uint(self, f, subject):
        a = self.access(f, subject)
        fmt = f.format
        if not fmt:
            return f"DP_text_writer_write_subfield_uint(writer, {a})"
        elif fmt == "div256":
            return f"DP_text_writer_write_subfield_decimal(writer, (double){a} / 256.0)"
        else:
            raise RuntimeError(f"Unknown uint subfield format '{fmt}'")


class DrawdancePlainFieldType(DrawdanceFieldType):
    def __init__(
        self,
        key,
        base_type,
        payload_length,
        serialize_payload_fn,
        write_payload_text_fn,
        deserialize_payload_fn,
        write_subfield_payload_text_fn=None,
    ):
        super().__init__(key, False)
        self.base_type = base_type
        self.payload_length = payload_length
        self.serialize_payload_fn = serialize_payload_fn
        self.write_payload_text_fn = write_payload_text_fn
        self.write_subfield_payload_text_fn = write_subfield_payload_text_fn
        self.deserialize_payload_fn = deserialize_payload_fn

    def accessor_return_type(self, f):
        return self.base_type

    def constructor_param(self, f):
        return f"{self.base_type} {f.name}"

    def deserialize_field(self, f):
        return f"{self.base_type} {f.name} = {self.deserialize_payload_fn}(buffer + read, &read);"

    def deserialize_constructor_arg(self, f):
        return f.name

    def static_payload_length(self, f):
        return self.payload_length

    def struct_declaration(self, f):
        return f"{self.base_type} {f.name}"

    def equals(self, f):
        return f"a->{f.field_name} == b->{f.field_name}"

    def serialize_payload(self, f, subject, dst):
        a = self.access(f, subject)
        return f"{self.serialize_payload_fn}({a}, {dst})"

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
        return f"{self.deserialize_payload_fn}, {f.name}_{self.array_size_name(f)}, {f.name}_user"

    def dynamic_payload_length(self, f):
        base = self._to_size(
            f"{f.message.param_name}->{f.name}_{self.array_size_name(f)}"
        )
        return base + self.multiplier

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
        return f"{self.serialize_payload_fn}({a}, {a_count}, {dst})"

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

    def dynamic_payload_length(self, f):
        return f"DP_uint16_to_size({f.message.param_name}->{f.name}_len)"

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
        return f"{f.sub.func_name}_deserialize, {f.name}_count, {f.name}_user"

    def dynamic_payload_length(self, f):
        payload_length = sum([sf.static_payload_length for sf in f.sub.fields])
        multiplier = "" if payload_length == 1 else f" * {payload_length}"
        return f"DP_int_to_size({f.message.param_name}->{f.name}_count){multiplier}"

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
)

DrawdancePlainFieldType.declare(
    key="argb32",
    base_type="uint32_t",
    payload_length=4,
    serialize_payload_fn="DP_write_bigendian_uint32",
    write_payload_text_fn="DP_text_writer_write_argb_color",
    deserialize_payload_fn="read_uint32",
)

DrawdancePlainFieldType.declare(
    key="bool",
    base_type="bool",
    payload_length=1,
    serialize_payload_fn="DP_write_bigendian_uint8",
    write_payload_text_fn="DP_text_writer_write_bool",
    deserialize_payload_fn="read_bool",
)

DrawdancePlainFieldType.declare(
    key="i8",
    base_type="int8_t",
    payload_length=1,
    serialize_payload_fn="DP_write_bigendian_int8",
    write_payload_text_fn=DrawdanceFieldType.write_text_int,
    write_subfield_payload_text_fn=DrawdanceFieldType.write_subfield_text_int,
    deserialize_payload_fn="read_int8",
)

DrawdancePlainFieldType.declare(
    key="i16",
    base_type="int16_t",
    payload_length=2,
    serialize_payload_fn="DP_write_bigendian_int16",
    write_payload_text_fn=DrawdanceFieldType.write_text_int,
    write_subfield_payload_text_fn=DrawdanceFieldType.write_subfield_text_int,
    deserialize_payload_fn="read_int16",
)

DrawdancePlainFieldType.declare(
    key="i32",
    base_type="int32_t",
    payload_length=4,
    serialize_payload_fn="DP_write_bigendian_int32",
    write_payload_text_fn=DrawdanceFieldType.write_text_int,
    write_subfield_payload_text_fn=DrawdanceFieldType.write_subfield_text_int,
    deserialize_payload_fn="read_int32",
)

DrawdancePlainFieldType.declare(
    key="u8",
    base_type="uint8_t",
    payload_length=1,
    serialize_payload_fn="DP_write_bigendian_uint8",
    write_payload_text_fn=DrawdanceFieldType.write_text_uint,
    write_subfield_payload_text_fn=DrawdanceFieldType.write_subfield_text_uint,
    deserialize_payload_fn="read_uint8",
)

DrawdancePlainFieldType.declare(
    key="u16",
    base_type="uint16_t",
    payload_length=2,
    serialize_payload_fn="DP_write_bigendian_uint16",
    write_payload_text_fn=DrawdanceFieldType.write_text_uint,
    write_subfield_payload_text_fn=DrawdanceFieldType.write_subfield_text_uint,
    deserialize_payload_fn="read_uint16",
)

DrawdancePlainFieldType.declare(
    key="u32",
    base_type="uint32_t",
    payload_length=4,
    serialize_payload_fn="DP_write_bigendian_uint32",
    write_payload_text_fn=DrawdanceFieldType.write_text_uint,
    write_subfield_payload_text_fn=DrawdanceFieldType.write_subfield_text_uint,
    deserialize_payload_fn="read_uint32",
)

DrawdanceArrayFieldType.declare(
    key="Bytes",
    base_type="unsigned char",
    size_type="size_t",
    payload_length=1,
    serialize_payload_fn="write_bytes",
    write_payload_text_fn="DP_text_writer_write_base64",
    deserialize_payload_fn="read_bytes",
)

DrawdanceArrayFieldType.declare(
    key="Vec<u8>",
    base_type="uint8_t",
    size_type="int",
    payload_length=1,
    serialize_payload_fn="DP_write_bigendian_uint8_array",
    write_payload_text_fn="DP_text_writer_write_uint8_list",
    deserialize_payload_fn="read_uint8_array",
)

DrawdanceArrayFieldType.declare(
    key="Vec<u16>",
    base_type="uint16_t",
    size_type="int",
    payload_length=2,
    serialize_payload_fn="DP_write_bigendian_uint16_array",
    write_payload_text_fn=DrawdanceFieldType.write_text_uint16_list,
    deserialize_payload_fn="read_uint16_array",
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


class DrawdanceMessage:
    def __init__(self, message):
        self.id = message.id
        self.name = message.name
        self.alias = message.alias
        self.reserved = message.reserved
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

    def init_alias(self, messages):
        if self.alias:
            for m in messages:
                if m.name == self.alias:
                    self.alias_message = m
                    self.struct_name = m.struct_name
                    self.param = self.struct_name + " *" + self.param_name
                    self.effective_fields = m.fields
                    self.min_length = m.min_length
                    self.max_length = m.max_length
                    self.array_field = m.array_field
                    break
        else:
            self.alias_message = None
            self.effective_fields = self.fields

    @property
    def static_payload_length(self):
        return sum(f.static_payload_length for f in self.fields)

    @property
    def dynamic_payload_length(self):
        return " + ".join(filter(bool, [f.dynamic_payload_length for f in self.fields]))

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
    def static_payload_length(self):
        return self.type.static_payload_length(self)

    @property
    def dynamic_payload_length(self):
        return self.type.dynamic_payload_length(self)

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
        return jinja2.Template(fh.read(), trim_blocks=True, lstrip_blocks=True)


def render(path, template_file_name, protocol, messages):
    template = load_template_file(template_file_name)
    with open(path, "w") as fh:
        fh.write(
            template.render(
                messages=messages,
                message_types=protogen.MSG_TYPES,
                version=protocol["version"],
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

    messages = [DrawdanceMessage(m) for m in protocol["messages"]]
    for m in messages:
        m.init_alias(messages)

    for target in ["messages.h", "messages.c"]:
        render(os.path.join(output_dir, target), f"{target}.jinja", protocol, messages)
