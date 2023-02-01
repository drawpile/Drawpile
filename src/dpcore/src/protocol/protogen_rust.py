#!/usr/bin/env python3

from jinja2 import Template

from protogen import load_protocol_definition, MSG_TYPES

template = Template("""
// Message definitions generated with protogen-rust.py

// allow some stylistic errors that make code generations simpler
#![allow(clippy::needless_borrow, clippy::uninlined_format_args)]

use super::serialization::{MessageReader, MessageWriter, DeserializationError};
use super::textmessage::TextMessage;
use std::fmt;
use std::str::FromStr;
use num_enum::IntoPrimitive;
use num_enum::TryFromPrimitive;

pub static PROTOCOL_VERSION: &str = "{{ version }}";
pub const UNDO_DEPTH: u32 = {{ undo_depth }};

{# ### STRUCTS FOR MESSAGES WITH NONTRIVIAL PAYLOADS ### #}
{% for message in messages %}{% if message.fields|length > 1 %}

{# THE STRUCT FIELD TYPE IS USED FOR DAB DATA ONLY (AT THE MOMENT) #}
{% if message.fields[-1].subfields %}{% set sfield = message.fields[-1] %}
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct {{ sfield.struct_name }} {
    {% for f in sfield.subfields %}
    pub {{ f.name }}: {{ field_rust_type(f) }},
    {% endfor %}
}
{% endif %}

{# ENUM TYPE #}
{% for f in message.fields %}
{% if f.variants %}
#[derive(Copy, Clone, Debug, Eq, PartialEq, IntoPrimitive, TryFromPrimitive)]
#[repr(u8)]
pub enum {{ f.enum_name }} {
{% for v in f.variants %}{{ v }},{% endfor %}
}
{% endif %}
{% endfor %}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct {{ message.name }}Message {
    {% for f in message.fields %}
    pub {{ f.name }}: {{ field_rust_type(f) }},
    {% endfor %}
}

impl {{ message.name }}Message {
    {# ADD MAX STRUCT ITEM COUNT CONSTANT #}
    {% if message.fields[-1].max_items %}
    pub const MAX_{{ message.fields[-1].struct_name|upper }}S: usize = {{ message.fields[-1].max_items }};
    {% endif %}

    {# ADD FLAG CONSTANTS #}
    {% for field in message.fields %}{% if field.flags %}
    {% for flagname, flagbit in field.flags %}
    pub const {{ field.name.upper() }}_{{ flagname.upper() }}: {{ field.field_type }} = 0x{{ '%.x' | format(flagbit) }};
    {% endfor %}
    pub const {{ field.name.upper() }}: &'static [&'static str] = &[
    {% for flagname, flagbit in field.flags %}"{{ flagname }}", {% endfor %}];
    {% endif %}{% endfor %}

    {# (DE)SERIALIZATION FUNCTIONS #}
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate({{ message.min_len }}, {{ message.max_len }})?;

        {% for field in message.fields %}
        {% if field.subfields %}
        let mut {{ field.name }} = Vec::<{{ field.struct_name }}>::with_capacity(reader.remaining() / {{ field.min_len }});
        while reader.remaining() > 0 {
            {% for subfield in field.subfields %}
            let {{ subfield.name }} = reader.{{ read_field(subfield.field_type) }};
            {% endfor %}
            {{ field.name }}.push({{ field.struct_name}} {
                {%+ for subfield in field.subfields %}{{ subfield.name }},{% endfor -%}
            });
        }{% elif field.prefix_type %}
        let {{ field.name }}_len = reader.{{ read_field(field.prefix_type) }} as usize;
        if reader.remaining() < {{ field.name }}_len {
            return Err(DeserializationError::InvalidField("{{ message.name }}::{{ field.name }} field is too long"));
        }
        let {{ field.name }} = reader.{{ read_field(field.field_type, field.name + "_len") }};
        {% else %}
        let {{ field.name }} = reader.{{ read_field(field.field_type) }};
        {% endif -%}
        {% endfor %}{# field in fields #}

        Ok(Self {
            {% for f in message.fields %}{{ f.name }},{% endfor %}
        })
    }

    fn serialize(&self, w: &mut MessageWriter, {% if message.is_aliased %}msg_id: u8,{%endif %} user_id: u8) {
        w.write_header({% if message.is_aliased %}msg_id{% else %}{{ message.id }}{% endif %}, user_id, {{ payload_len(message) }});
        {% for field in message.fields %}
        {% if field.subfields %}
        for item in self.{{ field.name }}.iter() {
            {% for subfield in field.subfields %}
            w.{{ write_field(subfield.field_type, 'item.' + subfield.name) }};
            {% endfor %}
        }
        {% else %}
        {% if field.prefix_type %}
        w.{{ write_field(field.prefix_type, 'self.' + field.name + '.len() as ' + field.prefix_type) }};
        {% endif %}
        w.{{ write_field(field.field_type, 'self.' + field.name) }};
        {% endif %}{# struct or normal field#}
        {% endfor %}{# field in message.fields #}
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        {% if message.fields[-1].subfields %}{% set dabfield = message.fields[-1] %}
        let mut dabs: Vec<Vec<f64>> = Vec::with_capacity(self.{{ dabfield.name }}.len());
        for dab in self.{{ dabfield.name }}.iter() {
            dabs.push(vec![
                {% for d in dabfield.subfields %}
                dab.{{ d.name }} as f64{% if d.format.startswith('div') %} / {{ d.format[3:] }}.0{% endif %},
                {% endfor %}
            ]);
        }
        {% endif %}{# dabfield #}
        txt
        {% for field in message.fields %}
        {% if field.subfields %}
            .set_dabs(dabs)
        {% else %}
            .{{ textmessage_setfield(field, 'self.' + field.name) }}
        {% endif %}
        {% endfor %}{# field in message.fields #}
    }

    fn from_text(tm: &TextMessage) -> Self {
        {% if message.fields[-1].subfields %}{% set dabfield = message.fields[-1] %}
        let mut dab_structs: Vec<{{ dabfield.struct_name }}> = Vec::with_capacity(tm.dabs.len());
        for dab in tm.dabs.iter() {
            if dab.len() != {{ dabfield.subfields|length }} { continue; }
            dab_structs.push({{ dabfield.struct_name }} {
                {% for d in dabfield.subfields %}
                {{ d.name }}: (dab[{{ loop.index0 }}]{% if d.format.startswith('div') %} * {{ d.format[3:] }}.0{% endif %}) as {{ field_rust_type(d) }},
                {% endfor %}{# dab subfield #}
            });
        }
        {% endif %}{# dabfield #}

        Self{
            {% for field in message.fields %}
            {{ field.name }}: {% if field.subfields %}dab_structs{% else %}{{ textmessage_getfield('tm', field) }}{% endif %},
            {% endfor %}
        }
    }
}

{% endif %}{% endfor %}{# messages with more than one field #}

{% for message_type in message_types %}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum {{ message_type }}Message {
    {% for message in messages %}{% if message.message_type == message_type %}
    {{ comment(message.comment) }}
    {% if message.alias %}
    {{ message.name }}(u8, {{ message.alias }}Message),
    {% elif message.fields|length > 1 %}
    {{ message.name }}(u8, {{ message.name }}Message),
    {% elif message.fields %}
    {{ message.name }}(u8, {{ field_rust_type(message.fields[0]) }}),
    {% else %}
    {{ message.name }}(u8),
    {% endif %}

    {% endif %}{% endfor %}{# messages #}
}
{% endfor %}{# normal_message_types #}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Message {
    {% for mt in message_types %}{{ mt }}({{ mt }}Message),{% endfor %}
}

{% for message_type in message_types %}
impl {{ message_type }}Message {
    pub fn write(&self, w: &mut MessageWriter) {
        use {{ message_type }}Message::*;
        match &self {
            {% for message in messages %}{% if message.message_type == message_type %}
            {% if message.alias or message.fields|length > 1 %}
            {{ message.name }}(user_id, b) => b.serialize(w, {% if message.is_aliased %}{{ message.id }},{% endif %} *user_id),
            {% elif message.fields %}
            {{ message.name }}(user_id, b) => w.single({{ message.id }}, *user_id, {{ deref_primitive(message.fields[0]) }}b),
            {% else %}
            {{ message.name }}(user_id) => w.write_header({{ message.id }}, *user_id, 0),
            {% endif %}
            {% endif %}{% endfor %}{# message in messages #}
        }
    }

    pub fn as_text(&self) -> TextMessage {
        use {{ message_type }}Message::*;
        match &self {
            {% for message in messages %}{% if message.message_type == message_type %}
            {% if message.alias or message.fields|length > 1%}
            {{ message.name }}(user_id, b) => b.to_text(TextMessage::new(*user_id, "{{ message.cmd_name }}")),
            {% elif message.fields %}
            {{ message.name }}(user_id, b) => TextMessage::new(*user_id, "{{ message.cmd_name }}").{{ textmessage_setfield(message.fields[0], 'b') }},
            {% else %}
            {{ message.name }}(user_id) => TextMessage::new(*user_id, "{{ message.cmd_name }}"),
            {% endif %}
            {% endif %}{% endfor %}{# message in messages #}
        }
    }

    pub fn user(&self) -> u8 {
        use {{ message_type }}Message::*;
        match &self {
            {% for message in messages %}{% if message.message_type == message_type %}
            {% if message.alias or message.fields %}
            {{ message.name }}(user_id, _) => *user_id,
            {% else %}
            {{ message.name }}(user_id) => *user_id,
            {% endif %}
            {% endif %}{% endfor %}{# message in messages #}
        }
    }
}

impl fmt::Display for {{ message_type }}Message {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.as_text().fmt(f)
    }
}

impl From<{{ message_type }}Message> for Message {
    fn from(item: {{ message_type}}Message) -> Message {
        Message::{{ message_type }}(item)
    }
}

{% endfor %}{# message_type #}

impl Message {
    pub fn deserialize(buf: &[u8]) -> Result<Message, DeserializationError> {
        let mut r = MessageReader::new(buf);
        Message::read(&mut r)
    }

    pub fn serialize(&self) -> Vec<u8> {
        let mut w = MessageWriter::new();
        self.write(&mut w);
        w.into()
    }

    pub fn read(r: &mut MessageReader) -> Result<Message, DeserializationError> {
        let (message_type, u) = r.read_header()?;

        use Message::*;
        Ok(match message_type {
            {% for message in messages %}
            {{ message.id }} => {{ message.message_type }}({{ message.message_type }}Message::{{ message.name }}(u,
                {% if message.alias %}
                    {{ message.alias }}Message::deserialize(r)?
                {% elif message.fields|length > 1 %}
                    {{ message.name }}Message::deserialize(r)?
                {% elif message.fields %}
                    r.validate({{ message.min_len }}, {{ message.max_len }})?.{{ read_field(message.fields[0].field_type) }}
                {% endif %})),
            {% endfor %}{# message in messages #}
            _ => {
                return Err(DeserializationError::UnknownMessage(u, message_type, r.remaining()));
            }
        })
    }

    pub fn write(&self, w: &mut MessageWriter) {
        use Message::*;
        match &self {
            {% for mt in message_types %}{{ mt }}(m) => m.write(w),{% endfor %}
        }
    }

    pub fn from_text(tm: &TextMessage) -> Option<Message> {
        // tm.user_id
        use Message::*;
        Some(match tm.name.as_ref() {
            {% for message in messages %}
            "{{ message.cmd_name }}" => {{ message.message_type }}({{ message.message_type }}Message::{{ message.name }}(tm.user_id,
            {% if message.alias or message.fields|length > 1%}
                {{ message.alias or message.name }}Message::from_text(tm)
            {% elif message.fields %}
                {{ textmessage_getfield('tm', message.fields[0]) }}
            {% endif %})),
            {% endfor %}{# message in messages #}
            _ => { return None; }
        })
    }

    pub fn as_text(&self) -> TextMessage {
        use Message::*;
        match &self {
            {% for mt in message_types %}{{ mt }}(m) => m.as_text(),{% endfor %}
        }
    }
}

impl fmt::Display for Message {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.as_text().fmt(f)
    }
}

""",
    trim_blocks=True,
    lstrip_blocks=True,
)

def field_rust_type(field):
    if field.field_type == 'Bytes':
        return 'Vec<u8>'
    elif field.field_type == 'struct':
        return f'Vec<{field.struct_name}>'
    elif field.field_type == 'blendmode':
        return 'u8'
    elif field.field_type == 'argb32':
        return 'u32'

    return field.field_type

def read_field(ftype, length=None):
    if ftype == 'String':
        if length is None:
            return f"read_remaining_str()"

        return f"read_str({length})"

    elif ftype in ('Vec<u8>', 'Vec<u16>', 'Bytes'):
        if ftype == 'Bytes':
            ftype = 'u8'
        else:
            ftype = ftype[4:2]

        if length is None:
            return f"read_remaining_vec::<{ftype}>()"

        return f"read_vec::<{ftype}>({length})"

    elif ftype == 'blendmode':
        ftype = 'u8'

    elif ftype == 'argb32':
        ftype = 'u32'

    return f"read::<{ftype}>()"


def write_field(ftype, value):
    if ftype in ('String', 'Vec<u8>', 'Vec<u16>', 'Bytes'):
        value = "&" + value

    return f"write({value})"


def deref_primitive(field):
    if field.field_type in ('u8', 'u16', 'u32', 'i32', 'bool'):
        return '*'
    return ''


def textmessage_setfield(field, name):
    if field.field_type == 'String':
        setter = 'set'
        name = name + ".clone()"
    elif field.field_type == 'Bytes':
        setter = 'set_bytes'
        name = '&' + name
    elif field.field_type == 'Vec<u8>':
        setter = 'set_vec_u8'
        name = '&' + name
    elif field.field_type == 'Vec<u16>':
        setter = 'set_vec_u16'
        name = '&' + name + (", true" if field.format == 'hex' else ", false")
    elif field.field_type == 'argb32':
        setter = 'set_argb32'
    elif field.field_type == 'blendmode':
        setter = 'set_blendmode'
    elif hasattr(field, 'flags'):
        return f'set_flags("{field.name}", &Self::{field.name.upper()}, {name})'
    else:
        setter = 'set'
        if field.format.startswith("div"):
            name = f"({name} as f64 / {field.format[3:]}.0).to_string()"
        elif field.format == 'hex':
            digits = int(field.field_type[1:]) // 4 # [ui](8|16|32) -> 2|4|8
            name = f'format!("0x{{:0{digits}x}}", {name})'
        else:
            name = name + ".to_string()"

    return f'{setter}("{field.name}", {name})'


def textmessage_getfield(var, field):
    if field.field_type == 'Bytes':
        getter = 'get_bytes'
    elif field.field_type == 'String':
        return f'{var}.get_str("{field.name}").to_string()'
    elif field.field_type == 'Vec<u8>':
        getter = 'get_vec_u8'
    elif field.field_type == 'Vec<u16>':
        getter = 'get_vec_u16'
    elif field.field_type == 'argb32':
        getter = 'get_argb32'
    elif field.field_type == 'blendmode':
        getter = 'get_blendmode'
    elif field.field_type == 'bool':
        return f'{var}.get_str("{field.name}") == "true"'
    elif hasattr(field, 'flags'):
        return f'{var}.get_flags(&Self::{field.name.upper()}, "{field.name}")'
    elif field.field_type in ("u8", "u16", "u32", "i8", "i16", "i32"):
        if field.format.startswith("div"):
            return f'({var}.get_f64("{field.name}") * {field.format[3:]}.0) as {field.field_type}'
        elif field.field_type.startswith("u"):
            return f'{var}.get_{field.field_type}("{field.name}")'
        else:
            return f'{field.field_type}::from_str({var}.get_str("{field.name}")).unwrap_or_default()'
    else:
        raise ValueError("Unhandled textfield get type: " + field.field_type)

    return f'{var}.{getter}("{field.name}")';


def payload_len(message):
    fixed, vectors = message.length()

    if not vectors:
        return fixed

    total = [str(fixed)]

    for vec in vectors:
        if vec.item_len > 1:
            total.append(f'(self.{vec.name}.len() * {vec.item_len})')
        else:
            total.append(f'self.{vec.name}.len()')

    return '+'.join(total)


def comment(comments):
    return '\n'.join('/// ' + c for c in comments.split('\n'))


if __name__ == '__main__':
    protocol = load_protocol_definition()

    print(template.render(
        messages=[m for m in protocol['messages'] if not m.reserved],
        message_types=MSG_TYPES,
        version=protocol['version'],
        undo_depth=protocol['undo_depth'],
        field_rust_type=field_rust_type,
        read_field=read_field,
        write_field=write_field,
        deref_primitive=deref_primitive,
        payload_len=payload_len,
        textmessage_setfield=textmessage_setfield,
        textmessage_getfield=textmessage_getfield,
        comment=comment,
    ))
