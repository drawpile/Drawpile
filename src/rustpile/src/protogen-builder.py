from protogen import load_protocol_definition, MSG_TYPES
from jinja2 import Template


template = Template("""
// Generated with protogen-builder.py

use dpcore::protocol::message::*;
use dpcore::paint::{Image, UserID, LayerID, Blendmode, Pixel};
use dpcore::canvas::images::make_putimage;
use dpcore::protocol::MessageWriter;
use std::slice;

#[no_mangle]
pub extern "C" fn messagewriter_new() -> *mut MessageWriter {
    Box::into_raw(Box::new(MessageWriter::new()))
}

#[no_mangle]
pub extern "C" fn messagewriter_free(mw: *mut MessageWriter) {
    if !mw.is_null() {
        unsafe { Box::from_raw(mw) };
    }
}

#[no_mangle]
pub extern "C" fn messagewriter_content(mw: &MessageWriter, len: &mut usize) -> *const u8 {
    mw.as_ptr(len)
}

{% for msg in messages %}
#[no_mangle]
pub extern "C" fn write_{{ msg.cmd_name }}(
    writer: &mut MessageWriter,
    ctx: UserID,
    {% for f in msg.fields %}{{ field_parameter(f) }}{% endfor %}
) {
    {{ msg.message_type }}Message::{{ msg.name }}(
        ctx,
        {% if msg.fields|length > 1 %}
        {{ msg.name }}Message {
            {% for f in msg.fields %}{{ field_argument(f, True) }},{% endfor %}
        }
        {% else %}
            {% for f in msg.fields %}{{ field_argument(f) }},{% endfor %}
        {% endif %}
    ).write(writer);
}
{% endfor %}

// Custom implementation for PutImage that automatically splits the image
// and takes a byte array as input.
#[no_mangle]
pub extern "C" fn write_putimage(
    writer: &mut MessageWriter,
    user: UserID,
    layer: LayerID,
    x: u32,
    y: u32,
    w: u32,
    h: u32,
    mode: Blendmode,
    pixels: *const u8,
) {
    // TODO ImageRef type so we don't need to make copies
    let mut image = Image::new(w as usize, h as usize);

    image.pixels[..].copy_from_slice(unsafe {
        slice::from_raw_parts_mut(pixels as *mut Pixel, (w * h) as usize)
    });

    make_putimage(user, layer, x, y, &image, mode)
        .iter().for_each(|cmd| cmd.write(writer));
}
""")


def field_parameter(field):
    if field.field_type in ('Bytes', 'Vec<u8>'):
        ff = ((field.name, '*const u8'), (f'{field.name}_len', 'usize'))
    elif field.field_type in ('String', 'Vec<u16>'):
        ff = ((field.name, '*const u16'), (f'{field.name}_len', 'usize'))
    elif field.field_type == 'struct':
        raise NotImplemented
    elif field.field_type == 'argb32':
        ff = ((field.name, 'u32'),)
    elif field.field_type == 'blendmode':
        ff = ((field.name, 'Blendmode'),)
    else:
        ff = ((field.name, field.field_type),)

    return ''.join('{}: {},'.format(*f) for f in ff)


def field_argument(field, named=False):
    if field.field_type in ('Bytes', 'Vec<u8>', 'Vec<u16>'):
        ff = f'unsafe {{ slice::from_raw_parts({field.name}, {field.name}_len) }}.into()'
    elif field.field_type == 'String':  # TODO use Cow?
        # We use UTF16 here because that's how QStrings are encoded internally
        ff = f'String::from_utf16_lossy(unsafe {{ slice::from_raw_parts({field.name}, {field.name}_len) }}).to_string()'
    elif field.field_type == 'struct':
        raise NotImplementedError
    elif field.field_type == 'blendmode':
        ff = f'{field.name} as u8'
    else:
        ff = f'{field.name}'

    if named:
        return f'{field.name}: {ff}'
    return ff


if __name__ == '__main__':
    protocol = load_protocol_definition('../../dpcore/src/protocol/protocol.yaml')

    # Produce builder functions for these messages.
    # The other messages are generated solely from Rust code
    messages = set((
        'ServerCommand',
        'Ping',
        'Disconnect',
        'SessionOwner',
        'Chat',
        'TrustedUsers',
        'PrivateChat',
        'LaserTrail',
        'MovePointer',
        'Marker',
        'UserACL',
        'LayerACL',
        'FeatureAccessLevels',
        'DefaultLayer',
        'UndoPoint',
        'CanvasResize',
        'LayerCreate',
        'LayerAttributes',
        'LayerRetitle',
        #'LayerOrder', # used via make_movelayer
        'LayerDelete',
        # 'PutImage', custom implementation
        'FillRect',
        'PenUp',
        'AnnotationCreate',
        'AnnotationReshape',
        'AnnotationEdit',
        'AnnotationDelete',
        #'MoveRegion',
        #'PutTile',
        'CanvasBackground',
        'MoveRect',
        'SetMetadataInt',
        'SetTimelineFrame',
        'Undo',
    ))

    print(template.render(
        messages=[m for m in protocol['messages'] if m.name in messages],
        field_parameter=field_parameter,
        field_argument=field_argument,
    ))
