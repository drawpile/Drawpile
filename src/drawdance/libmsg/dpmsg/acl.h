/*
 * Copyright (C) 2022 askmeaboutloom
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * This code is based on Drawpile, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/drawpile/COPYING for details.
 */
#ifndef DPMSG_ACL_H
#define DPMSG_ACL_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


#define DP_ACL_ALL_LOCKED_BIT 0x80

#define DP_ACCESS_TIER_MASK                           \
    (DP_ACCESS_TIER_OPERATOR | DP_ACCESS_TIER_TRUSTED \
     | DP_ACCESS_TIER_AUTHENTICATED | DP_ACCESS_TIER_GUEST)

#define DP_ACL_STATE_FILTERED_BIT             (1 << 0)
#define DP_ACL_STATE_CHANGE_USERS_BIT         (1 << 1)
#define DP_ACL_STATE_CHANGE_LAYERS_BIT        (1 << 2)
#define DP_ACL_STATE_CHANGE_FEATURE_TIERS_BIT (1 << 3)
#define DP_ACL_STATE_CHANGE_MASK                                    \
    (DP_ACL_STATE_CHANGE_USERS_BIT | DP_ACL_STATE_CHANGE_LAYERS_BIT \
     | DP_ACL_STATE_CHANGE_FEATURE_TIERS_BIT)

#define DP_ACL_STATE_RESET_IMAGE_INCLUDE_SESSION_OWNER       (1 << 0)
#define DP_ACL_STATE_RESET_IMAGE_INCLUDE_TRUSTED_USERS       (1 << 1)
#define DP_ACL_STATE_RESET_IMAGE_INCLUDE_USER_ACL            (1 << 2)
#define DP_ACL_STATE_RESET_IMAGE_INCLUDE_LAYER_ACL_EXCLUSIVE (1 << 3)
// Session resets include user and layer locks, but the operator and trusted
// states are retained server-side. It also includes a reset lock message.
#define DP_ACL_STATE_RESET_IMAGE_SESSION_RESET_FLAGS \
    (DP_ACL_STATE_RESET_IMAGE_INCLUDE_USER_ACL       \
     | DP_ACL_STATE_RESET_IMAGE_INCLUDE_LAYER_ACL_EXCLUSIVE)
// Recordings include all user permissions.
#define DP_ACL_STATE_RESET_IMAGE_RECORDING_FLAGS      \
    (DP_ACL_STATE_RESET_IMAGE_INCLUDE_SESSION_OWNER   \
     | DP_ACL_STATE_RESET_IMAGE_INCLUDE_TRUSTED_USERS \
     | DP_ACL_STATE_RESET_IMAGE_INCLUDE_USER_ACL      \
     | DP_ACL_STATE_RESET_IMAGE_INCLUDE_LAYER_ACL_EXCLUSIVE)
// Session templates don't include any user-related state.
#define DP_ACL_STATE_RESET_IMAGE_TEMPLATE_FLAGS 0

typedef enum DP_AccessTier {
    DP_ACCESS_TIER_OPERATOR = 0,
    DP_ACCESS_TIER_TRUSTED = 1,
    DP_ACCESS_TIER_AUTHENTICATED = 2,
    DP_ACCESS_TIER_GUEST = 3,
    DP_ACCESS_TIER_COUNT,
} DP_AccessTier;

typedef enum DP_Feature {
    DP_FEATURE_PUT_IMAGE,
    DP_FEATURE_REGION_MOVE,
    DP_FEATURE_RESIZE,
    DP_FEATURE_BACKGROUND,
    DP_FEATURE_EDIT_LAYERS,
    DP_FEATURE_OWN_LAYERS,
    DP_FEATURE_CREATE_ANNOTATION,
    DP_FEATURE_LASER,
    DP_FEATURE_UNDO,
    DP_FEATURE_METADATA,
    DP_FEATURE_TIMELINE,
    DP_FEATURE_MYPAINT,
    DP_FEATURE_COUNT,
} DP_Feature;

typedef struct DP_FeatureTiers {
    DP_AccessTier tiers[DP_FEATURE_COUNT];
} DP_FeatureTiers;

// Bitfield for storing user ids between 0 and 255. 255 / 8 = 32.
typedef uint8_t DP_UserBits[32];

typedef struct DP_UserAcls {
    DP_UserBits operators;
    DP_UserBits trusted;
    DP_UserBits authenticated;
    DP_UserBits locked;
    bool all_locked;
} DP_UserAcls;

typedef struct DP_LayerAcl {
    bool locked;
    DP_AccessTier tier;
    DP_UserBits exclusive;
} DP_LayerAcl;

typedef struct DP_AclState DP_AclState;

typedef void (*DP_AclStateLayerFn)(void *user, int layer_id,
                                   const DP_LayerAcl *l);


int DP_access_tier_clamp(int tier);

const char *DP_access_tier_enum_name(int tier);

const char *DP_access_tier_name(int tier);


const char *DP_feature_enum_name(int feature);


bool DP_user_bit_get(const uint8_t *users, uint8_t user_id);
void DP_user_bit_set(uint8_t *users, uint8_t user_id);
void DP_user_bit_unset(uint8_t *users, uint8_t user_id);
void DP_user_bits_set(uint8_t *users, int count, const uint8_t *user_ids);
void DP_user_bits_unset(uint8_t *users, int count, const uint8_t *user_ids);
void DP_user_bits_replace(uint8_t *users, int count, const uint8_t *user_ids);

bool DP_user_acls_is_op(const DP_UserAcls *users, uint8_t user_id);
bool DP_user_acls_is_trusted(const DP_UserAcls *users, uint8_t user_id);
bool DP_user_acls_is_authenticated(const DP_UserAcls *users, uint8_t user_id);
bool DP_user_acls_is_locked(const DP_UserAcls *users, uint8_t user_id);

DP_AccessTier DP_user_acls_tier(const DP_UserAcls *users, uint8_t user_id);


DP_AclState *DP_acl_state_new(void);

// Drawpile 2.1 recordings don't start out setting up permissions properly, so
// we need a special ACL state for them. It starts with all features on access
// tier "guest" and every user marked as an operator.
DP_AclState *DP_acl_state_new_playback(void);

DP_AclState *DP_acl_state_new_clone(DP_AclState *acls, uint8_t local_user_id);

void DP_acl_state_free(DP_AclState *acls);

void DP_acl_state_reset(DP_AclState *acls, uint8_t local_user_id);

// Dumps a textural description of the ACL state to a DP_malloc'd string.
char *DP_acl_state_dump(DP_AclState *acls);

uint8_t DP_acl_state_local_user_id(DP_AclState *acls);

DP_UserAcls DP_acl_state_users(DP_AclState *acls);

DP_FeatureTiers DP_acl_state_feature_tiers(DP_AclState *acls);

void DP_acl_state_layers_each(DP_AclState *acls, DP_AclStateLayerFn fn,
                              void *user);

bool DP_acl_state_is_op(DP_AclState *acls, uint8_t user_id);

DP_AccessTier DP_acl_state_user_tier(DP_AclState *acls, uint8_t user_id);

bool DP_acl_state_can_use_feature(DP_AclState *acls, DP_Feature feature,
                                  uint8_t user_id);

bool DP_acl_state_layer_locked_for(DP_AclState *acls, uint8_t user_id,
                                   int layer_id);

bool DP_acl_state_annotation_locked(DP_AclState *acls, int annotation_id);

// Returns a set of flags describing the outcome. If DP_ACL_STATE_FILTERED_BIT
// is set, the message was filtered out and should not be processed further. If
// any of the DP_ACL_STATE_CHANGE_*_BITs are set, the ACL state itself changed
// and UI elements should be updated as appropriate. If override is set,
// permissions aren't checked, the message is always processed.
uint8_t DP_acl_state_handle(DP_AclState *acls, DP_Message *msg,
                            bool override) DP_MUST_CHECK;

DP_Message *DP_acl_state_msg_feature_access_all_new(unsigned int context_id);

bool DP_acl_state_reset_image_build(DP_AclState *acls, unsigned int context_id,
                                    unsigned int include_flags,
                                    bool (*push_message)(void *, DP_Message *),
                                    void *user);


#endif
