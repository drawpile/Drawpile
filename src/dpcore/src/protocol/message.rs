// Message definitions generated with protogen-rust.py

// allow some stylistic errors that make code generations simpler
#![allow(clippy::needless_borrow)]

use super::serialization::{DeserializationError, MessageReader, MessageWriter};
use super::textmessage::TextMessage;
use num_enum::IntoPrimitive;
use num_enum::TryFromPrimitive;
use std::fmt;
use std::str::FromStr;

pub static PROTOCOL_VERSION: &str = "dp:4.22.2";
pub const UNDO_DEPTH: u32 = 30;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DisconnectMessage {
    pub reason: u8,
    pub message: String,
}

impl DisconnectMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(1, 65535)?;

        let reason = reader.read::<u8>();
        let message = reader.read_remaining_str();

        Ok(Self { reason, message })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(1, user_id, 1 + self.message.len());
        w.write(self.reason);
        w.write(&self.message);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("reason", self.reason.to_string())
            .set("message", self.message.clone())
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            reason: tm.get_u8("reason"),
            message: tm.get_str("message").to_string(),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct JoinMessage {
    pub flags: u8,
    pub name: String,
    pub avatar: Vec<u8>,
}

impl JoinMessage {
    pub const FLAGS_AUTH: u8 = 0x1;
    pub const FLAGS_MOD: u8 = 0x2;
    pub const FLAGS_BOT: u8 = 0x4;
    pub const FLAGS: &'static [&'static str] = &["auth", "mod", "bot"];

    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(2, 65535)?;

        let flags = reader.read::<u8>();
        let name_len = reader.read::<u8>() as usize;
        if reader.remaining() < name_len {
            return Err(DeserializationError::InvalidField(
                "Join::name field is too long",
            ));
        }
        let name = reader.read_str(name_len);
        let avatar = reader.read_remaining_vec::<u8>();

        Ok(Self {
            flags,
            name,
            avatar,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(32, user_id, 2 + self.name.len() + self.avatar.len());
        w.write(self.flags);
        w.write(self.name.len() as u8);
        w.write(&self.name);
        w.write(&self.avatar);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set_flags("flags", &Self::FLAGS, self.flags)
            .set("name", self.name.clone())
            .set_bytes("avatar", &self.avatar)
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            flags: tm.get_flags(&Self::FLAGS, "flags"),
            name: tm.get_str("name").to_string(),
            avatar: tm.get_bytes("avatar"),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ChatMessage {
    pub tflags: u8,
    pub oflags: u8,
    pub message: String,
}

impl ChatMessage {
    pub const TFLAGS_BYPASS: u8 = 0x1;
    pub const TFLAGS: &'static [&'static str] = &["bypass"];
    pub const OFLAGS_SHOUT: u8 = 0x1;
    pub const OFLAGS_ACTION: u8 = 0x2;
    pub const OFLAGS_PIN: u8 = 0x4;
    pub const OFLAGS_ALERT: u8 = 0x8;
    pub const OFLAGS: &'static [&'static str] = &["shout", "action", "pin", "alert"];

    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(2, 65535)?;

        let tflags = reader.read::<u8>();
        let oflags = reader.read::<u8>();
        let message = reader.read_remaining_str();

        Ok(Self {
            tflags,
            oflags,
            message,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(35, user_id, 2 + self.message.len());
        w.write(self.tflags);
        w.write(self.oflags);
        w.write(&self.message);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set_flags("tflags", &Self::TFLAGS, self.tflags)
            .set_flags("oflags", &Self::OFLAGS, self.oflags)
            .set("message", self.message.clone())
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            tflags: tm.get_flags(&Self::TFLAGS, "tflags"),
            oflags: tm.get_flags(&Self::OFLAGS, "oflags"),
            message: tm.get_str("message").to_string(),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PrivateChatMessage {
    pub target: u8,
    pub oflags: u8,
    pub message: String,
}

impl PrivateChatMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(2, 65535)?;

        let target = reader.read::<u8>();
        let oflags = reader.read::<u8>();
        let message = reader.read_remaining_str();

        Ok(Self {
            target,
            oflags,
            message,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(38, user_id, 2 + self.message.len());
        w.write(self.target);
        w.write(self.oflags);
        w.write(&self.message);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("target", self.target.to_string())
            .set("oflags", self.oflags.to_string())
            .set("message", self.message.clone())
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            target: tm.get_u8("target"),
            oflags: tm.get_u8("oflags"),
            message: tm.get_str("message").to_string(),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct LaserTrailMessage {
    pub color: u32,
    pub persistence: u8,
}

impl LaserTrailMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(5, 5)?;

        let color = reader.read::<u32>();
        let persistence = reader.read::<u8>();

        Ok(Self { color, persistence })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(65, user_id, 5);
        w.write(self.color);
        w.write(self.persistence);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set_argb32("color", self.color)
            .set("persistence", self.persistence.to_string())
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            color: tm.get_argb32("color"),
            persistence: tm.get_u8("persistence"),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MovePointerMessage {
    pub x: i32,
    pub y: i32,
}

impl MovePointerMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(8, 8)?;

        let x = reader.read::<i32>();
        let y = reader.read::<i32>();

        Ok(Self { x, y })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(66, user_id, 8);
        w.write(self.x);
        w.write(self.y);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("x", self.x.to_string())
            .set("y", self.y.to_string())
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            x: i32::from_str(tm.get_str("x")).unwrap_or_default(),
            y: i32::from_str(tm.get_str("y")).unwrap_or_default(),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct LayerACLMessage {
    pub id: u16,
    pub flags: u8,
    pub exclusive: Vec<u8>,
}

impl LayerACLMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(3, 258)?;

        let id = reader.read::<u16>();
        let flags = reader.read::<u8>();
        let exclusive = reader.read_remaining_vec();

        Ok(Self {
            id,
            flags,
            exclusive,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(69, user_id, 3 + self.exclusive.len());
        w.write(self.id);
        w.write(self.flags);
        w.write(&self.exclusive);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("id", self.id.to_string())
            .set("flags", self.flags.to_string())
            .set_vec_u8("exclusive", &self.exclusive)
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            id: tm.get_u16("id"),
            flags: tm.get_u8("flags"),
            exclusive: tm.get_vec_u8("exclusive"),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CanvasResizeMessage {
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
    pub left: i32,
}

impl CanvasResizeMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(16, 16)?;

        let top = reader.read::<i32>();
        let right = reader.read::<i32>();
        let bottom = reader.read::<i32>();
        let left = reader.read::<i32>();

        Ok(Self {
            top,
            right,
            bottom,
            left,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(129, user_id, 16);
        w.write(self.top);
        w.write(self.right);
        w.write(self.bottom);
        w.write(self.left);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("top", self.top.to_string())
            .set("right", self.right.to_string())
            .set("bottom", self.bottom.to_string())
            .set("left", self.left.to_string())
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            top: i32::from_str(tm.get_str("top")).unwrap_or_default(),
            right: i32::from_str(tm.get_str("right")).unwrap_or_default(),
            bottom: i32::from_str(tm.get_str("bottom")).unwrap_or_default(),
            left: i32::from_str(tm.get_str("left")).unwrap_or_default(),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct LayerCreateMessage {
    pub id: u16,
    pub source: u16,
    pub target: u16,
    pub fill: u32,
    pub flags: u8,
    pub name: String,
}

impl LayerCreateMessage {
    pub const FLAGS_GROUP: u8 = 0x1;
    pub const FLAGS_INTO: u8 = 0x2;
    pub const FLAGS: &'static [&'static str] = &["group", "into"];

    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(11, 65535)?;

        let id = reader.read::<u16>();
        let source = reader.read::<u16>();
        let target = reader.read::<u16>();
        let fill = reader.read::<u32>();
        let flags = reader.read::<u8>();
        let name = reader.read_remaining_str();

        Ok(Self {
            id,
            source,
            target,
            fill,
            flags,
            name,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(130, user_id, 11 + self.name.len());
        w.write(self.id);
        w.write(self.source);
        w.write(self.target);
        w.write(self.fill);
        w.write(self.flags);
        w.write(&self.name);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("id", format!("0x{:04x}", self.id))
            .set("source", format!("0x{:04x}", self.source))
            .set("target", format!("0x{:04x}", self.target))
            .set_argb32("fill", self.fill)
            .set_flags("flags", &Self::FLAGS, self.flags)
            .set("name", self.name.clone())
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            id: tm.get_u16("id"),
            source: tm.get_u16("source"),
            target: tm.get_u16("target"),
            fill: tm.get_argb32("fill"),
            flags: tm.get_flags(&Self::FLAGS, "flags"),
            name: tm.get_str("name").to_string(),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct LayerAttributesMessage {
    pub id: u16,
    pub sublayer: u8,
    pub flags: u8,
    pub opacity: u8,
    pub blend: u8,
}

impl LayerAttributesMessage {
    pub const FLAGS_CENSOR: u8 = 0x1;
    pub const FLAGS_FIXED: u8 = 0x2;
    pub const FLAGS_ISOLATED: u8 = 0x4;
    pub const FLAGS: &'static [&'static str] = &["censor", "fixed", "isolated"];

    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(6, 6)?;

        let id = reader.read::<u16>();
        let sublayer = reader.read::<u8>();
        let flags = reader.read::<u8>();
        let opacity = reader.read::<u8>();
        let blend = reader.read::<u8>();

        Ok(Self {
            id,
            sublayer,
            flags,
            opacity,
            blend,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(131, user_id, 6);
        w.write(self.id);
        w.write(self.sublayer);
        w.write(self.flags);
        w.write(self.opacity);
        w.write(self.blend);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("id", format!("0x{:04x}", self.id))
            .set("sublayer", self.sublayer.to_string())
            .set_flags("flags", &Self::FLAGS, self.flags)
            .set("opacity", self.opacity.to_string())
            .set_blendmode("blend", self.blend)
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            id: tm.get_u16("id"),
            sublayer: tm.get_u8("sublayer"),
            flags: tm.get_flags(&Self::FLAGS, "flags"),
            opacity: tm.get_u8("opacity"),
            blend: tm.get_blendmode("blend"),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct LayerRetitleMessage {
    pub id: u16,
    pub title: String,
}

impl LayerRetitleMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(2, 65535)?;

        let id = reader.read::<u16>();
        let title = reader.read_remaining_str();

        Ok(Self { id, title })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(132, user_id, 2 + self.title.len());
        w.write(self.id);
        w.write(&self.title);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("id", format!("0x{:04x}", self.id))
            .set("title", self.title.clone())
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            id: tm.get_u16("id"),
            title: tm.get_str("title").to_string(),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct LayerOrderMessage {
    pub root: u16,
    pub layers: Vec<u16>,
}

impl LayerOrderMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(2, 65535)?;

        let root = reader.read::<u16>();
        let layers = reader.read_remaining_vec();

        Ok(Self { root, layers })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(133, user_id, 2 + (self.layers.len() * 2));
        w.write(self.root);
        w.write(&self.layers);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("root", format!("0x{:04x}", self.root))
            .set_vec_u16("layers", &self.layers, true)
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            root: tm.get_u16("root"),
            layers: tm.get_vec_u16("layers"),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct LayerDeleteMessage {
    pub id: u16,
    pub merge_to: u16,
}

impl LayerDeleteMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(4, 4)?;

        let id = reader.read::<u16>();
        let merge_to = reader.read::<u16>();

        Ok(Self { id, merge_to })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(134, user_id, 4);
        w.write(self.id);
        w.write(self.merge_to);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("id", format!("0x{:04x}", self.id))
            .set("merge_to", format!("0x{:04x}", self.merge_to))
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            id: tm.get_u16("id"),
            merge_to: tm.get_u16("merge_to"),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct LayerVisibilityMessage {
    pub id: u16,
    pub visible: bool,
}

impl LayerVisibilityMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(3, 3)?;

        let id = reader.read::<u16>();
        let visible = reader.read::<bool>();

        Ok(Self { id, visible })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(135, user_id, 3);
        w.write(self.id);
        w.write(self.visible);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("id", self.id.to_string())
            .set("visible", self.visible.to_string())
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            id: tm.get_u16("id"),
            visible: tm.get_str("visible") == "true",
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PutImageMessage {
    pub layer: u16,
    pub mode: u8,
    pub x: u32,
    pub y: u32,
    pub w: u32,
    pub h: u32,
    pub image: Vec<u8>,
}

impl PutImageMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(19, 65535)?;

        let layer = reader.read::<u16>();
        let mode = reader.read::<u8>();
        let x = reader.read::<u32>();
        let y = reader.read::<u32>();
        let w = reader.read::<u32>();
        let h = reader.read::<u32>();
        let image = reader.read_remaining_vec::<u8>();

        Ok(Self {
            layer,
            mode,
            x,
            y,
            w,
            h,
            image,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(136, user_id, 19 + self.image.len());
        w.write(self.layer);
        w.write(self.mode);
        w.write(self.x);
        w.write(self.y);
        w.write(self.w);
        w.write(self.h);
        w.write(&self.image);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("layer", format!("0x{:04x}", self.layer))
            .set_blendmode("mode", self.mode)
            .set("x", self.x.to_string())
            .set("y", self.y.to_string())
            .set("w", self.w.to_string())
            .set("h", self.h.to_string())
            .set_bytes("image", &self.image)
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            layer: tm.get_u16("layer"),
            mode: tm.get_blendmode("mode"),
            x: tm.get_u32("x"),
            y: tm.get_u32("y"),
            w: tm.get_u32("w"),
            h: tm.get_u32("h"),
            image: tm.get_bytes("image"),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct FillRectMessage {
    pub layer: u16,
    pub mode: u8,
    pub x: u32,
    pub y: u32,
    pub w: u32,
    pub h: u32,
    pub color: u32,
}

impl FillRectMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(23, 23)?;

        let layer = reader.read::<u16>();
        let mode = reader.read::<u8>();
        let x = reader.read::<u32>();
        let y = reader.read::<u32>();
        let w = reader.read::<u32>();
        let h = reader.read::<u32>();
        let color = reader.read::<u32>();

        Ok(Self {
            layer,
            mode,
            x,
            y,
            w,
            h,
            color,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(137, user_id, 23);
        w.write(self.layer);
        w.write(self.mode);
        w.write(self.x);
        w.write(self.y);
        w.write(self.w);
        w.write(self.h);
        w.write(self.color);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("layer", format!("0x{:04x}", self.layer))
            .set_blendmode("mode", self.mode)
            .set("x", self.x.to_string())
            .set("y", self.y.to_string())
            .set("w", self.w.to_string())
            .set("h", self.h.to_string())
            .set_argb32("color", self.color)
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            layer: tm.get_u16("layer"),
            mode: tm.get_blendmode("mode"),
            x: tm.get_u32("x"),
            y: tm.get_u32("y"),
            w: tm.get_u32("w"),
            h: tm.get_u32("h"),
            color: tm.get_argb32("color"),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct AnnotationCreateMessage {
    pub id: u16,
    pub x: i32,
    pub y: i32,
    pub w: u16,
    pub h: u16,
}

impl AnnotationCreateMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(14, 14)?;

        let id = reader.read::<u16>();
        let x = reader.read::<i32>();
        let y = reader.read::<i32>();
        let w = reader.read::<u16>();
        let h = reader.read::<u16>();

        Ok(Self { id, x, y, w, h })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(141, user_id, 14);
        w.write(self.id);
        w.write(self.x);
        w.write(self.y);
        w.write(self.w);
        w.write(self.h);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("id", format!("0x{:04x}", self.id))
            .set("x", self.x.to_string())
            .set("y", self.y.to_string())
            .set("w", self.w.to_string())
            .set("h", self.h.to_string())
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            id: tm.get_u16("id"),
            x: i32::from_str(tm.get_str("x")).unwrap_or_default(),
            y: i32::from_str(tm.get_str("y")).unwrap_or_default(),
            w: tm.get_u16("w"),
            h: tm.get_u16("h"),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct AnnotationReshapeMessage {
    pub id: u16,
    pub x: i32,
    pub y: i32,
    pub w: u16,
    pub h: u16,
}

impl AnnotationReshapeMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(14, 14)?;

        let id = reader.read::<u16>();
        let x = reader.read::<i32>();
        let y = reader.read::<i32>();
        let w = reader.read::<u16>();
        let h = reader.read::<u16>();

        Ok(Self { id, x, y, w, h })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(142, user_id, 14);
        w.write(self.id);
        w.write(self.x);
        w.write(self.y);
        w.write(self.w);
        w.write(self.h);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("id", format!("0x{:04x}", self.id))
            .set("x", self.x.to_string())
            .set("y", self.y.to_string())
            .set("w", self.w.to_string())
            .set("h", self.h.to_string())
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            id: tm.get_u16("id"),
            x: i32::from_str(tm.get_str("x")).unwrap_or_default(),
            y: i32::from_str(tm.get_str("y")).unwrap_or_default(),
            w: tm.get_u16("w"),
            h: tm.get_u16("h"),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct AnnotationEditMessage {
    pub id: u16,
    pub bg: u32,
    pub flags: u8,
    pub border: u8,
    pub text: String,
}

impl AnnotationEditMessage {
    pub const FLAGS_PROTECT: u8 = 0x1;
    pub const FLAGS_VALIGN_CENTER: u8 = 0x2;
    pub const FLAGS_VALIGN_BOTTOM: u8 = 0x4;
    pub const FLAGS: &'static [&'static str] = &["protect", "valign_center", "valign_bottom"];

    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(8, 65535)?;

        let id = reader.read::<u16>();
        let bg = reader.read::<u32>();
        let flags = reader.read::<u8>();
        let border = reader.read::<u8>();
        let text = reader.read_remaining_str();

        Ok(Self {
            id,
            bg,
            flags,
            border,
            text,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(143, user_id, 8 + self.text.len());
        w.write(self.id);
        w.write(self.bg);
        w.write(self.flags);
        w.write(self.border);
        w.write(&self.text);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("id", format!("0x{:04x}", self.id))
            .set_argb32("bg", self.bg)
            .set_flags("flags", &Self::FLAGS, self.flags)
            .set("border", self.border.to_string())
            .set("text", self.text.clone())
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            id: tm.get_u16("id"),
            bg: tm.get_argb32("bg"),
            flags: tm.get_flags(&Self::FLAGS, "flags"),
            border: tm.get_u8("border"),
            text: tm.get_str("text").to_string(),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PutTileMessage {
    pub layer: u16,
    pub sublayer: u8,
    pub last_touch: u8,
    pub col: u16,
    pub row: u16,
    pub repeat: u16,
    pub image: Vec<u8>,
}

impl PutTileMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(10, 65535)?;

        let layer = reader.read::<u16>();
        let sublayer = reader.read::<u8>();
        let last_touch = reader.read::<u8>();
        let col = reader.read::<u16>();
        let row = reader.read::<u16>();
        let repeat = reader.read::<u16>();
        let image = reader.read_remaining_vec::<u8>();

        Ok(Self {
            layer,
            sublayer,
            last_touch,
            col,
            row,
            repeat,
            image,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(146, user_id, 10 + self.image.len());
        w.write(self.layer);
        w.write(self.sublayer);
        w.write(self.last_touch);
        w.write(self.col);
        w.write(self.row);
        w.write(self.repeat);
        w.write(&self.image);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("layer", format!("0x{:04x}", self.layer))
            .set("sublayer", self.sublayer.to_string())
            .set("last_touch", self.last_touch.to_string())
            .set("col", self.col.to_string())
            .set("row", self.row.to_string())
            .set("repeat", self.repeat.to_string())
            .set_bytes("image", &self.image)
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            layer: tm.get_u16("layer"),
            sublayer: tm.get_u8("sublayer"),
            last_touch: tm.get_u8("last_touch"),
            col: tm.get_u16("col"),
            row: tm.get_u16("row"),
            repeat: tm.get_u16("repeat"),
            image: tm.get_bytes("image"),
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ClassicDab {
    pub x: i8,
    pub y: i8,
    pub size: u16,
    pub hardness: u8,
    pub opacity: u8,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DrawDabsClassicMessage {
    pub layer: u16,
    pub x: i32,
    pub y: i32,
    pub color: u32,
    pub mode: u8,
    pub dabs: Vec<ClassicDab>,
}

impl DrawDabsClassicMessage {
    pub const MAX_CLASSICDABS: usize = 10920;

    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(21, 65535)?;

        let layer = reader.read::<u16>();
        let x = reader.read::<i32>();
        let y = reader.read::<i32>();
        let color = reader.read::<u32>();
        let mode = reader.read::<u8>();
        let mut dabs = Vec::<ClassicDab>::with_capacity(reader.remaining() / 6);
        while reader.remaining() > 0 {
            let x = reader.read::<i8>();
            let y = reader.read::<i8>();
            let size = reader.read::<u16>();
            let hardness = reader.read::<u8>();
            let opacity = reader.read::<u8>();
            dabs.push(ClassicDab {
                x,
                y,
                size,
                hardness,
                opacity,
            });
        }
        Ok(Self {
            layer,
            x,
            y,
            color,
            mode,
            dabs,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(148, user_id, 15 + (self.dabs.len() * 6));
        w.write(self.layer);
        w.write(self.x);
        w.write(self.y);
        w.write(self.color);
        w.write(self.mode);
        for item in self.dabs.iter() {
            w.write(item.x);
            w.write(item.y);
            w.write(item.size);
            w.write(item.hardness);
            w.write(item.opacity);
        }
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        let mut dabs: Vec<Vec<f64>> = Vec::with_capacity(self.dabs.len());
        for dab in self.dabs.iter() {
            dabs.push(vec![
                dab.x as f64 / 4.0,
                dab.y as f64 / 4.0,
                dab.size as f64 / 256.0,
                dab.hardness as f64,
                dab.opacity as f64,
            ]);
        }
        txt.set("layer", format!("0x{:04x}", self.layer))
            .set("x", (self.x as f64 / 4.0).to_string())
            .set("y", (self.y as f64 / 4.0).to_string())
            .set_argb32("color", self.color)
            .set_blendmode("mode", self.mode)
            .set_dabs(dabs)
    }

    fn from_text(tm: &TextMessage) -> Self {
        let mut dab_structs: Vec<ClassicDab> = Vec::with_capacity(tm.dabs.len());
        for dab in tm.dabs.iter() {
            if dab.len() != 5 {
                continue;
            }
            dab_structs.push(ClassicDab {
                x: (dab[0] * 4.0) as i8,
                y: (dab[1] * 4.0) as i8,
                size: (dab[2] * 256.0) as u16,
                hardness: (dab[3]) as u8,
                opacity: (dab[4]) as u8,
            });
        }

        Self {
            layer: tm.get_u16("layer"),
            x: (tm.get_f64("x") * 4.0) as i32,
            y: (tm.get_f64("y") * 4.0) as i32,
            color: tm.get_argb32("color"),
            mode: tm.get_blendmode("mode"),
            dabs: dab_structs,
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct PixelDab {
    pub x: i8,
    pub y: i8,
    pub size: u8,
    pub opacity: u8,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DrawDabsPixelMessage {
    pub layer: u16,
    pub x: i32,
    pub y: i32,
    pub color: u32,
    pub mode: u8,
    pub dabs: Vec<PixelDab>,
}

impl DrawDabsPixelMessage {
    pub const MAX_PIXELDABS: usize = 16380;

    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(19, 65535)?;

        let layer = reader.read::<u16>();
        let x = reader.read::<i32>();
        let y = reader.read::<i32>();
        let color = reader.read::<u32>();
        let mode = reader.read::<u8>();
        let mut dabs = Vec::<PixelDab>::with_capacity(reader.remaining() / 4);
        while reader.remaining() > 0 {
            let x = reader.read::<i8>();
            let y = reader.read::<i8>();
            let size = reader.read::<u8>();
            let opacity = reader.read::<u8>();
            dabs.push(PixelDab {
                x,
                y,
                size,
                opacity,
            });
        }
        Ok(Self {
            layer,
            x,
            y,
            color,
            mode,
            dabs,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, msg_id: u8, user_id: u8) {
        w.write_header(msg_id, user_id, 15 + (self.dabs.len() * 4));
        w.write(self.layer);
        w.write(self.x);
        w.write(self.y);
        w.write(self.color);
        w.write(self.mode);
        for item in self.dabs.iter() {
            w.write(item.x);
            w.write(item.y);
            w.write(item.size);
            w.write(item.opacity);
        }
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        let mut dabs: Vec<Vec<f64>> = Vec::with_capacity(self.dabs.len());
        for dab in self.dabs.iter() {
            dabs.push(vec![
                dab.x as f64,
                dab.y as f64,
                dab.size as f64,
                dab.opacity as f64,
            ]);
        }
        txt.set("layer", format!("0x{:04x}", self.layer))
            .set("x", self.x.to_string())
            .set("y", self.y.to_string())
            .set_argb32("color", self.color)
            .set_blendmode("mode", self.mode)
            .set_dabs(dabs)
    }

    fn from_text(tm: &TextMessage) -> Self {
        let mut dab_structs: Vec<PixelDab> = Vec::with_capacity(tm.dabs.len());
        for dab in tm.dabs.iter() {
            if dab.len() != 4 {
                continue;
            }
            dab_structs.push(PixelDab {
                x: (dab[0]) as i8,
                y: (dab[1]) as i8,
                size: (dab[2]) as u8,
                opacity: (dab[3]) as u8,
            });
        }

        Self {
            layer: tm.get_u16("layer"),
            x: i32::from_str(tm.get_str("x")).unwrap_or_default(),
            y: i32::from_str(tm.get_str("y")).unwrap_or_default(),
            color: tm.get_argb32("color"),
            mode: tm.get_blendmode("mode"),
            dabs: dab_structs,
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct MyPaintDab {
    pub x: i8,
    pub y: i8,
    pub size: u16,
    pub hardness: u8,
    pub opacity: u8,
    pub angle: u8,
    pub aspect_ratio: u8,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DrawDabsMyPaintMessage {
    pub layer: u16,
    pub x: i32,
    pub y: i32,
    pub color: u32,
    pub lock_alpha: u8,
    pub dabs: Vec<MyPaintDab>,
}

impl DrawDabsMyPaintMessage {
    pub const MAX_MYPAINTDABS: usize = 8190;

    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(23, 65535)?;

        let layer = reader.read::<u16>();
        let x = reader.read::<i32>();
        let y = reader.read::<i32>();
        let color = reader.read::<u32>();
        let lock_alpha = reader.read::<u8>();
        let mut dabs = Vec::<MyPaintDab>::with_capacity(reader.remaining() / 8);
        while reader.remaining() > 0 {
            let x = reader.read::<i8>();
            let y = reader.read::<i8>();
            let size = reader.read::<u16>();
            let hardness = reader.read::<u8>();
            let opacity = reader.read::<u8>();
            let angle = reader.read::<u8>();
            let aspect_ratio = reader.read::<u8>();
            dabs.push(MyPaintDab {
                x,
                y,
                size,
                hardness,
                opacity,
                angle,
                aspect_ratio,
            });
        }
        Ok(Self {
            layer,
            x,
            y,
            color,
            lock_alpha,
            dabs,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(151, user_id, 15 + (self.dabs.len() * 8));
        w.write(self.layer);
        w.write(self.x);
        w.write(self.y);
        w.write(self.color);
        w.write(self.lock_alpha);
        for item in self.dabs.iter() {
            w.write(item.x);
            w.write(item.y);
            w.write(item.size);
            w.write(item.hardness);
            w.write(item.opacity);
            w.write(item.angle);
            w.write(item.aspect_ratio);
        }
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        let mut dabs: Vec<Vec<f64>> = Vec::with_capacity(self.dabs.len());
        for dab in self.dabs.iter() {
            dabs.push(vec![
                dab.x as f64 / 4.0,
                dab.y as f64 / 4.0,
                dab.size as f64 / 256.0,
                dab.hardness as f64,
                dab.opacity as f64,
                dab.angle as f64,
                dab.aspect_ratio as f64,
            ]);
        }
        txt.set("layer", format!("0x{:04x}", self.layer))
            .set("x", (self.x as f64 / 4.0).to_string())
            .set("y", (self.y as f64 / 4.0).to_string())
            .set_argb32("color", self.color)
            .set("lock_alpha", self.lock_alpha.to_string())
            .set_dabs(dabs)
    }

    fn from_text(tm: &TextMessage) -> Self {
        let mut dab_structs: Vec<MyPaintDab> = Vec::with_capacity(tm.dabs.len());
        for dab in tm.dabs.iter() {
            if dab.len() != 7 {
                continue;
            }
            dab_structs.push(MyPaintDab {
                x: (dab[0] * 4.0) as i8,
                y: (dab[1] * 4.0) as i8,
                size: (dab[2] * 256.0) as u16,
                hardness: (dab[3]) as u8,
                opacity: (dab[4]) as u8,
                angle: (dab[5]) as u8,
                aspect_ratio: (dab[6]) as u8,
            });
        }

        Self {
            layer: tm.get_u16("layer"),
            x: (tm.get_f64("x") * 4.0) as i32,
            y: (tm.get_f64("y") * 4.0) as i32,
            color: tm.get_argb32("color"),
            lock_alpha: tm.get_u8("lock_alpha"),
            dabs: dab_structs,
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MoveRectMessage {
    pub layer: u16,
    pub sx: i32,
    pub sy: i32,
    pub tx: i32,
    pub ty: i32,
    pub w: i32,
    pub h: i32,
    pub mask: Vec<u8>,
}

impl MoveRectMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(26, 65535)?;

        let layer = reader.read::<u16>();
        let sx = reader.read::<i32>();
        let sy = reader.read::<i32>();
        let tx = reader.read::<i32>();
        let ty = reader.read::<i32>();
        let w = reader.read::<i32>();
        let h = reader.read::<i32>();
        let mask = reader.read_remaining_vec::<u8>();

        Ok(Self {
            layer,
            sx,
            sy,
            tx,
            ty,
            w,
            h,
            mask,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(160, user_id, 26 + self.mask.len());
        w.write(self.layer);
        w.write(self.sx);
        w.write(self.sy);
        w.write(self.tx);
        w.write(self.ty);
        w.write(self.w);
        w.write(self.h);
        w.write(&self.mask);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("layer", format!("0x{:04x}", self.layer))
            .set("sx", self.sx.to_string())
            .set("sy", self.sy.to_string())
            .set("tx", self.tx.to_string())
            .set("ty", self.ty.to_string())
            .set("w", self.w.to_string())
            .set("h", self.h.to_string())
            .set_bytes("mask", &self.mask)
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            layer: tm.get_u16("layer"),
            sx: i32::from_str(tm.get_str("sx")).unwrap_or_default(),
            sy: i32::from_str(tm.get_str("sy")).unwrap_or_default(),
            tx: i32::from_str(tm.get_str("tx")).unwrap_or_default(),
            ty: i32::from_str(tm.get_str("ty")).unwrap_or_default(),
            w: i32::from_str(tm.get_str("w")).unwrap_or_default(),
            h: i32::from_str(tm.get_str("h")).unwrap_or_default(),
            mask: tm.get_bytes("mask"),
        }
    }
}

#[derive(Copy, Clone, Debug, Eq, PartialEq, IntoPrimitive, TryFromPrimitive)]
#[repr(u8)]
pub enum MetadataInt {
    Dpix,
    Dpiy,
    Framerate,
    UseTimeline,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SetMetadataIntMessage {
    pub field: u8,
    pub value: i32,
}

impl SetMetadataIntMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(5, 5)?;

        let field = reader.read::<u8>();
        let value = reader.read::<i32>();

        Ok(Self { field, value })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(161, user_id, 5);
        w.write(self.field);
        w.write(self.value);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("field", self.field.to_string())
            .set("value", self.value.to_string())
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            field: tm.get_u8("field"),
            value: i32::from_str(tm.get_str("value")).unwrap_or_default(),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SetMetadataStrMessage {
    pub field: u8,
    pub value: String,
}

impl SetMetadataStrMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(1, 65535)?;

        let field = reader.read::<u8>();
        let value = reader.read_remaining_str();

        Ok(Self { field, value })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(162, user_id, 1 + self.value.len());
        w.write(self.field);
        w.write(&self.value);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("field", self.field.to_string())
            .set("value", self.value.clone())
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            field: tm.get_u8("field"),
            value: tm.get_str("value").to_string(),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SetTimelineFrameMessage {
    pub frame: u16,
    pub insert: bool,
    pub layers: Vec<u16>,
}

impl SetTimelineFrameMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(3, 27)?;

        let frame = reader.read::<u16>();
        let insert = reader.read::<bool>();
        let layers = reader.read_remaining_vec();

        Ok(Self {
            frame,
            insert,
            layers,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(163, user_id, 3 + (self.layers.len() * 2));
        w.write(self.frame);
        w.write(self.insert);
        w.write(&self.layers);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("frame", self.frame.to_string())
            .set("insert", self.insert.to_string())
            .set_vec_u16("layers", &self.layers, true)
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            frame: tm.get_u16("frame"),
            insert: tm.get_str("insert") == "true",
            layers: tm.get_vec_u16("layers"),
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct UndoMessage {
    pub override_user: u8,
    pub redo: bool,
}

impl UndoMessage {
    fn deserialize(reader: &mut MessageReader) -> Result<Self, DeserializationError> {
        reader.validate(2, 2)?;

        let override_user = reader.read::<u8>();
        let redo = reader.read::<bool>();

        Ok(Self {
            override_user,
            redo,
        })
    }

    fn serialize(&self, w: &mut MessageWriter, user_id: u8) {
        w.write_header(255, user_id, 2);
        w.write(self.override_user);
        w.write(self.redo);
    }

    fn to_text(&self, txt: TextMessage) -> TextMessage {
        txt.set("override_user", self.override_user.to_string())
            .set("redo", self.redo.to_string())
    }

    fn from_text(tm: &TextMessage) -> Self {
        Self {
            override_user: tm.get_u8("override_user"),
            redo: tm.get_str("redo") == "true",
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ControlMessage {
    /// Server command message
    ///
    /// This is a general purpose message for sending commands to the server
    /// and receiving replies. This is used for (among other things):
    ///
    /// - the login handshake
    /// - setting session parameters (e.g. max user count and password)
    /// - sending administration commands (e.g. kick user)
    ///
    ServerCommand(u8, Vec<u8>),

    /// Disconnect notification
    ///
    /// This message is used when closing the connection gracefully. The message queue
    /// will automatically close the socket after sending this message.
    ///
    Disconnect(u8, DisconnectMessage),

    /// Ping message
    ///
    /// This is used for latency measurement as well as a keepalive. Normally, the client
    /// should be the one to send the ping messages.
    ///
    /// The server should return a Ping with the is_pong flag set
    ///
    Ping(u8, bool),
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ServerMetaMessage {
    /// Inform the client of a new user
    ///
    /// This message is sent only be the server. It associates a username
    /// with a context ID.
    ///
    Join(u8, JoinMessage),

    /// Inform the client of a user leaving
    ///
    /// This message is sent only by the server. Upon receiving this message,
    /// clients will typically remove the user from the user listing. The client
    /// is also allowed to release resources associated with this context ID.
    ///
    Leave(u8),

    /// Session ownership change
    ///
    /// This message sets the users who have operator status. It can be
    /// sent by users who are already operators or by the server (user id=0).
    ///
    /// The list of operators implicitly contains the user who sends the
    /// message, thus users cannot deop themselves.
    ///
    /// The server sanitizes the ID list so, when distributed to other users,
    /// it does not contain any duplicates or non-existing users and can be trusted
    /// without checking the access control list.
    ///
    SessionOwner(u8, Vec<u8>),

    /// A chat message
    ///
    /// Chat message sent by the server with the user ID 0 are server messages.
    /// (Typically a Command message is used for server announcements, but the Chat message
    /// is used for those messages that must be stored in the session history.)
    ///
    Chat(u8, ChatMessage),

    /// List of trusted users
    ///
    /// This message sets the list of user who have been tagged as trusted,
    /// but who are not operators. The meaning of "trusted" is a mostly
    /// clientside concept, but the session can be configured to allow trusted
    /// users access to some operator commands. (Deputies)
    ///
    /// This command can be sent by operators or by the server (ctx=0).
    ///
    /// The server sanitizes the ID list so, when distributed to other users,
    /// it does not contain any duplicates or non-existing users and can be trusted
    /// without checking the access control list.
    ///
    TrustedUsers(u8, Vec<u8>),

    /// Soft reset point marker
    ///
    /// This message marks the point in the session history where a soft reset occurs.
    /// A thick-server performs an internal soft-reset when a user joins.
    ///
    /// All users should truncate their own session history when receiving this message,
    /// since undos cannot cross the reset boundary.
    ///
    SoftReset(u8),

    /// A private chat message
    ///
    /// Note. This message type was added in protocol 4.21.2 (v. 2.1.0). For backward compatiblity,
    /// the server will not send any private messages from itself; it will only relay them from
    /// other users. In version 3.0, this should be merged with the normal Chat message.
    ///
    /// Private messages always bypass the session history.
    ///
    PrivateChat(u8, PrivateChatMessage),
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ClientMetaMessage {
    /// Event interval record
    ///
    /// This is used to preserve timing information in session recordings.
    ///
    /// Note. The maximum interval (using a single message) is about 65 seconds.
    /// Typically the intervals we want to store are a few seconds at most, so this should be enough.
    ///
    Interval(u8, u16),

    /// Start/end drawing pointer laser trail
    ///
    /// This signals the beginning or the end of a laser pointer trail. The trail coordinates
    /// are sent with MovePointer messages.
    ///
    /// A nonzero persistence indicates the start of the trail and zero the end.
    ///
    LaserTrail(u8, LaserTrailMessage),

    /// Move user pointer
    ///
    /// This is message is used to update the position of the user pointer when no
    /// actual drawing is taking place. It is also used to draw the "laser pointer" trail.
    /// Note. This is a META message, since this is used for a temporary visual effect only,
    /// and thus doesn't affect the actual canvas content.
    ///
    /// The pointer position is given in integer coordinates.
    ///
    MovePointer(u8, MovePointerMessage),

    /// A bookmark
    ///
    /// This is used to bookmark points in the session for quick access when playing back a recording
    ///
    Marker(u8, String),

    /// Set user specific locks
    ///
    /// This is an opaque meta command that contains a list of users to be locked.
    /// It can only be sent by session operators.
    ///
    UserACL(u8, Vec<u8>),

    /// Change layer access control list
    ///
    /// This is an opaque meta command. It is used to set the general layer lock
    /// as well as give exclusive access to selected users.
    ///
    /// When the OWNLAYERS mode is set, any user can use this to change the ACLs on layers they themselves
    /// have created (identified by the ID prefix.)
    ///
    /// Using layer ID 0 sets or clears a general canvaswide lock. The tier and exclusive user list is not
    /// used in this case.
    ///
    /// The eighth bit of the flags field (0x80) indicates whether the layer is locked in general.
    /// The first three bits (0x07) indicate the access tier level.
    ///
    LayerACL(u8, LayerACLMessage),

    /// Change feature access tiers
    FeatureAccessLevels(u8, Vec<u8>),

    /// Set the default layer
    ///
    /// The default layer is the one new users default to when logging in.
    /// If no default layer is set, the newest layer will be selected by default.
    ///
    DefaultLayer(u8, u16),

    /// A message that has been filtered away by the ACL filter
    ///
    /// This is only used in recordings for mainly debugging purposes.
    /// This message should never be sent over the network.
    ///
    Filtered(u8, Vec<u8>),
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum CommandMessage {
    /// Undo demarcation point
    ///
    /// The client sends an UndoPoint message to signal the start of an undoable sequence.
    ///
    UndoPoint(u8),

    /// Adjust canvas size
    ///
    /// This is the first command that must be sent to initialize the session.
    ///
    /// This affects the size of all existing and future layers.
    ///
    /// The new canvas size is relative to the old one. The four adjustement
    /// parameters extend or retract their respective borders.
    /// Initial canvas resize should be (0, w, h, 0).
    ///
    CanvasResize(u8, CanvasResizeMessage),

    /// Create a new layer
    ///
    /// A session starts with zero layers, so a layer creation command is typically
    /// the second command to be sent, right after setting the canvas size.
    ///
    /// The layer ID must be prefixed with the context ID of the user creating it.
    /// This allows the client to choose the layer ID without worrying about
    /// clashes. In multiuser mode the ACL filter validates the prefix for all new layers.
    ///
    /// If the `source` field is nonzero, a copy of the source layer is made.
    /// Otherwise, either a blank new bitmap or a group layer is created.
    /// When copying a group, the group's layers are assigned new IDs sequentally,
    /// starting from the group ID, using the group IDs user prefix.
    ///
    /// If the `target` field is nonzero, the newly created layer will be
    /// insert above that layer or group, or into that group. If zero,
    /// the layer will be added to the top of the root group.
    ///
    /// The following flags can be used with layer creation:
    /// - GROUP: a group layer is created (ignored if `source` is set)
    /// - INTO: the new layer will be added to the top to the `target` group.
    ///         The target must be nonzero.
    ///
    /// If layer controls are locked, this command requires session operator privileges.
    ///
    LayerCreate(u8, LayerCreateMessage),

    /// Change layer attributes
    ///
    /// If the target layer is locked, this command requires session operator privileges.
    ///
    /// Specifying a sublayer requires session operator privileges. Currently, it is used
    /// only when sublayers are needed at canvas initialization.
    ///
    /// Note: the `fixed` flag is unused since version 2.2. It's functionality is replaced
    /// by the custom timeline feature.
    ///
    LayerAttributes(u8, LayerAttributesMessage),

    /// Change a layer's title
    LayerRetitle(u8, LayerRetitleMessage),

    /// Reorder layers
    ///
    /// The layer tree of the given group (0 means whole tree) will be reordered
    /// according to the given order.
    /// The order should describe a tree using (child count, layer ID) pairs.
    ///
    /// For example (indented for clarity):
    ///
    ///   2, 1,
    ///     0, 10,
    ///     0, 11,
    ///   0, 2,
    ///   2, 3,
    ///     1, 30,
    ///       0, 31
    ///     0, 32
    ///
    ///  Each layer in the group must be listed exactly once in the new order,
    ///  or the command will be rejected.
    ///
    LayerOrder(u8, LayerOrderMessage),

    /// Delete a layer
    ///
    /// If the merge attribute is nonzero, the contents of the layer is merged
    /// to the layer with the given ID.
    ///
    /// If the current layer or layer controls in general are locked, this command
    /// requires session operator privileges.
    ///
    LayerDelete(u8, LayerDeleteMessage),

    /// Toggle layer local visibility (this is used internally only and never sent over the network)
    LayerVisibility(u8, LayerVisibilityMessage),

    /// Draw a bitmap onto a layer
    ///
    /// This is used for pasting images, floodfill, merging annotations and
    /// other tasks where image processing is done clientisde.
    ///
    /// All layer blending modes are supported.
    ///
    /// The image data is DEFLATEd 32bit premultiplied ARGB data. The image
    /// is prefixed with a 32 bit unsigned integer (big endian) which contains
    /// the expected length of the uncompressed data.
    ///
    /// Note that since the message length is fairly limited, a
    /// large image may have to be divided into multiple PutImage
    /// commands.
    ///
    PutImage(u8, PutImageMessage),

    /// Fill a rectangle with solid color
    FillRect(u8, FillRectMessage),

    /// Pen up command
    ///
    /// The pen up command signals the end of a stroke. In indirect drawing mode, it causes
    /// indirect dabs (by this user) to be merged to their parent layers.
    ///
    PenUp(u8),

    /// Create a new annotation
    ///
    /// Annotations are floating text layers. They are drawn over the image layers and
    /// have no defined stacking order.
    ///
    /// The new annotation created with this command is initally empy with a transparent background
    ///
    AnnotationCreate(u8, AnnotationCreateMessage),

    /// Change the position and size of an annotation
    AnnotationReshape(u8, AnnotationReshapeMessage),

    /// Change annotation content
    ///
    /// Accepted contents is the subset of HTML understood by QTextDocument
    ///
    /// If an annotation is flagged as protected, it cannot be modified by users
    /// other than the one who created it, or session operators.
    ///
    AnnotationEdit(u8, AnnotationEditMessage),

    /// Delete an annotation
    ///
    /// Note: Unlike in layer delete command, there is no "merge" option here.
    /// Merging an annotation is done by rendering the annotation item to
    /// an image and drawing the image with the PutImage command. This ensures
    /// identical rendering on all clients.
    ///
    AnnotationDelete(u8, u16),

    /// Set the content of a tile
    ///
    /// Unlike PutImage, this replaces an entire tile directly without any blending.
    /// This command is typically used during canvas initialization to set the initial content.
    ///
    /// PutTile can target sublayers as well. This is used when generating a reset image
    /// with incomplete indirect strokes. Sending a PenUp command will merge the sublayer.
    ///
    PutTile(u8, PutTileMessage),

    /// Set the canvas background tile
    ///
    /// If the payload is exactly 4 bytes long, it should be interpreted as a solid background color.
    /// Otherwise, it is the DEFLATED tile bitmap
    ///
    CanvasBackground(u8, Vec<u8>),

    /// Draw classic brush dabs
    ///
    /// A simple delta compression scheme is used.
    /// The coordinates of each dab are relative to the previous dab.
    /// The coordinate system has 1/4 pixel resolution. Divide by 4.0 before use.
    /// The size field is the brush diameter multiplied by 256.
    ///
    DrawDabsClassic(u8, DrawDabsClassicMessage),

    /// Draw round pixel brush dabs
    ///
    /// The same kind of delta compression is used as in classicdabs,
    /// but the fields all have integer precision.
    ///
    DrawDabsPixel(u8, DrawDabsPixelMessage),

    /// Draw square pixel brush dabs
    DrawDabsPixelSquare(u8, DrawDabsPixelMessage),

    /// Draw MyPaint brush dabs
    DrawDabsMyPaint(u8, DrawDabsMyPaintMessage),

    /// Move a rectangular area on a layer.
    ///
    /// A mask image can be given to mask out part of the region
    /// to support non-rectangular selections.
    ///
    /// Source and target rects may be (partially) outside the canvas.
    ///
    /// Note: The MoveRegion command that can also transform the
    /// selection is currently not implemented. The same effect can be
    /// achieved by performing the transformation clientside then
    /// sending the results as PutImages, including one to erase the
    /// source.
    ///
    MoveRect(u8, MoveRectMessage),

    /// Set a document metadata field (integer type)
    ///
    /// These typically don't have an immediate visual effect,
    /// but these fields are part of the document, like the pixel content
    /// or the annotations.
    ///
    SetMetadataInt(u8, SetMetadataIntMessage),

    /// Set a document metadata field (string type)
    SetMetadataStr(u8, SetMetadataStrMessage),

    /// Set the layers included in the given animation frame
    ///
    /// The frame number must be between zero and last frame number + 1.
    ///
    /// If the `insert` flag is true, the frame will be inserted at the given
    /// position rather than replacing the existing frame.
    ///
    SetTimelineFrame(u8, SetTimelineFrameMessage),

    /// Remove a frame from the timeline
    RemoveTimelineFrame(u8, u16),

    /// Undo or redo actions
    Undo(u8, UndoMessage),
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Message {
    Control(ControlMessage),
    ServerMeta(ServerMetaMessage),
    ClientMeta(ClientMetaMessage),
    Command(CommandMessage),
}

impl ControlMessage {
    pub fn write(&self, w: &mut MessageWriter) {
        use ControlMessage::*;
        match &self {
            ServerCommand(user_id, b) => w.single(0, *user_id, b),
            Disconnect(user_id, b) => b.serialize(w, *user_id),
            Ping(user_id, b) => w.single(2, *user_id, *b),
        }
    }

    pub fn as_text(&self) -> TextMessage {
        use ControlMessage::*;
        match &self {
            ServerCommand(user_id, b) => {
                TextMessage::new(*user_id, "servercommand").set_bytes("msg", &b)
            }
            Disconnect(user_id, b) => b.to_text(TextMessage::new(*user_id, "disconnect")),
            Ping(user_id, b) => TextMessage::new(*user_id, "ping").set("is_pong", b.to_string()),
        }
    }

    pub fn user(&self) -> u8 {
        use ControlMessage::*;
        match &self {
            ServerCommand(user_id, _) => *user_id,
            Disconnect(user_id, _) => *user_id,
            Ping(user_id, _) => *user_id,
        }
    }
}

impl fmt::Display for ControlMessage {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.as_text().fmt(f)
    }
}

impl From<ControlMessage> for Message {
    fn from(item: ControlMessage) -> Message {
        Message::Control(item)
    }
}

impl ServerMetaMessage {
    pub fn write(&self, w: &mut MessageWriter) {
        use ServerMetaMessage::*;
        match &self {
            Join(user_id, b) => b.serialize(w, *user_id),
            Leave(user_id) => w.write_header(33, *user_id, 0),
            SessionOwner(user_id, b) => w.single(34, *user_id, b),
            Chat(user_id, b) => b.serialize(w, *user_id),
            TrustedUsers(user_id, b) => w.single(36, *user_id, b),
            SoftReset(user_id) => w.write_header(37, *user_id, 0),
            PrivateChat(user_id, b) => b.serialize(w, *user_id),
        }
    }

    pub fn as_text(&self) -> TextMessage {
        use ServerMetaMessage::*;
        match &self {
            Join(user_id, b) => b.to_text(TextMessage::new(*user_id, "join")),
            Leave(user_id) => TextMessage::new(*user_id, "leave"),
            SessionOwner(user_id, b) => {
                TextMessage::new(*user_id, "sessionowner").set_vec_u8("users", &b)
            }
            Chat(user_id, b) => b.to_text(TextMessage::new(*user_id, "chat")),
            TrustedUsers(user_id, b) => {
                TextMessage::new(*user_id, "trusted").set_vec_u8("users", &b)
            }
            SoftReset(user_id) => TextMessage::new(*user_id, "softreset"),
            PrivateChat(user_id, b) => b.to_text(TextMessage::new(*user_id, "privatechat")),
        }
    }

    pub fn user(&self) -> u8 {
        use ServerMetaMessage::*;
        match &self {
            Join(user_id, _) => *user_id,
            Leave(user_id) => *user_id,
            SessionOwner(user_id, _) => *user_id,
            Chat(user_id, _) => *user_id,
            TrustedUsers(user_id, _) => *user_id,
            SoftReset(user_id) => *user_id,
            PrivateChat(user_id, _) => *user_id,
        }
    }
}

impl fmt::Display for ServerMetaMessage {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.as_text().fmt(f)
    }
}

impl From<ServerMetaMessage> for Message {
    fn from(item: ServerMetaMessage) -> Message {
        Message::ServerMeta(item)
    }
}

impl ClientMetaMessage {
    pub fn write(&self, w: &mut MessageWriter) {
        use ClientMetaMessage::*;
        match &self {
            Interval(user_id, b) => w.single(64, *user_id, *b),
            LaserTrail(user_id, b) => b.serialize(w, *user_id),
            MovePointer(user_id, b) => b.serialize(w, *user_id),
            Marker(user_id, b) => w.single(67, *user_id, b),
            UserACL(user_id, b) => w.single(68, *user_id, b),
            LayerACL(user_id, b) => b.serialize(w, *user_id),
            FeatureAccessLevels(user_id, b) => w.single(70, *user_id, b),
            DefaultLayer(user_id, b) => w.single(71, *user_id, *b),
            Filtered(user_id, b) => w.single(72, *user_id, b),
        }
    }

    pub fn as_text(&self) -> TextMessage {
        use ClientMetaMessage::*;
        match &self {
            Interval(user_id, b) => {
                TextMessage::new(*user_id, "interval").set("msecs", b.to_string())
            }
            LaserTrail(user_id, b) => b.to_text(TextMessage::new(*user_id, "lasertrail")),
            MovePointer(user_id, b) => b.to_text(TextMessage::new(*user_id, "movepointer")),
            Marker(user_id, b) => TextMessage::new(*user_id, "marker").set("text", b.clone()),
            UserACL(user_id, b) => TextMessage::new(*user_id, "useracl").set_vec_u8("users", &b),
            LayerACL(user_id, b) => b.to_text(TextMessage::new(*user_id, "layeracl")),
            FeatureAccessLevels(user_id, b) => {
                TextMessage::new(*user_id, "featureaccess").set_vec_u8("feature_tiers", &b)
            }
            DefaultLayer(user_id, b) => {
                TextMessage::new(*user_id, "defaultlayer").set("id", format!("0x{:04x}", b))
            }
            Filtered(user_id, b) => TextMessage::new(*user_id, "filtered").set_bytes("message", &b),
        }
    }

    pub fn user(&self) -> u8 {
        use ClientMetaMessage::*;
        match &self {
            Interval(user_id, _) => *user_id,
            LaserTrail(user_id, _) => *user_id,
            MovePointer(user_id, _) => *user_id,
            Marker(user_id, _) => *user_id,
            UserACL(user_id, _) => *user_id,
            LayerACL(user_id, _) => *user_id,
            FeatureAccessLevels(user_id, _) => *user_id,
            DefaultLayer(user_id, _) => *user_id,
            Filtered(user_id, _) => *user_id,
        }
    }
}

impl fmt::Display for ClientMetaMessage {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.as_text().fmt(f)
    }
}

impl From<ClientMetaMessage> for Message {
    fn from(item: ClientMetaMessage) -> Message {
        Message::ClientMeta(item)
    }
}

impl CommandMessage {
    pub fn write(&self, w: &mut MessageWriter) {
        use CommandMessage::*;
        match &self {
            UndoPoint(user_id) => w.write_header(128, *user_id, 0),
            CanvasResize(user_id, b) => b.serialize(w, *user_id),
            LayerCreate(user_id, b) => b.serialize(w, *user_id),
            LayerAttributes(user_id, b) => b.serialize(w, *user_id),
            LayerRetitle(user_id, b) => b.serialize(w, *user_id),
            LayerOrder(user_id, b) => b.serialize(w, *user_id),
            LayerDelete(user_id, b) => b.serialize(w, *user_id),
            LayerVisibility(user_id, b) => b.serialize(w, *user_id),
            PutImage(user_id, b) => b.serialize(w, *user_id),
            FillRect(user_id, b) => b.serialize(w, *user_id),
            PenUp(user_id) => w.write_header(140, *user_id, 0),
            AnnotationCreate(user_id, b) => b.serialize(w, *user_id),
            AnnotationReshape(user_id, b) => b.serialize(w, *user_id),
            AnnotationEdit(user_id, b) => b.serialize(w, *user_id),
            AnnotationDelete(user_id, b) => w.single(144, *user_id, *b),
            PutTile(user_id, b) => b.serialize(w, *user_id),
            CanvasBackground(user_id, b) => w.single(147, *user_id, b),
            DrawDabsClassic(user_id, b) => b.serialize(w, *user_id),
            DrawDabsPixel(user_id, b) => b.serialize(w, 149, *user_id),
            DrawDabsPixelSquare(user_id, b) => b.serialize(w, 150, *user_id),
            DrawDabsMyPaint(user_id, b) => b.serialize(w, *user_id),
            MoveRect(user_id, b) => b.serialize(w, *user_id),
            SetMetadataInt(user_id, b) => b.serialize(w, *user_id),
            SetMetadataStr(user_id, b) => b.serialize(w, *user_id),
            SetTimelineFrame(user_id, b) => b.serialize(w, *user_id),
            RemoveTimelineFrame(user_id, b) => w.single(164, *user_id, *b),
            Undo(user_id, b) => b.serialize(w, *user_id),
        }
    }

    pub fn as_text(&self) -> TextMessage {
        use CommandMessage::*;
        match &self {
            UndoPoint(user_id) => TextMessage::new(*user_id, "undopoint"),
            CanvasResize(user_id, b) => b.to_text(TextMessage::new(*user_id, "resize")),
            LayerCreate(user_id, b) => b.to_text(TextMessage::new(*user_id, "newlayer")),
            LayerAttributes(user_id, b) => b.to_text(TextMessage::new(*user_id, "layerattr")),
            LayerRetitle(user_id, b) => b.to_text(TextMessage::new(*user_id, "retitlelayer")),
            LayerOrder(user_id, b) => b.to_text(TextMessage::new(*user_id, "layerorder")),
            LayerDelete(user_id, b) => b.to_text(TextMessage::new(*user_id, "deletelayer")),
            LayerVisibility(user_id, b) => b.to_text(TextMessage::new(*user_id, "layervisibility")),
            PutImage(user_id, b) => b.to_text(TextMessage::new(*user_id, "putimage")),
            FillRect(user_id, b) => b.to_text(TextMessage::new(*user_id, "fillrect")),
            PenUp(user_id) => TextMessage::new(*user_id, "penup"),
            AnnotationCreate(user_id, b) => b.to_text(TextMessage::new(*user_id, "newannotation")),
            AnnotationReshape(user_id, b) => {
                b.to_text(TextMessage::new(*user_id, "reshapeannotation"))
            }
            AnnotationEdit(user_id, b) => b.to_text(TextMessage::new(*user_id, "editannotation")),
            AnnotationDelete(user_id, b) => {
                TextMessage::new(*user_id, "deleteannotation").set("id", format!("0x{:04x}", b))
            }
            PutTile(user_id, b) => b.to_text(TextMessage::new(*user_id, "puttile")),
            CanvasBackground(user_id, b) => {
                TextMessage::new(*user_id, "background").set_bytes("image", &b)
            }
            DrawDabsClassic(user_id, b) => b.to_text(TextMessage::new(*user_id, "classicdabs")),
            DrawDabsPixel(user_id, b) => b.to_text(TextMessage::new(*user_id, "pixeldabs")),
            DrawDabsPixelSquare(user_id, b) => {
                b.to_text(TextMessage::new(*user_id, "squarepixeldabs"))
            }
            DrawDabsMyPaint(user_id, b) => b.to_text(TextMessage::new(*user_id, "mypaintdabs")),
            MoveRect(user_id, b) => b.to_text(TextMessage::new(*user_id, "moverect")),
            SetMetadataInt(user_id, b) => b.to_text(TextMessage::new(*user_id, "setmetadataint")),
            SetMetadataStr(user_id, b) => b.to_text(TextMessage::new(*user_id, "setmetadatastr")),
            SetTimelineFrame(user_id, b) => {
                b.to_text(TextMessage::new(*user_id, "settimelineframe"))
            }
            RemoveTimelineFrame(user_id, b) => {
                TextMessage::new(*user_id, "removetimelineframe").set("frame", b.to_string())
            }
            Undo(user_id, b) => b.to_text(TextMessage::new(*user_id, "undo")),
        }
    }

    pub fn user(&self) -> u8 {
        use CommandMessage::*;
        match &self {
            UndoPoint(user_id) => *user_id,
            CanvasResize(user_id, _) => *user_id,
            LayerCreate(user_id, _) => *user_id,
            LayerAttributes(user_id, _) => *user_id,
            LayerRetitle(user_id, _) => *user_id,
            LayerOrder(user_id, _) => *user_id,
            LayerDelete(user_id, _) => *user_id,
            LayerVisibility(user_id, _) => *user_id,
            PutImage(user_id, _) => *user_id,
            FillRect(user_id, _) => *user_id,
            PenUp(user_id) => *user_id,
            AnnotationCreate(user_id, _) => *user_id,
            AnnotationReshape(user_id, _) => *user_id,
            AnnotationEdit(user_id, _) => *user_id,
            AnnotationDelete(user_id, _) => *user_id,
            PutTile(user_id, _) => *user_id,
            CanvasBackground(user_id, _) => *user_id,
            DrawDabsClassic(user_id, _) => *user_id,
            DrawDabsPixel(user_id, _) => *user_id,
            DrawDabsPixelSquare(user_id, _) => *user_id,
            DrawDabsMyPaint(user_id, _) => *user_id,
            MoveRect(user_id, _) => *user_id,
            SetMetadataInt(user_id, _) => *user_id,
            SetMetadataStr(user_id, _) => *user_id,
            SetTimelineFrame(user_id, _) => *user_id,
            RemoveTimelineFrame(user_id, _) => *user_id,
            Undo(user_id, _) => *user_id,
        }
    }
}

impl fmt::Display for CommandMessage {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.as_text().fmt(f)
    }
}

impl From<CommandMessage> for Message {
    fn from(item: CommandMessage) -> Message {
        Message::Command(item)
    }
}

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
            0 => Control(ControlMessage::ServerCommand(
                u,
                r.validate(0, 65535)?.read_remaining_vec::<u8>(),
            )),
            1 => Control(ControlMessage::Disconnect(
                u,
                DisconnectMessage::deserialize(r)?,
            )),
            2 => Control(ControlMessage::Ping(u, r.validate(1, 1)?.read::<bool>())),
            32 => ServerMeta(ServerMetaMessage::Join(u, JoinMessage::deserialize(r)?)),
            33 => ServerMeta(ServerMetaMessage::Leave(u)),
            34 => ServerMeta(ServerMetaMessage::SessionOwner(
                u,
                r.validate(0, 255)?.read_remaining_vec(),
            )),
            35 => ServerMeta(ServerMetaMessage::Chat(u, ChatMessage::deserialize(r)?)),
            36 => ServerMeta(ServerMetaMessage::TrustedUsers(
                u,
                r.validate(0, 255)?.read_remaining_vec(),
            )),
            37 => ServerMeta(ServerMetaMessage::SoftReset(u)),
            38 => ServerMeta(ServerMetaMessage::PrivateChat(
                u,
                PrivateChatMessage::deserialize(r)?,
            )),
            64 => ClientMeta(ClientMetaMessage::Interval(
                u,
                r.validate(2, 2)?.read::<u16>(),
            )),
            65 => ClientMeta(ClientMetaMessage::LaserTrail(
                u,
                LaserTrailMessage::deserialize(r)?,
            )),
            66 => ClientMeta(ClientMetaMessage::MovePointer(
                u,
                MovePointerMessage::deserialize(r)?,
            )),
            67 => ClientMeta(ClientMetaMessage::Marker(
                u,
                r.validate(0, 65535)?.read_remaining_str(),
            )),
            68 => ClientMeta(ClientMetaMessage::UserACL(
                u,
                r.validate(0, 255)?.read_remaining_vec(),
            )),
            69 => ClientMeta(ClientMetaMessage::LayerACL(
                u,
                LayerACLMessage::deserialize(r)?,
            )),
            70 => ClientMeta(ClientMetaMessage::FeatureAccessLevels(
                u,
                r.validate(11, 11)?.read_remaining_vec(),
            )),
            71 => ClientMeta(ClientMetaMessage::DefaultLayer(
                u,
                r.validate(2, 2)?.read::<u16>(),
            )),
            72 => ClientMeta(ClientMetaMessage::Filtered(
                u,
                r.validate(0, 65535)?.read_remaining_vec::<u8>(),
            )),
            128 => Command(CommandMessage::UndoPoint(u)),
            129 => Command(CommandMessage::CanvasResize(
                u,
                CanvasResizeMessage::deserialize(r)?,
            )),
            130 => Command(CommandMessage::LayerCreate(
                u,
                LayerCreateMessage::deserialize(r)?,
            )),
            131 => Command(CommandMessage::LayerAttributes(
                u,
                LayerAttributesMessage::deserialize(r)?,
            )),
            132 => Command(CommandMessage::LayerRetitle(
                u,
                LayerRetitleMessage::deserialize(r)?,
            )),
            133 => Command(CommandMessage::LayerOrder(
                u,
                LayerOrderMessage::deserialize(r)?,
            )),
            134 => Command(CommandMessage::LayerDelete(
                u,
                LayerDeleteMessage::deserialize(r)?,
            )),
            135 => Command(CommandMessage::LayerVisibility(
                u,
                LayerVisibilityMessage::deserialize(r)?,
            )),
            136 => Command(CommandMessage::PutImage(
                u,
                PutImageMessage::deserialize(r)?,
            )),
            137 => Command(CommandMessage::FillRect(
                u,
                FillRectMessage::deserialize(r)?,
            )),
            140 => Command(CommandMessage::PenUp(u)),
            141 => Command(CommandMessage::AnnotationCreate(
                u,
                AnnotationCreateMessage::deserialize(r)?,
            )),
            142 => Command(CommandMessage::AnnotationReshape(
                u,
                AnnotationReshapeMessage::deserialize(r)?,
            )),
            143 => Command(CommandMessage::AnnotationEdit(
                u,
                AnnotationEditMessage::deserialize(r)?,
            )),
            144 => Command(CommandMessage::AnnotationDelete(
                u,
                r.validate(2, 2)?.read::<u16>(),
            )),
            146 => Command(CommandMessage::PutTile(u, PutTileMessage::deserialize(r)?)),
            147 => Command(CommandMessage::CanvasBackground(
                u,
                r.validate(0, 65535)?.read_remaining_vec::<u8>(),
            )),
            148 => Command(CommandMessage::DrawDabsClassic(
                u,
                DrawDabsClassicMessage::deserialize(r)?,
            )),
            149 => Command(CommandMessage::DrawDabsPixel(
                u,
                DrawDabsPixelMessage::deserialize(r)?,
            )),
            150 => Command(CommandMessage::DrawDabsPixelSquare(
                u,
                DrawDabsPixelMessage::deserialize(r)?,
            )),
            151 => Command(CommandMessage::DrawDabsMyPaint(
                u,
                DrawDabsMyPaintMessage::deserialize(r)?,
            )),
            160 => Command(CommandMessage::MoveRect(
                u,
                MoveRectMessage::deserialize(r)?,
            )),
            161 => Command(CommandMessage::SetMetadataInt(
                u,
                SetMetadataIntMessage::deserialize(r)?,
            )),
            162 => Command(CommandMessage::SetMetadataStr(
                u,
                SetMetadataStrMessage::deserialize(r)?,
            )),
            163 => Command(CommandMessage::SetTimelineFrame(
                u,
                SetTimelineFrameMessage::deserialize(r)?,
            )),
            164 => Command(CommandMessage::RemoveTimelineFrame(
                u,
                r.validate(2, 2)?.read::<u16>(),
            )),
            255 => Command(CommandMessage::Undo(u, UndoMessage::deserialize(r)?)),
            _ => {
                return Err(DeserializationError::UnknownMessage(
                    u,
                    message_type,
                    r.remaining(),
                ));
            }
        })
    }

    pub fn write(&self, w: &mut MessageWriter) {
        use Message::*;
        match &self {
            Control(m) => m.write(w),
            ServerMeta(m) => m.write(w),
            ClientMeta(m) => m.write(w),
            Command(m) => m.write(w),
        }
    }

    pub fn from_text(tm: &TextMessage) -> Option<Message> {
        // tm.user_id
        use Message::*;
        Some(match tm.name.as_ref() {
            "servercommand" => Control(ControlMessage::ServerCommand(
                tm.user_id,
                tm.get_bytes("msg"),
            )),
            "disconnect" => Control(ControlMessage::Disconnect(
                tm.user_id,
                DisconnectMessage::from_text(tm),
            )),
            "ping" => Control(ControlMessage::Ping(
                tm.user_id,
                tm.get_str("is_pong") == "true",
            )),
            "join" => ServerMeta(ServerMetaMessage::Join(
                tm.user_id,
                JoinMessage::from_text(tm),
            )),
            "leave" => ServerMeta(ServerMetaMessage::Leave(tm.user_id)),
            "sessionowner" => ServerMeta(ServerMetaMessage::SessionOwner(
                tm.user_id,
                tm.get_vec_u8("users"),
            )),
            "chat" => ServerMeta(ServerMetaMessage::Chat(
                tm.user_id,
                ChatMessage::from_text(tm),
            )),
            "trusted" => ServerMeta(ServerMetaMessage::TrustedUsers(
                tm.user_id,
                tm.get_vec_u8("users"),
            )),
            "softreset" => ServerMeta(ServerMetaMessage::SoftReset(tm.user_id)),
            "privatechat" => ServerMeta(ServerMetaMessage::PrivateChat(
                tm.user_id,
                PrivateChatMessage::from_text(tm),
            )),
            "interval" => ClientMeta(ClientMetaMessage::Interval(tm.user_id, tm.get_u16("msecs"))),
            "lasertrail" => ClientMeta(ClientMetaMessage::LaserTrail(
                tm.user_id,
                LaserTrailMessage::from_text(tm),
            )),
            "movepointer" => ClientMeta(ClientMetaMessage::MovePointer(
                tm.user_id,
                MovePointerMessage::from_text(tm),
            )),
            "marker" => ClientMeta(ClientMetaMessage::Marker(
                tm.user_id,
                tm.get_str("text").to_string(),
            )),
            "useracl" => ClientMeta(ClientMetaMessage::UserACL(
                tm.user_id,
                tm.get_vec_u8("users"),
            )),
            "layeracl" => ClientMeta(ClientMetaMessage::LayerACL(
                tm.user_id,
                LayerACLMessage::from_text(tm),
            )),
            "featureaccess" => ClientMeta(ClientMetaMessage::FeatureAccessLevels(
                tm.user_id,
                tm.get_vec_u8("feature_tiers"),
            )),
            "defaultlayer" => ClientMeta(ClientMetaMessage::DefaultLayer(
                tm.user_id,
                tm.get_u16("id"),
            )),
            "filtered" => ClientMeta(ClientMetaMessage::Filtered(
                tm.user_id,
                tm.get_bytes("message"),
            )),
            "undopoint" => Command(CommandMessage::UndoPoint(tm.user_id)),
            "resize" => Command(CommandMessage::CanvasResize(
                tm.user_id,
                CanvasResizeMessage::from_text(tm),
            )),
            "newlayer" => Command(CommandMessage::LayerCreate(
                tm.user_id,
                LayerCreateMessage::from_text(tm),
            )),
            "layerattr" => Command(CommandMessage::LayerAttributes(
                tm.user_id,
                LayerAttributesMessage::from_text(tm),
            )),
            "retitlelayer" => Command(CommandMessage::LayerRetitle(
                tm.user_id,
                LayerRetitleMessage::from_text(tm),
            )),
            "layerorder" => Command(CommandMessage::LayerOrder(
                tm.user_id,
                LayerOrderMessage::from_text(tm),
            )),
            "deletelayer" => Command(CommandMessage::LayerDelete(
                tm.user_id,
                LayerDeleteMessage::from_text(tm),
            )),
            "layervisibility" => Command(CommandMessage::LayerVisibility(
                tm.user_id,
                LayerVisibilityMessage::from_text(tm),
            )),
            "putimage" => Command(CommandMessage::PutImage(
                tm.user_id,
                PutImageMessage::from_text(tm),
            )),
            "fillrect" => Command(CommandMessage::FillRect(
                tm.user_id,
                FillRectMessage::from_text(tm),
            )),
            "penup" => Command(CommandMessage::PenUp(tm.user_id)),
            "newannotation" => Command(CommandMessage::AnnotationCreate(
                tm.user_id,
                AnnotationCreateMessage::from_text(tm),
            )),
            "reshapeannotation" => Command(CommandMessage::AnnotationReshape(
                tm.user_id,
                AnnotationReshapeMessage::from_text(tm),
            )),
            "editannotation" => Command(CommandMessage::AnnotationEdit(
                tm.user_id,
                AnnotationEditMessage::from_text(tm),
            )),
            "deleteannotation" => Command(CommandMessage::AnnotationDelete(
                tm.user_id,
                tm.get_u16("id"),
            )),
            "puttile" => Command(CommandMessage::PutTile(
                tm.user_id,
                PutTileMessage::from_text(tm),
            )),
            "background" => Command(CommandMessage::CanvasBackground(
                tm.user_id,
                tm.get_bytes("image"),
            )),
            "classicdabs" => Command(CommandMessage::DrawDabsClassic(
                tm.user_id,
                DrawDabsClassicMessage::from_text(tm),
            )),
            "pixeldabs" => Command(CommandMessage::DrawDabsPixel(
                tm.user_id,
                DrawDabsPixelMessage::from_text(tm),
            )),
            "squarepixeldabs" => Command(CommandMessage::DrawDabsPixelSquare(
                tm.user_id,
                DrawDabsPixelMessage::from_text(tm),
            )),
            "mypaintdabs" => Command(CommandMessage::DrawDabsMyPaint(
                tm.user_id,
                DrawDabsMyPaintMessage::from_text(tm),
            )),
            "moverect" => Command(CommandMessage::MoveRect(
                tm.user_id,
                MoveRectMessage::from_text(tm),
            )),
            "setmetadataint" => Command(CommandMessage::SetMetadataInt(
                tm.user_id,
                SetMetadataIntMessage::from_text(tm),
            )),
            "setmetadatastr" => Command(CommandMessage::SetMetadataStr(
                tm.user_id,
                SetMetadataStrMessage::from_text(tm),
            )),
            "settimelineframe" => Command(CommandMessage::SetTimelineFrame(
                tm.user_id,
                SetTimelineFrameMessage::from_text(tm),
            )),
            "removetimelineframe" => Command(CommandMessage::RemoveTimelineFrame(
                tm.user_id,
                tm.get_u16("frame"),
            )),
            "undo" => Command(CommandMessage::Undo(tm.user_id, UndoMessage::from_text(tm))),
            _ => {
                return None;
            }
        })
    }

    pub fn as_text(&self) -> TextMessage {
        use Message::*;
        match &self {
            Control(m) => m.as_text(),
            ServerMeta(m) => m.as_text(),
            ClientMeta(m) => m.as_text(),
            Command(m) => m.as_text(),
        }
    }
}

impl fmt::Display for Message {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.as_text().fmt(f)
    }
}
