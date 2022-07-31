// Generated with protogen-builder.py

use dpcore::canvas::images::make_putimage;
use dpcore::paint::{Blendmode, Image, LayerID, Pixel, UserID};
use dpcore::protocol::message::*;
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

#[no_mangle]
pub extern "C" fn write_servercommand(
    writer: &mut MessageWriter,
    ctx: UserID,
    msg: *const u8,
    msg_len: usize,
) {
    ControlMessage::ServerCommand(ctx, unsafe { slice::from_raw_parts(msg, msg_len) }.into())
        .write(writer);
}

#[no_mangle]
pub extern "C" fn write_disconnect(
    writer: &mut MessageWriter,
    ctx: UserID,
    reason: u8,
    message: *const u16,
    message_len: usize,
) {
    ControlMessage::Disconnect(
        ctx,
        DisconnectMessage {
            reason: reason,
            message: String::from_utf16_lossy(unsafe {
                slice::from_raw_parts(message, message_len)
            })
            .to_string(),
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_ping(writer: &mut MessageWriter, ctx: UserID, is_pong: bool) {
    ControlMessage::Ping(ctx, is_pong).write(writer);
}

#[no_mangle]
pub extern "C" fn write_sessionowner(
    writer: &mut MessageWriter,
    ctx: UserID,
    users: *const u8,
    users_len: usize,
) {
    ServerMetaMessage::SessionOwner(
        ctx,
        unsafe { slice::from_raw_parts(users, users_len) }.into(),
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_chat(
    writer: &mut MessageWriter,
    ctx: UserID,
    tflags: u8,
    oflags: u8,
    message: *const u16,
    message_len: usize,
) {
    ServerMetaMessage::Chat(
        ctx,
        ChatMessage {
            tflags: tflags,
            oflags: oflags,
            message: String::from_utf16_lossy(unsafe {
                slice::from_raw_parts(message, message_len)
            })
            .to_string(),
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_trusted(
    writer: &mut MessageWriter,
    ctx: UserID,
    users: *const u8,
    users_len: usize,
) {
    ServerMetaMessage::TrustedUsers(
        ctx,
        unsafe { slice::from_raw_parts(users, users_len) }.into(),
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_privatechat(
    writer: &mut MessageWriter,
    ctx: UserID,
    target: u8,
    oflags: u8,
    message: *const u16,
    message_len: usize,
) {
    ServerMetaMessage::PrivateChat(
        ctx,
        PrivateChatMessage {
            target: target,
            oflags: oflags,
            message: String::from_utf16_lossy(unsafe {
                slice::from_raw_parts(message, message_len)
            })
            .to_string(),
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_lasertrail(
    writer: &mut MessageWriter,
    ctx: UserID,
    color: u32,
    persistence: u8,
) {
    ClientMetaMessage::LaserTrail(
        ctx,
        LaserTrailMessage {
            color: color,
            persistence: persistence,
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_movepointer(writer: &mut MessageWriter, ctx: UserID, x: i32, y: i32) {
    ClientMetaMessage::MovePointer(ctx, MovePointerMessage { x: x, y: y }).write(writer);
}

#[no_mangle]
pub extern "C" fn write_marker(
    writer: &mut MessageWriter,
    ctx: UserID,
    text: *const u16,
    text_len: usize,
) {
    ClientMetaMessage::Marker(
        ctx,
        String::from_utf16_lossy(unsafe { slice::from_raw_parts(text, text_len) }).to_string(),
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_useracl(
    writer: &mut MessageWriter,
    ctx: UserID,
    users: *const u8,
    users_len: usize,
) {
    ClientMetaMessage::UserACL(
        ctx,
        unsafe { slice::from_raw_parts(users, users_len) }.into(),
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_layeracl(
    writer: &mut MessageWriter,
    ctx: UserID,
    id: u16,
    flags: u8,
    exclusive: *const u8,
    exclusive_len: usize,
) {
    ClientMetaMessage::LayerACL(
        ctx,
        LayerACLMessage {
            id: id,
            flags: flags,
            exclusive: unsafe { slice::from_raw_parts(exclusive, exclusive_len) }.into(),
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_featureaccess(
    writer: &mut MessageWriter,
    ctx: UserID,
    feature_tiers: *const u8,
    feature_tiers_len: usize,
) {
    ClientMetaMessage::FeatureAccessLevels(
        ctx,
        unsafe { slice::from_raw_parts(feature_tiers, feature_tiers_len) }.into(),
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_defaultlayer(writer: &mut MessageWriter, ctx: UserID, id: u16) {
    ClientMetaMessage::DefaultLayer(ctx, id).write(writer);
}

#[no_mangle]
pub extern "C" fn write_undopoint(writer: &mut MessageWriter, ctx: UserID) {
    CommandMessage::UndoPoint(ctx).write(writer);
}

#[no_mangle]
pub extern "C" fn write_resize(
    writer: &mut MessageWriter,
    ctx: UserID,
    top: i32,
    right: i32,
    bottom: i32,
    left: i32,
) {
    CommandMessage::CanvasResize(
        ctx,
        CanvasResizeMessage {
            top: top,
            right: right,
            bottom: bottom,
            left: left,
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_newlayer(
    writer: &mut MessageWriter,
    ctx: UserID,
    id: u16,
    source: u16,
    target: u16,
    fill: u32,
    flags: u8,
    name: *const u16,
    name_len: usize,
) {
    CommandMessage::LayerCreate(
        ctx,
        LayerCreateMessage {
            id: id,
            source: source,
            target: target,
            fill: fill,
            flags: flags,
            name: String::from_utf16_lossy(unsafe { slice::from_raw_parts(name, name_len) })
                .to_string(),
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_layerattr(
    writer: &mut MessageWriter,
    ctx: UserID,
    id: u16,
    sublayer: u8,
    flags: u8,
    opacity: u8,
    blend: Blendmode,
) {
    CommandMessage::LayerAttributes(
        ctx,
        LayerAttributesMessage {
            id: id,
            sublayer: sublayer,
            flags: flags,
            opacity: opacity,
            blend: blend as u8,
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_retitlelayer(
    writer: &mut MessageWriter,
    ctx: UserID,
    id: u16,
    title: *const u16,
    title_len: usize,
) {
    CommandMessage::LayerRetitle(
        ctx,
        LayerRetitleMessage {
            id: id,
            title: String::from_utf16_lossy(unsafe { slice::from_raw_parts(title, title_len) })
                .to_string(),
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_deletelayer(
    writer: &mut MessageWriter,
    ctx: UserID,
    id: u16,
    merge_to: u16,
) {
    CommandMessage::LayerDelete(
        ctx,
        LayerDeleteMessage {
            id: id,
            merge_to: merge_to,
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_fillrect(
    writer: &mut MessageWriter,
    ctx: UserID,
    layer: u16,
    mode: Blendmode,
    x: u32,
    y: u32,
    w: u32,
    h: u32,
    color: u32,
) {
    CommandMessage::FillRect(
        ctx,
        FillRectMessage {
            layer: layer,
            mode: mode as u8,
            x: x,
            y: y,
            w: w,
            h: h,
            color: color,
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_penup(writer: &mut MessageWriter, ctx: UserID) {
    CommandMessage::PenUp(ctx).write(writer);
}

#[no_mangle]
pub extern "C" fn write_newannotation(
    writer: &mut MessageWriter,
    ctx: UserID,
    id: u16,
    x: i32,
    y: i32,
    w: u16,
    h: u16,
) {
    CommandMessage::AnnotationCreate(
        ctx,
        AnnotationCreateMessage {
            id: id,
            x: x,
            y: y,
            w: w,
            h: h,
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_reshapeannotation(
    writer: &mut MessageWriter,
    ctx: UserID,
    id: u16,
    x: i32,
    y: i32,
    w: u16,
    h: u16,
) {
    CommandMessage::AnnotationReshape(
        ctx,
        AnnotationReshapeMessage {
            id: id,
            x: x,
            y: y,
            w: w,
            h: h,
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_editannotation(
    writer: &mut MessageWriter,
    ctx: UserID,
    id: u16,
    bg: u32,
    flags: u8,
    border: u8,
    text: *const u16,
    text_len: usize,
) {
    CommandMessage::AnnotationEdit(
        ctx,
        AnnotationEditMessage {
            id: id,
            bg: bg,
            flags: flags,
            border: border,
            text: String::from_utf16_lossy(unsafe { slice::from_raw_parts(text, text_len) })
                .to_string(),
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_deleteannotation(writer: &mut MessageWriter, ctx: UserID, id: u16) {
    CommandMessage::AnnotationDelete(ctx, id).write(writer);
}

#[no_mangle]
pub extern "C" fn write_background(
    writer: &mut MessageWriter,
    ctx: UserID,
    image: *const u8,
    image_len: usize,
) {
    CommandMessage::CanvasBackground(
        ctx,
        unsafe { slice::from_raw_parts(image, image_len) }.into(),
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_moverect(
    writer: &mut MessageWriter,
    ctx: UserID,
    layer: u16,
    sx: i32,
    sy: i32,
    tx: i32,
    ty: i32,
    w: i32,
    h: i32,
    mask: *const u8,
    mask_len: usize,
) {
    CommandMessage::MoveRect(
        ctx,
        MoveRectMessage {
            layer: layer,
            sx: sx,
            sy: sy,
            tx: tx,
            ty: ty,
            w: w,
            h: h,
            mask: unsafe { slice::from_raw_parts(mask, mask_len) }.into(),
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_setmetadataint(
    writer: &mut MessageWriter,
    ctx: UserID,
    field: u8,
    value: i32,
) {
    CommandMessage::SetMetadataInt(
        ctx,
        SetMetadataIntMessage {
            field: field,
            value: value,
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_settimelineframe(
    writer: &mut MessageWriter,
    ctx: UserID,
    frame: u16,
    insert: bool,
    layers: *const u16,
    layers_len: usize,
) {
    CommandMessage::SetTimelineFrame(
        ctx,
        SetTimelineFrameMessage {
            frame: frame,
            insert: insert,
            layers: unsafe { slice::from_raw_parts(layers, layers_len) }.into(),
        },
    )
    .write(writer);
}

#[no_mangle]
pub extern "C" fn write_removetimelineframe(writer: &mut MessageWriter, ctx: UserID, frame: u16) {
    CommandMessage::RemoveTimelineFrame(ctx, frame).write(writer);
}

#[no_mangle]
pub extern "C" fn write_undo(
    writer: &mut MessageWriter,
    ctx: UserID,
    override_user: u8,
    redo: bool,
) {
    CommandMessage::Undo(
        ctx,
        UndoMessage {
            override_user: override_user,
            redo: redo,
        },
    )
    .write(writer);
}

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
        .iter()
        .for_each(|cmd| cmd.write(writer));
}
