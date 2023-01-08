use super::message::*;
use crate::paint::{LayerID, UserID};

use num_enum::IntoPrimitive;
use num_enum::TryFromPrimitive;
use std::collections::{HashMap, HashSet};
use std::convert::TryFrom;

/// Bitfield for storing a set of users (IDs range from 0..255)
pub type UserBits = [u8; 32];

/// Feature access tiers
#[derive(Copy, Clone, Debug, PartialEq, PartialOrd, IntoPrimitive, TryFromPrimitive)]
#[repr(u8)]
pub enum Tier {
    Operator,
    Trusted,
    Authenticated,
    #[num_enum(default)]
    Guest,
}

#[repr(C)]
pub struct FeatureTiers {
    /// Use of the PutImage command (covers cut&paste, move with transform, etc.)
    pub put_image: Tier,

    /// Selection moving (without transformation)
    pub move_rect: Tier,

    /// Canvas resize
    pub resize: Tier,

    /// Canvas background changing
    pub background: Tier,

    /// Permission to edit any layer's properties and to reorganize them
    pub edit_layers: Tier,

    /// Permission to create and edit own layers
    pub own_layers: Tier,

    /// Permission to create new annotations
    pub create_annotation: Tier,

    /// Permission to use the laser pointer tool
    pub laser: Tier,

    /// Permission to use undo/redo
    pub undo: Tier,

    /// Permission to edit document metadata
    pub metadata: Tier,

    // Permission to edit animation timeline
    pub timeline: Tier,
}

/// Set of general user related permission bits
#[repr(C)]
pub struct UserACLs {
    pub operators: UserBits,
    pub trusted: UserBits,
    pub authenticated: UserBits,
    pub locked: UserBits,
    pub all_locked: bool,
}

/// Layer specific permissions
#[repr(C)]
pub struct LayerACL {
    /// General layer-wide lock
    pub locked: bool,

    /// Layer general access tier
    pub tier: Tier,

    /// Exclusive access granted to these users
    pub exclusive: UserBits,
}

/// Access Control List filtering for messages
pub struct AclFilter {
    users: UserACLs,
    layers: HashMap<LayerID, LayerACL>,
    locked_annotations: HashSet<LayerID>,
    feature_tier: FeatureTiers,
}

pub type AclChange = u32;

pub const ACLCHANGE_USERBITS: AclChange = 0x01;
pub const ACLCHANGE_LAYERS: AclChange = 0x02;
pub const ACLCHANGE_FEATURES: AclChange = 0x04;

impl UserACLs {
    fn new() -> Self {
        Self {
            operators: [0; 32],
            trusted: [0; 32],
            authenticated: [0; 32],
            locked: [0; 32],
            all_locked: false,
        }
    }

    fn is_op(&self, user: UserID) -> bool {
        user == 0 || is_userbit(&self.operators, user)
    }
}

impl LayerACL {
    pub fn flags(&self) -> u8 {
        u8::from(self.tier) | if self.locked { 0x80 } else { 0 }
    }
}

impl AclFilter {
    /// Create a fresh ACL filter with default settings
    ///
    /// The local_op argument is set to true when running in local mode
    pub fn new() -> Self {
        Self {
            users: UserACLs::new(),
            layers: HashMap::new(),
            locked_annotations: HashSet::new(),
            feature_tier: FeatureTiers {
                put_image: Tier::Guest,
                move_rect: Tier::Guest,
                resize: Tier::Operator,
                background: Tier::Operator,
                edit_layers: Tier::Operator,
                own_layers: Tier::Guest,
                create_annotation: Tier::Guest,
                laser: Tier::Guest,
                undo: Tier::Guest,
                metadata: Tier::Operator,
                timeline: Tier::Guest,
            },
        }
    }

    pub fn users(&self) -> &UserACLs {
        &self.users
    }

    pub fn layers(&self) -> &HashMap<LayerID, LayerACL> {
        &self.layers
    }

    pub fn feature_tiers(&self) -> &FeatureTiers {
        &self.feature_tier
    }

    /// Reset the filter back to local operating mode
    pub fn reset(&mut self, local_user: UserID) {
        *self = AclFilter::new();

        set_userbit(&mut self.users.operators, local_user);
    }

    /// Evalute a message
    ///
    /// This will return true if the message passes the filter.
    /// Some messages will affect the state of the filter itself, in
    /// which case the affected state is returned also. When the
    /// returned AclChange is nonzero, the GUI layer can refresh
    /// itself to match the current state.
    pub fn filter_message(&mut self, msg: &Message) -> (bool, AclChange) {
        match msg {
            // We don't care about these
            Message::Control(_) => (true, 0),

            // No need to validate these but they may affect the filter's state
            Message::ServerMeta(m) => (true, self.handle_servermeta(m)),

            // These need to be validated and may affect the filter's state
            Message::ClientMeta(m) => self.handle_clientmeta(m),

            // These need to be validated but have no externally visible effect on the filter's state
            Message::Command(m) => (self.handle_command(m), 0),
        }
    }

    fn handle_servermeta(&mut self, message: &ServerMetaMessage) -> AclChange {
        use ServerMetaMessage::*;
        match message {
            Join(u, m) => {
                if (m.flags & JoinMessage::FLAGS_AUTH) != 0 {
                    set_userbit(&mut self.users.authenticated, *u);
                    return ACLCHANGE_USERBITS;
                }
            }
            Leave(u) => {
                unset_userbit(&mut self.users.operators, *u);
                unset_userbit(&mut self.users.trusted, *u);
                unset_userbit(&mut self.users.authenticated, *u);
                unset_userbit(&mut self.users.locked, *u);

                // TODO remove layer locks
                return ACLCHANGE_USERBITS;
            }
            SessionOwner(_, users) => {
                self.users.operators = vec_to_userbits(users);
                return ACLCHANGE_USERBITS;
            }
            TrustedUsers(_, users) => {
                self.users.trusted = vec_to_userbits(users);
                return ACLCHANGE_USERBITS;
            }
            // The other commands have no effect on the filter
            _ => (),
        };

        0
    }

    fn handle_clientmeta(&mut self, message: &ClientMetaMessage) -> (bool, AclChange) {
        use ClientMetaMessage::*;
        match message {
            // These only have effect in recordings
            Interval(_, _) => (true, 0),
            LaserTrail(u, _) => (self.users.tier(*u) <= self.feature_tier.laser, 0),
            MovePointer(_, _) => (true, 0),
            Marker(_, _) => (true, 0),
            UserACL(u, users) => {
                if self.users.is_op(*u) {
                    self.users.locked = vec_to_userbits(users);
                    (true, ACLCHANGE_USERBITS)
                } else {
                    (false, 0)
                }
            }
            LayerACL(u, m) => {
                let tier = self.users.tier(*u);
                if tier <= self.feature_tier.edit_layers
                    || (tier <= self.feature_tier.own_layers && layer_creator(m.id) == *u)
                {
                    if m.flags == u8::from(Tier::Guest) && m.exclusive.is_empty() {
                        match self.layers.remove(&m.id) {
                            Some(_) => (true, ACLCHANGE_LAYERS),
                            None => (true, 0),
                        }
                    } else {
                        self.layers.insert(
                            m.id,
                            self::LayerACL {
                                locked: m.flags & 0x80 > 0,
                                tier: Tier::try_from(m.flags & 0x07).unwrap(),
                                exclusive: if m.exclusive.is_empty() {
                                    [0xff; 32]
                                } else {
                                    vec_to_userbits(&m.exclusive)
                                },
                            },
                        );
                        (true, ACLCHANGE_LAYERS)
                    }
                } else {
                    (false, 0)
                }
            }
            FeatureAccessLevels(u, f) => {
                if self.users.is_op(*u) {
                    self.feature_tier = FeatureTiers {
                        put_image: Tier::try_from(f[0]).unwrap(),
                        move_rect: Tier::try_from(f[1]).unwrap(),
                        resize: Tier::try_from(f[2]).unwrap(),
                        background: Tier::try_from(f[3]).unwrap(),
                        edit_layers: Tier::try_from(f[4]).unwrap(),
                        own_layers: Tier::try_from(f[5]).unwrap(),
                        create_annotation: Tier::try_from(f[6]).unwrap(),
                        laser: Tier::try_from(f[7]).unwrap(),
                        undo: Tier::try_from(f[8]).unwrap(),
                        metadata: Tier::try_from(f[9]).unwrap(),
                        timeline: Tier::try_from(f[10]).unwrap(),
                    };
                    (true, ACLCHANGE_FEATURES)
                } else {
                    (false, 0)
                }
            }
            DefaultLayer(u, _) => (self.users.is_op(*u), 0),
            Filtered(_, _) => (false, 0),
        }
    }

    fn handle_command(&mut self, message: &CommandMessage) -> bool {
        // General and user specific locks apply to all command messages
        if self.users.all_locked || is_userbit(&self.users.locked, message.user()) {
            return false;
        }

        use CommandMessage::*;
        match message {
            UndoPoint(_) => true,
            CanvasResize(u, _) => self.users.tier(*u) <= self.feature_tier.resize,
            LayerCreate(u, m) => {
                if !self.users.is_op(*u) && layer_creator(m.id) != *u {
                    // enforce layer ID prefixing scheme for non-ops
                    return false;
                }
                let tier = self.users.tier(*u);
                tier <= self.feature_tier.edit_layers || tier <= self.feature_tier.own_layers
            }
            LayerAttributes(u, m) => self.check_layer_perms(*u, m.id),
            LayerRetitle(u, m) => self.check_layer_perms(*u, m.id),
            LayerOrder(u, m) => self.check_layer_perms(*u, m.root),
            LayerDelete(u, m) => {
                let mut ok = self.check_layer_perms(*u, m.id);
                if m.merge_to > 0 {
                    ok = ok && !self.is_layer_locked(*u, m.merge_to);
                }
                if ok {
                    self.layers.remove(&m.id);
                }
                ok
            }
            LayerVisibility(_, _) => false, // internal use only
            PutImage(u, m) => {
                self.users.tier(*u) <= self.feature_tier.put_image
                    && !self.is_layer_locked(*u, m.layer)
            }
            FillRect(u, m) => {
                self.users.tier(*u) <= self.feature_tier.put_image
                    && !self.is_layer_locked(*u, m.layer)
            }
            PenUp(_) => true,
            AnnotationCreate(u, m) => {
                self.users.tier(*u) <= self.feature_tier.create_annotation
                    && (self.users.is_op(*u) || layer_creator(m.id) == *u)
            }
            AnnotationReshape(u, m) => {
                self.users.is_op(*u)
                    || *u == layer_creator(m.id)
                    || !self.locked_annotations.contains(&m.id)
            }
            AnnotationEdit(u, m) => {
                if self.users.is_op(*u) || *u == layer_creator(m.id) {
                    if m.flags & AnnotationEditMessage::FLAGS_PROTECT > 0 {
                        self.locked_annotations.insert(m.id);
                    } else {
                        self.locked_annotations.remove(&m.id);
                    }
                    true
                } else {
                    !self.locked_annotations.contains(&m.id)
                }
            }
            AnnotationDelete(u, id) => {
                let ok = self.users.is_op(*u)
                    || *u == layer_creator(*id)
                    || !self.locked_annotations.contains(id);
                if ok {
                    self.locked_annotations.remove(id);
                }
                ok
            }
            PutTile(u, _) => self.users.is_op(*u),
            CanvasBackground(u, _) => self.users.tier(*u) <= self.feature_tier.background,
            DrawDabsClassic(u, m) => !self.is_layer_locked(*u, m.layer),
            DrawDabsPixel(u, m) | DrawDabsPixelSquare(u, m) => !self.is_layer_locked(*u, m.layer),
            DrawDabsMyPaint(u, m) => !self.is_layer_locked(*u, m.layer),
            MoveRect(u, m) => {
                self.users.tier(*u) <= self.feature_tier.move_rect
                    && !self.is_layer_locked(*u, m.layer)
            }
            Undo(u, _) => self.users.tier(*u) <= self.feature_tier.undo,
            SetMetadataInt(u, SetMetadataIntMessage { field, .. }) => {
                // Certain metadata fields belong to specific features
                let tier = match MetadataInt::try_from(*field) {
                    Ok(MetadataInt::Framerate) | Ok(MetadataInt::UseTimeline) => {
                        self.feature_tier.timeline
                    }
                    _ => self.feature_tier.metadata,
                };

                self.users.tier(*u) <= tier
            }
            SetMetadataStr(u, _) => self.users.tier(*u) <= self.feature_tier.metadata,
            SetTimelineFrame(u, _) => self.users.tier(*u) <= self.feature_tier.timeline,
            RemoveTimelineFrame(u, _) => self.users.tier(*u) <= self.feature_tier.timeline,
        }
    }

    /// Check if this user has edit permissions on the given layer
    /// Editing here means changing properties, deleting or reordering group layers.
    fn check_layer_perms(&self, user: UserID, layer: LayerID) -> bool {
        let tier = self.users.tier(user);
        tier <= self.feature_tier.edit_layers
            || (user == layer_creator(layer) && tier <= self.feature_tier.own_layers)
    }

    /// Check if the layer is locked for the given user
    ///
    /// A layer can be off-limits because of an insufficient access tier,
    /// because exclusive access has been assigned, or because the whole layer is locked.
    /// Note! There is also a canvas-wide lock, but that is checked elsewhere as it
    /// affects all Command messages equally.
    ///
    /// If ACLs haven't been explicitly assigned to a layer, the default is
    /// Guest tier access and no locks.
    fn is_layer_locked(&self, user: UserID, layer: LayerID) -> bool {
        self.layers.get(&layer).map_or(false, |l| {
            l.locked || !is_userbit(&l.exclusive, user) || l.tier < self.users.tier(user)
        })
    }
}

impl UserACLs {
    /// Get the highest access tier for this user based on the permission bits
    fn tier(&self, user: UserID) -> Tier {
        if user == 0 || is_userbit(&self.operators, user) {
            // ID 0 is reserved for server use
            Tier::Operator
        } else if is_userbit(&self.trusted, user) {
            Tier::Trusted
        } else if is_userbit(&self.authenticated, user) {
            Tier::Authenticated
        } else {
            Tier::Guest
        }
    }
}

impl Default for AclFilter {
    fn default() -> Self {
        Self::new()
    }
}

fn set_userbit(bits: &mut UserBits, user: UserID) {
    bits[user as usize / 8] |= 1 << (user % 8);
}

fn unset_userbit(bits: &mut UserBits, user: UserID) {
    bits[user as usize / 8] &= !(1 << (user % 8));
}

fn vec_to_userbits(users: &[UserID]) -> UserBits {
    let mut bits: UserBits = [0; 32];
    for u in users {
        set_userbit(&mut bits, *u);
    }

    bits
}

fn is_userbit(bits: &UserBits, user: UserID) -> bool {
    (bits[user as usize / 8] & (1 << (user % 8))) != 0
}

fn layer_creator(id: u16) -> UserID {
    (id >> 8) as UserID
}

pub fn userbits_to_vec(users: &UserBits) -> Vec<UserID> {
    let mut v = Vec::new();

    if users.iter().all(|&b| b == 0xff) {
        return v;
    }

    for (i, &user) in users.iter().enumerate() {
        if user != 0 {
            for j in 0..8 {
                if user & (1 << j) > 0 {
                    v.push((i * 8 + j) as u8);
                }
            }
        }
    }
    v
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_layer_edit() {
        let mut acl = AclFilter::new();
        join(&mut acl, 1, 0);
        join(&mut acl, 2, 0);
        assert!(set_op(&mut acl, 0, 1));

        // Operators can always create layers
        assert!(create_layer(&mut acl, 1, 0x0101, 0));

        // Even for other users
        assert!(create_layer(&mut acl, 1, 0x0201, 0));

        // By default, guest users can also create layers
        assert!(create_layer(&mut acl, 2, 0x0202, 0));

        // But only for themselves
        assert!(!create_layer(&mut acl, 2, 0x0302, 0));
    }

    #[test]
    fn test_access_tiers() {
        let mut acl = AclFilter::new();

        let ok = acl.filter_message(&Message::ClientMeta(
            ClientMetaMessage::FeatureAccessLevels(
                0,
                vec![
                    u8::from(Tier::Operator),      // put image
                    u8::from(Tier::Trusted),       // move rect
                    u8::from(Tier::Authenticated), // resize
                    u8::from(Tier::Guest),         // background
                    u8::from(Tier::Guest),         // layer edit
                    u8::from(Tier::Guest),         // own layers
                    u8::from(Tier::Operator),      // create annotations
                    u8::from(Tier::Trusted),       // laser
                    u8::from(Tier::Authenticated), // undo
                    u8::from(Tier::Guest),         // metadata
                    u8::from(Tier::Trusted),       // timeline
                ],
            ),
        ));
        assert!(ok.0);
        assert_eq!(ok.1, ACLCHANGE_FEATURES);

        join(&mut acl, 1, 0); // this will become the operator
        join(&mut acl, 2, 0); // this will become trusted
        join(&mut acl, 3, JoinMessage::FLAGS_AUTH); // authenticated user
        join(&mut acl, 4, 0); // guest user

        let ok = acl.filter_message(&Message::ServerMeta(ServerMetaMessage::SessionOwner(
            0,
            vec![1],
        )));
        assert!(ok.0);
        assert_eq!(ok.1, ACLCHANGE_USERBITS);

        let ok = acl.filter_message(&Message::ServerMeta(ServerMetaMessage::TrustedUsers(
            0,
            vec![2],
        )));
        assert!(ok.0);
        assert_eq!(ok.1, ACLCHANGE_USERBITS);

        // ACL filter is now set up and ready for testing

        // Check that each user got their tiers assigned properly
        assert_eq!(acl.users().tier(1), Tier::Operator);
        assert_eq!(acl.users().tier(2), Tier::Trusted);
        assert_eq!(acl.users().tier(3), Tier::Authenticated);
        assert_eq!(acl.users().tier(4), Tier::Guest);

        // Create annotations: operator tier
        let (ok, _) = acl.filter_message(&Message::Command(CommandMessage::AnnotationCreate(
            1,
            AnnotationCreateMessage {
                id: 0x0101,
                x: 1,
                y: 1,
                w: 1,
                h: 1,
            },
        )));
        assert!(ok);

        let (ok, _) = acl.filter_message(&Message::Command(CommandMessage::AnnotationCreate(
            2,
            AnnotationCreateMessage {
                id: 0x0201,
                x: 1,
                y: 1,
                w: 1,
                h: 1,
            },
        )));
        assert!(!ok);

        let (ok, _) = acl.filter_message(&Message::Command(CommandMessage::AnnotationCreate(
            3,
            AnnotationCreateMessage {
                id: 0x0301,
                x: 1,
                y: 1,
                w: 1,
                h: 1,
            },
        )));
        assert!(!ok);

        let (ok, _) = acl.filter_message(&Message::Command(CommandMessage::AnnotationCreate(
            4,
            AnnotationCreateMessage {
                id: 0x0401,
                x: 1,
                y: 1,
                w: 1,
                h: 1,
            },
        )));
        assert!(!ok);

        // User laser pointer: Trusted tier
        let (ok, _) = acl.filter_message(&Message::ClientMeta(ClientMetaMessage::LaserTrail(
            1,
            LaserTrailMessage {
                color: 1,
                persistence: 1,
            },
        )));
        assert!(ok);

        let (ok, _) = acl.filter_message(&Message::ClientMeta(ClientMetaMessage::LaserTrail(
            2,
            LaserTrailMessage {
                color: 1,
                persistence: 1,
            },
        )));
        assert!(ok);

        let (ok, _) = acl.filter_message(&Message::ClientMeta(ClientMetaMessage::LaserTrail(
            3,
            LaserTrailMessage {
                color: 1,
                persistence: 1,
            },
        )));
        assert!(!ok);

        let (ok, _) = acl.filter_message(&Message::ClientMeta(ClientMetaMessage::LaserTrail(
            4,
            LaserTrailMessage {
                color: 1,
                persistence: 1,
            },
        )));
        assert!(!ok);
    }

    #[test]
    fn test_layer_locks() {
        let mut acl = AclFilter::new();

        // Add users
        join(&mut acl, 1, 0); // this will become the operator
        join(&mut acl, 2, 0); // this will become trusted
        join(&mut acl, 3, JoinMessage::FLAGS_AUTH); // authenticated user
        join(&mut acl, 4, 0); // guest user

        let ok = acl.filter_message(&Message::ServerMeta(ServerMetaMessage::SessionOwner(
            0,
            vec![1],
        )));
        assert_eq!(ok, (true, ACLCHANGE_USERBITS));

        let ok = acl.filter_message(&Message::ServerMeta(ServerMetaMessage::TrustedUsers(
            0,
            vec![2],
        )));
        assert_eq!(ok, (true, ACLCHANGE_USERBITS));

        // Create tiered layers
        let ok = acl.filter_message(&Message::ClientMeta(ClientMetaMessage::LayerACL(
            0,
            LayerACLMessage {
                id: 1,
                flags: u8::from(Tier::Operator),
                exclusive: vec![],
            },
        )));
        assert_eq!(ok, (true, ACLCHANGE_LAYERS));

        let ok = acl.filter_message(&Message::ClientMeta(ClientMetaMessage::LayerACL(
            0,
            LayerACLMessage {
                id: 2,
                flags: u8::from(Tier::Trusted),
                exclusive: vec![],
            },
        )));
        assert_eq!(ok, (true, ACLCHANGE_LAYERS));

        let ok = acl.filter_message(&Message::ClientMeta(ClientMetaMessage::LayerACL(
            0,
            LayerACLMessage {
                id: 3,
                flags: u8::from(Tier::Authenticated),
                exclusive: vec![],
            },
        )));
        assert_eq!(ok, (true, ACLCHANGE_LAYERS));

        let ok = acl.filter_message(&Message::ClientMeta(ClientMetaMessage::LayerACL(
            0,
            LayerACLMessage {
                id: 4,
                flags: u8::from(Tier::Guest),
                exclusive: vec![],
            },
        )));
        assert_eq!(ok, (true, 0)); // no change: guest tier + no locks is the default

        let ok = acl.filter_message(&Message::ClientMeta(ClientMetaMessage::LayerACL(
            0,
            LayerACLMessage {
                id: 5,
                flags: u8::from(Tier::Guest),
                exclusive: vec![4], // exclusive to user 4
            },
        )));
        assert_eq!(ok, (true, ACLCHANGE_LAYERS));

        let ok = acl.filter_message(&Message::ClientMeta(ClientMetaMessage::LayerACL(
            0,
            LayerACLMessage {
                id: 6,
                flags: u8::from(Tier::Guest) | 0x80, // general layer lock
                exclusive: vec![],
            },
        )));
        assert_eq!(ok, (true, ACLCHANGE_LAYERS));

        // ACL filter is now set up and ready for testing

        // owner should be able to drawn on any layer, except the ones explicitly locked
        assert!(fillrect(&mut acl, 1, 1));
        assert!(fillrect(&mut acl, 1, 2));
        assert!(fillrect(&mut acl, 1, 3));
        assert!(fillrect(&mut acl, 1, 4));
        assert!(!fillrect(&mut acl, 1, 5)); // user 4's exclusive
        assert!(!fillrect(&mut acl, 1, 6)); // locked

        // trusted user
        assert!(!fillrect(&mut acl, 2, 1));
        assert!(fillrect(&mut acl, 2, 2));
        assert!(fillrect(&mut acl, 2, 3));
        assert!(fillrect(&mut acl, 2, 4));
        assert!(!fillrect(&mut acl, 2, 5)); // user 4's exclusive
        assert!(!fillrect(&mut acl, 2, 6)); // locked

        // authenticated user
        assert!(!fillrect(&mut acl, 3, 1));
        assert!(!fillrect(&mut acl, 3, 2));
        assert!(fillrect(&mut acl, 3, 3));
        assert!(fillrect(&mut acl, 3, 4));
        assert!(!fillrect(&mut acl, 3, 5)); // user 4's exclusive
        assert!(!fillrect(&mut acl, 3, 6)); // locked

        // guest user
        assert!(!fillrect(&mut acl, 4, 1));
        assert!(!fillrect(&mut acl, 4, 2));
        assert!(!fillrect(&mut acl, 4, 3));
        assert!(fillrect(&mut acl, 4, 4));
        assert!(fillrect(&mut acl, 4, 5)); // user 4's exclusive
        assert!(!fillrect(&mut acl, 4, 6)); // locked
    }

    fn join(acl: &mut AclFilter, user: UserID, flags: u8) {
        let (ok, _) = acl.filter_message(&Message::ServerMeta(ServerMetaMessage::Join(
            user,
            JoinMessage {
                flags,
                name: String::new(),
                avatar: Vec::new(),
            },
        )));

        assert!(ok);
    }

    fn set_op(acl: &mut AclFilter, by_user: UserID, user: UserID) -> bool {
        let (ok, _) = acl.filter_message(&Message::ServerMeta(ServerMetaMessage::SessionOwner(
            by_user,
            vec![user],
        )));

        ok
    }

    fn create_layer(acl: &mut AclFilter, user: UserID, layer: LayerID, target: LayerID) -> bool {
        let (ok, _) = acl.filter_message(&Message::Command(CommandMessage::LayerCreate(
            user,
            LayerCreateMessage {
                id: layer,
                source: 0,
                target,
                fill: 0,
                flags: 0,
                name: String::new(),
            },
        )));

        ok
    }

    fn fillrect(acl: &mut AclFilter, user: UserID, layer: LayerID) -> bool {
        let (ok, bits) = acl.filter_message(&Message::Command(CommandMessage::FillRect(
            user,
            FillRectMessage {
                layer,
                mode: 0,
                x: 0,
                y: 0,
                w: 1,
                h: 1,
                color: 0,
            },
        )));

        assert_eq!(bits, 0);
        ok
    }
}
