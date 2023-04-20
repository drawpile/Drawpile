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
#include "acl.h"
#include "message.h"
#include "msg_internal.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/output.h>
#include <uthash_inc.h>


typedef struct DP_LayerAclEntry {
    int layer_id;
    DP_LayerAcl layer_acl;
    UT_hash_handle hh;
} DP_LayerAclEntry;

typedef struct DP_AnnotationAclEntry {
    int annotation_id;
    UT_hash_handle hh;
} DP_AnnotationAclEntry;

typedef struct DP_AclState {
    DP_UserAcls users;
    DP_LayerAclEntry *layers;
    DP_AnnotationAclEntry *annotations;
    DP_FeatureTiers feature;
} DP_AclState;

typedef struct DP_AccessTierAttributes {
    const char *enum_name;
    const char *name;
} DP_AccessTierAttributes;

typedef struct DP_FeatureAttributes {
    const char *enum_name;
    const char *name;
} DP_FeatureAttributes;

static const DP_AccessTierAttributes access_tier_attributes[] = {
    [DP_ACCESS_TIER_OPERATOR] = {"DP_ACCESS_TIER_OPERATOR", "op"},
    [DP_ACCESS_TIER_TRUSTED] = {"DP_ACCESS_TIER_TRUSTED", "trusted"},
    [DP_ACCESS_TIER_AUTHENTICATED] = {"DP_ACCESS_TIER_AUTHENTICATED ", "auth"},
    [DP_ACCESS_TIER_GUEST] = {"DP_ACCESS_TIER_GUEST", "guest"},
};

static DP_FeatureAttributes feature_attributes[] = {
    [DP_FEATURE_PUT_IMAGE] = {"DP_FEATURE_PUT_IMAGE", "put_image"},
    [DP_FEATURE_REGION_MOVE] = {"DP_FEATURE_REGION_MOVE", "region_move"},
    [DP_FEATURE_RESIZE] = {"DP_FEATURE_RESIZE", "resize"},
    [DP_FEATURE_BACKGROUND] = {"DP_FEATURE_BACKGROUND", "background"},
    [DP_FEATURE_EDIT_LAYERS] = {"DP_FEATURE_EDIT_LAYERS", "edit_layers"},
    [DP_FEATURE_OWN_LAYERS] = {"DP_FEATURE_OWN_LAYERS", "own_layers"},
    [DP_FEATURE_CREATE_ANNOTATION] = {"DP_FEATURE_CREATE_ANNOTATION",
                                      "create_annotation"},
    [DP_FEATURE_LASER] = {"DP_FEATURE_LASER", "laser"},
    [DP_FEATURE_UNDO] = {"DP_FEATURE_UNDO", "undo"},
    [DP_FEATURE_METADATA] = {"DP_FEATURE_METADATA", "metadata"},
    [DP_FEATURE_TIMELINE] = {"DP_FEATURE_TIMELINE", "timeline"},
    [DP_FEATURE_MYPAINT] = {"DP_FEATURE_MYPAINT", "mypaint"},
};

int DP_access_tier_clamp(int tier)
{
    if (tier < 0) {
        return 0;
    }
    else if (tier >= DP_ACCESS_TIER_COUNT) {
        return DP_ACCESS_TIER_COUNT - 1;
    }
    else {
        return tier;
    }
}

static const DP_AccessTierAttributes *access_tier_at(int tier)
{
    if (tier >= 0 && tier < DP_ACCESS_TIER_COUNT) {
        return &access_tier_attributes[tier];
    }
    else {
        DP_error_set("Unknown access tier: %d", tier);
        return NULL;
    }
}

const char *DP_access_tier_enum_name(int tier)
{
    const DP_AccessTierAttributes *attributes = access_tier_at(tier);
    return attributes ? attributes->enum_name : NULL;
}

const char *DP_access_tier_name(int tier)
{
    const DP_AccessTierAttributes *attributes = access_tier_at(tier);
    return attributes ? attributes->name : NULL;
}


static const DP_FeatureAttributes *feature_at(int feature)
{
    if (feature >= 0 && feature < DP_FEATURE_COUNT) {
        return &feature_attributes[feature];
    }
    else {
        DP_error_set("Unknown feature: %d", feature);
        return NULL;
    }
}

const char *DP_feature_enum_name(int feature)
{
    const DP_FeatureAttributes *attributes = feature_at(feature);
    return attributes ? attributes->enum_name : NULL;
}


static uint8_t user_id_index(uint8_t user_id)
{
    return user_id / 8;
}

static uint8_t user_id_mask(uint8_t user_id)
{
    return DP_int_to_uint8(1 << (user_id % 8));
}

bool DP_user_bit_get(const uint8_t *users, uint8_t user_id)
{
    DP_ASSERT(users);
    return (users[user_id_index(user_id)] & user_id_mask(user_id)) != 0;
}

void DP_user_bit_set(uint8_t *users, uint8_t user_id)
{
    DP_ASSERT(users);
    users[user_id_index(user_id)] |= user_id_mask(user_id);
}

void DP_user_bit_unset(uint8_t *users, uint8_t user_id)
{
    DP_ASSERT(users);
    users[user_id_index(user_id)] &= (uint8_t)~user_id_mask(user_id);
}

void DP_user_bits_set(uint8_t *users, int count, const uint8_t *user_ids)
{
    DP_ASSERT(users);
    DP_ASSERT(user_ids);
    for (int i = 0; i < count; ++i) {
        DP_user_bit_set(users, user_ids[i]);
    }
}

void DP_user_bits_unset(uint8_t *users, int count, const uint8_t *user_ids)
{
    DP_ASSERT(users);
    DP_ASSERT(user_ids);
    for (int i = 0; i < count; ++i) {
        DP_user_bit_unset(users, user_ids[i]);
    }
}

static void memset_userbits(uint8_t *users, uint8_t value)
{
    memset(users, value, sizeof(DP_UserBits));
}

void DP_user_bits_replace(uint8_t *users, int count, const uint8_t *user_ids)
{
    DP_ASSERT(users);
    DP_ASSERT(user_ids);
    memset_userbits(users, 0);
    DP_user_bits_set(users, count, user_ids);
}


bool DP_user_acls_is_op(const DP_UserAcls *users, uint8_t user_id)
{
    // User id 0 represents the server itself, which can do anything.
    return user_id == 0 || DP_user_bit_get(users->operators, user_id);
}

bool DP_user_acls_is_trusted(const DP_UserAcls *users, uint8_t user_id)
{
    return DP_user_bit_get(users->trusted, user_id);
}

bool DP_user_acls_is_authenticated(const DP_UserAcls *users, uint8_t user_id)
{
    return DP_user_bit_get(users->authenticated, user_id);
}

bool DP_user_acls_is_locked(const DP_UserAcls *users, uint8_t user_id)
{
    return DP_user_bit_get(users->locked, user_id);
}

DP_AccessTier DP_user_acls_tier(const DP_UserAcls *users, uint8_t user_id)
{
    DP_ASSERT(users);
    if (DP_user_acls_is_op(users, user_id)) {
        return DP_ACCESS_TIER_OPERATOR;
    }
    else if (DP_user_acls_is_trusted(users, user_id)) {
        return DP_ACCESS_TIER_TRUSTED;
    }
    else if (DP_user_acls_is_authenticated(users, user_id)) {
        return DP_ACCESS_TIER_AUTHENTICATED;
    }
    else {
        return DP_ACCESS_TIER_GUEST;
    }
}


static DP_FeatureTiers null_feature_tiers(void)
{
    return (DP_FeatureTiers){{
        DP_ACCESS_TIER_GUEST,
        DP_ACCESS_TIER_GUEST,
        DP_ACCESS_TIER_OPERATOR,
        DP_ACCESS_TIER_OPERATOR,
        DP_ACCESS_TIER_OPERATOR,
        DP_ACCESS_TIER_GUEST,
        DP_ACCESS_TIER_GUEST,
        DP_ACCESS_TIER_GUEST,
        DP_ACCESS_TIER_GUEST,
        DP_ACCESS_TIER_OPERATOR,
        DP_ACCESS_TIER_GUEST,
        DP_ACCESS_TIER_GUEST,
    }};
}

static DP_AclState null_acl_state(void)
{
    return (DP_AclState){
        {{0}, {0}, {0}, {0}, false}, NULL, NULL, null_feature_tiers()};
}

DP_AclState *DP_acl_state_new(void)
{
    DP_AclState *acls = DP_malloc(sizeof(*acls));
    *acls = null_acl_state();
    return acls;
}

static void clear_layers(DP_AclState *acls)
{
    DP_LayerAclEntry *entry, *tmp;
    HASH_ITER(hh, acls->layers, entry, tmp) {
        HASH_DEL(acls->layers, entry);
        DP_free(entry);
    }
}

static void clear_annotations(DP_AclState *acls)
{
    DP_AnnotationAclEntry *entry, *tmp;
    HASH_ITER(hh, acls->annotations, entry, tmp) {
        HASH_DEL(acls->annotations, entry);
        DP_free(entry);
    }
}

void DP_acl_state_free(DP_AclState *acls)
{
    if (acls) {
        clear_layers(acls);
        clear_annotations(acls);
        DP_free(acls);
    }
}

void DP_acl_state_reset(DP_AclState *acls, uint8_t local_user_id)
{
    DP_ASSERT(acls);
    clear_layers(acls);
    clear_annotations(acls);
    *acls = null_acl_state();
    if (local_user_id != 0) {
        DP_user_bit_set(acls->users.operators, local_user_id);
    }
}

static void dump_user_bits(DP_Output *output, const char *title,
                           const uint8_t *users)
{
    if (title) {
        DP_output_format(output, "    %s: ", title);
    }
    bool first = true;
    for (int i = 0; i < 256; ++i) {
        if (DP_user_bit_get(users, DP_int_to_uint8(i))) {
            DP_output_format(output, first ? "%d" : ", %d", i);
            first = false;
        }
    }
    DP_output_print(output, first ? "(none)\n" : "\n");
}

static void dump_layer_acls(DP_Output *output, DP_AclState *acls)
{
    if (acls->layers) {
        DP_OUTPUT_PRINT_LITERAL(output, "    layers:\n");
        DP_LayerAclEntry *entry, *tmp;
        HASH_ITER(hh, acls->layers, entry, tmp) {
            DP_LayerAcl *la = &entry->layer_acl;
            DP_output_format(
                output, "        layer_id %d, locked %d, tier %s, exclusive: ",
                entry->layer_id, la->locked ? 1 : 0,
                access_tier_attributes[la->tier].name);
            dump_user_bits(output, NULL, la->exclusive);
        }
    }
    else {
        DP_OUTPUT_PRINT_LITERAL(output, "    layers: (none)\n");
    }
}

static void dump_annotation_acls(DP_Output *output, DP_AclState *acls)
{
    DP_OUTPUT_PRINT_LITERAL(output, "    annotations: ");
    bool first = true;
    DP_AnnotationAclEntry *entry, *tmp;
    HASH_ITER(hh, acls->annotations, entry, tmp) {
        DP_output_format(output, first ? "%d" : ", %d", entry->annotation_id);
    }
    DP_output_print(output, first ? "(none)\n" : "\n");
}

char *DP_acl_state_dump(DP_AclState *acls)
{
    DP_ASSERT(acls);
    void **buffer_ptr;
    size_t *size_ptr;
    DP_Output *output = DP_mem_output_new(1024, false, &buffer_ptr, &size_ptr);

    DP_output_format(output, "acl state %p\n", (void *)acls);
    DP_output_format(output, "    all_locked: %s\n",
                     acls->users.all_locked ? "true" : "false");

    dump_user_bits(output, "operators", acls->users.operators);
    dump_user_bits(output, "trusted", acls->users.trusted);
    dump_user_bits(output, "authenticated", acls->users.authenticated);
    dump_user_bits(output, "locked", acls->users.locked);

    DP_OUTPUT_PRINT_LITERAL(output, "    features:\n");
    for (int i = 0; i < DP_FEATURE_COUNT; ++i) {
        DP_output_format(output, "        %s: %s\n",
                         feature_attributes[i].enum_name,
                         access_tier_attributes[acls->feature.tiers[i]].name);
    }

    dump_layer_acls(output, acls);
    dump_annotation_acls(output, acls);

    char *buffer = *buffer_ptr;
    DP_output_free(output);
    return buffer;
}

DP_UserAcls DP_acl_state_users(DP_AclState *acls)
{
    DP_ASSERT(acls);
    return acls->users;
}

DP_FeatureTiers DP_acl_state_feature_tiers(DP_AclState *acls)
{
    DP_ASSERT(acls);
    return acls->feature;
}

void DP_acl_state_layers_each(DP_AclState *acls, DP_AclStateLayerFn fn,
                              void *user)
{
    DP_ASSERT(acls);
    DP_LayerAclEntry *entry, *tmp;
    HASH_ITER(hh, acls->layers, entry, tmp) {
        fn(user, entry->layer_id, &entry->layer_acl);
    }
}

bool DP_acl_state_is_op(DP_AclState *acls, uint8_t user_id)
{
    DP_ASSERT(acls);
    return DP_user_acls_is_op(&acls->users, user_id);
}

DP_AccessTier DP_acl_state_user_tier(DP_AclState *acls, uint8_t user_id)
{
    DP_ASSERT(acls);
    return DP_user_acls_tier(&acls->users, user_id);
}

bool DP_acl_state_can_use_feature(DP_AclState *acls, DP_Feature feature,
                                  uint8_t user_id)
{
    DP_ASSERT(acls);
    DP_ASSERT(feature >= 0);
    DP_ASSERT(feature < DP_FEATURE_COUNT);
    DP_AccessTier feature_tier = acls->feature.tiers[feature];
    return feature_tier == DP_ACCESS_TIER_GUEST
        || DP_acl_state_user_tier(acls, user_id) <= feature_tier;
}

bool DP_acl_state_layer_locked_for(DP_AclState *acls, uint8_t user_id,
                                   int layer_id)
{
    DP_ASSERT(acls);
    DP_LayerAclEntry *entry;
    HASH_FIND_INT(acls->layers, &layer_id, entry);
    if (entry) {
        DP_LayerAcl *l = &entry->layer_acl;
        return l->locked || !DP_user_bit_get(l->exclusive, user_id)
            || l->tier < DP_acl_state_user_tier(acls, user_id);
    }
    else {
        return false;
    }
}

bool DP_acl_state_annotation_locked(DP_AclState *acls, int annotation_id)
{
    DP_ASSERT(acls);
    DP_AnnotationAclEntry *entry;
    HASH_FIND_INT(acls->annotations, &annotation_id, entry);
    return entry != NULL;
}


static uint8_t message_user_id(DP_Message *msg)
{
    return DP_uint_to_uint8(DP_message_context_id(msg));
}

static uint8_t filter_unless(bool condition)
{
    return condition ? 0 : DP_ACL_STATE_FILTERED_BIT;
}

static bool owns_id(uint8_t user_id, int id)
{
    return (id >> 8) == user_id;
}

static uint8_t handle_join(DP_AclState *acls, DP_Message *msg)
{
    DP_MsgJoin *mj = DP_msg_join_cast(msg);
    if (DP_msg_join_flags(mj) | DP_MSG_JOIN_FLAGS_AUTH) {
        DP_user_bit_set(acls->users.authenticated, message_user_id(msg));
        return DP_ACL_STATE_CHANGE_USERS_BIT;
    }
    else {
        return 0;
    }
}

static uint8_t handle_leave(DP_AclState *acls, DP_Message *msg)
{
    uint8_t user_id = message_user_id(msg);
    DP_user_bit_unset(acls->users.operators, user_id);
    DP_user_bit_unset(acls->users.trusted, user_id);
    DP_user_bit_unset(acls->users.authenticated, user_id);
    DP_user_bit_unset(acls->users.locked, user_id);
    // TODO remove layer locks
    return DP_ACL_STATE_CHANGE_USERS_BIT;
}

static uint8_t handle_session_owner(DP_AclState *acls, DP_Message *msg)
{
    DP_MsgSessionOwner *mso = DP_msg_session_owner_cast(msg);
    int count;
    const uint8_t *user_ids = DP_msg_session_owner_users(mso, &count);
    DP_user_bits_replace(acls->users.operators, count, user_ids);
    return DP_ACL_STATE_CHANGE_USERS_BIT;
}

static uint8_t handle_trusted_users(DP_AclState *acls, DP_Message *msg)
{
    DP_MsgTrustedUsers *mtu = DP_msg_trusted_users_cast(msg);
    int count;
    const uint8_t *user_ids = DP_msg_trusted_users_users(mtu, &count);
    DP_user_bits_replace(acls->users.trusted, count, user_ids);
    return DP_ACL_STATE_CHANGE_USERS_BIT;
}

static uint8_t handle_internal(DP_AclState *acls, DP_Message *msg)
{
    DP_MsgInternal *mi = DP_message_internal(msg);
    DP_MsgInternalType type = DP_msg_internal_type(mi);
    bool is_reset = type == DP_MSG_INTERNAL_TYPE_RESET
                 || type == DP_MSG_INTERNAL_TYPE_RESET_TO_STATE;
    if (is_reset) {
        clear_layers(acls);
        clear_annotations(acls);
        acls->users.all_locked = false;
        memset_userbits(acls->users.locked, 0);
        acls->feature = null_feature_tiers();
        return DP_ACL_STATE_CHANGE_MASK;
    }
    else {
        return 0;
    }
}

static uint8_t handle_user_acl(DP_AclState *acls, DP_Message *msg,
                               bool override)
{
    if (override || DP_acl_state_is_op(acls, message_user_id(msg))) {
        DP_MsgUserAcl *mua = DP_msg_user_acl_cast(msg);
        int count;
        const uint8_t *user_ids = DP_msg_user_acl_users(mua, &count);
        DP_user_bits_replace(acls->users.locked, count, user_ids);
        return DP_ACL_STATE_CHANGE_USERS_BIT;
    }
    else {
        return DP_ACL_STATE_FILTERED_BIT;
    }
}

static bool can_edit_layer(DP_AclState *acls, uint8_t user_id, int layer_id)
{
    return DP_acl_state_can_use_feature(acls, DP_FEATURE_EDIT_LAYERS, user_id)
        || (DP_acl_state_can_use_feature(acls, DP_FEATURE_OWN_LAYERS, user_id)
            && owns_id(user_id, layer_id));
}

static void set_layer_acl(DP_AclState *acls, int layer_id,
                          DP_LayerAclEntry *entry, uint8_t flags,
                          int exclusive_count, const uint8_t *exclusive)
{
    if (!entry) {
        entry = DP_malloc(sizeof(*entry));
        entry->layer_id = layer_id;
        HASH_ADD_INT(acls->layers, layer_id, entry);
    }

    DP_LayerAcl *l = &entry->layer_acl;
    l->locked = flags & DP_ACL_ALL_LOCKED_BIT;
    l->tier = DP_min_uint8(flags & DP_ACCESS_TIER_MASK, DP_ACCESS_TIER_GUEST);

    // If no exclusive user ids are given, all users are allowed to use this.
    // To lock it for all users, the locked flag needs to be set instead.
    if (exclusive_count == 0) {
        memset_userbits(l->exclusive, 0xff);
    }
    else {
        DP_user_bits_replace(l->exclusive, exclusive_count, exclusive);
    }
}

static uint8_t handle_layer_acl_session_lock(DP_AclState *acls,
                                             DP_MsgLayerAcl *mla,
                                             uint8_t user_id, bool override)
{
    if (override || DP_acl_state_is_op(acls, user_id)) {
        uint8_t flags = DP_msg_layer_acl_flags(mla);
        bool lock = flags & DP_ACL_ALL_LOCKED_BIT;
        if (acls->users.all_locked == lock) {
            return 0;
        }
        else {
            acls->users.all_locked = lock;
            return DP_ACL_STATE_CHANGE_USERS_BIT;
        }
    }
    else {
        return DP_ACL_STATE_FILTERED_BIT;
    }
}

static uint8_t handle_layer_acl_layer(DP_AclState *acls, DP_MsgLayerAcl *mla,
                                      uint8_t user_id, int layer_id,
                                      bool override)
{
    if (override || can_edit_layer(acls, user_id, layer_id)) {
        uint8_t flags = DP_msg_layer_acl_flags(mla);
        int exclusive_count;
        const uint8_t *exclusive =
            DP_msg_layer_acl_exclusive(mla, &exclusive_count);

        DP_LayerAclEntry *entry;
        HASH_FIND_INT(acls->layers, &layer_id, entry);

        if (flags == DP_ACCESS_TIER_GUEST && exclusive_count == 0) {
            if (entry) {
                HASH_DEL(acls->layers, entry);
                DP_free(entry);
                return DP_ACL_STATE_CHANGE_LAYERS_BIT;
            }
            else {
                return 0;
            }
        }
        else {
            set_layer_acl(acls, layer_id, entry, flags, exclusive_count,
                          exclusive);
            return DP_ACL_STATE_CHANGE_LAYERS_BIT;
        }
    }
    else {
        return DP_ACL_STATE_FILTERED_BIT;
    }
}

static uint8_t handle_layer_acl(DP_AclState *acls, DP_Message *msg,
                                bool override)
{
    uint8_t user_id = message_user_id(msg);
    DP_MsgLayerAcl *mla = DP_msg_layer_acl_cast(msg);
    int layer_id = DP_msg_layer_acl_id(mla);
    // Special case: layer 0 means lock or unlock the whole session.
    if (layer_id == 0) {
        return handle_layer_acl_session_lock(acls, mla, user_id, override);
    }
    else {
        return handle_layer_acl_layer(acls, mla, user_id, layer_id, override);
    }
}

static uint8_t handle_feature_access_levels(DP_AclState *acls, DP_Message *msg,
                                            bool override)
{
    if (override || DP_acl_state_is_op(acls, message_user_id(msg))) {
        DP_MsgFeatureAccessLevels *mfal =
            DP_msg_feature_access_levels_cast(msg);
        int feature_tiers_count;
        const uint8_t *feature_tiers =
            DP_msg_feature_access_levels_feature_tiers(mfal,
                                                       &feature_tiers_count);

        int count = DP_min_int(feature_tiers_count, DP_FEATURE_COUNT);
        for (int i = 0; i < count; ++i) {
            uint8_t feature_tier = feature_tiers[i];
            if (feature_tier != 255) {
                acls->feature.tiers[i] =
                    DP_min_uint8(feature_tier, DP_ACCESS_TIER_GUEST);
            }
        }

        return DP_ACL_STATE_CHANGE_FEATURE_TIERS_BIT;
    }
    else {
        return DP_ACL_STATE_FILTERED_BIT;
    }
}

static bool handle_layer_create(DP_AclState *acls, int layer_id,
                                uint8_t user_id, bool override)
{
    if (override) {
        return true;
    }
    // Only operators can create layers under a different owner.
    bool can_create =
        (owns_id(user_id, layer_id) || DP_acl_state_is_op(acls, user_id))
        && (DP_acl_state_can_use_feature(acls, DP_FEATURE_EDIT_LAYERS, user_id)
            || DP_acl_state_can_use_feature(acls, DP_FEATURE_OWN_LAYERS,
                                            user_id));
    return can_create;
}

static bool can_delete_layer(DP_AclState *acls, uint8_t user_id, int layer_id,
                             int merge_id)
{
    return can_edit_layer(acls, user_id, layer_id)
        && (merge_id == 0
            || !DP_acl_state_layer_locked_for(acls, user_id, merge_id));
}

static bool handle_layer_delete(DP_AclState *acls, int layer_id, int merge_id,
                                uint8_t user_id, bool override)
{
    if (override || can_delete_layer(acls, user_id, layer_id, merge_id)) {
        DP_LayerAclEntry *entry;
        HASH_FIND_INT(acls->layers, &layer_id, entry);
        if (entry) {
            HASH_DEL(acls->layers, entry);
            DP_free(entry);
        }
        return true; // Layer is gone, so no need to report a change for it.
    }
    else {
        return false;
    }
}

static bool handle_annotation_create(DP_AclState *acls, DP_Message *msg,
                                     uint8_t user_id, bool override)
{
    if (override) {
        return true;
    }
    DP_MsgAnnotationCreate *mac = DP_msg_annotation_create_cast(msg);
    int annotation_id = DP_msg_annotation_create_id(mac);
    return DP_acl_state_can_use_feature(acls, DP_FEATURE_CREATE_ANNOTATION,
                                        user_id)
        && (owns_id(user_id, annotation_id)
            || DP_acl_state_is_op(acls, user_id));
}

static bool handle_annotation_reshape(DP_AclState *acls, DP_Message *msg,
                                      uint8_t user_id, bool override)
{
    if (override) {
        return true;
    }
    DP_MsgAnnotationReshape *mar = DP_msg_annotation_reshape_cast(msg);
    int annotation_id = DP_msg_annotation_reshape_id(mar);
    return owns_id(user_id, annotation_id) || DP_acl_state_is_op(acls, user_id)
        || !DP_acl_state_annotation_locked(acls, annotation_id);
}

static bool handle_annotation_edit(DP_AclState *acls, DP_Message *msg,
                                   uint8_t user_id, bool override)
{
    DP_MsgAnnotationEdit *mae = DP_msg_annotation_edit_cast(msg);
    int annotation_id = DP_msg_annotation_edit_id(mae);
    bool can_edit = override || owns_id(user_id, annotation_id)
                 || DP_acl_state_is_op(acls, user_id);
    if (can_edit) {
        DP_AnnotationAclEntry *entry;
        HASH_FIND_INT(acls->annotations, &annotation_id, entry);
        bool protect = DP_msg_annotation_edit_flags(mae)
                     & DP_MSG_ANNOTATION_EDIT_FLAGS_PROTECT;
        if (entry && !protect) {
            HASH_DEL(acls->annotations, entry);
            DP_free(entry);
        }
        else if (!entry && protect) {
            entry = DP_malloc(sizeof(*entry));
            entry->annotation_id = annotation_id;
            HASH_ADD_INT(acls->annotations, annotation_id, entry);
        }
        return true;
    }
    else {
        return false;
    }
}

static bool handle_annotation_delete(DP_AclState *acls, DP_Message *msg,
                                     uint8_t user_id, bool override)
{
    DP_MsgAnnotationDelete *mad = DP_msg_annotation_delete_cast(msg);
    int annotation_id = DP_msg_annotation_delete_id(mad);
    bool can_delete = override || owns_id(user_id, annotation_id)
                   || DP_acl_state_is_op(acls, user_id)
                   || !DP_acl_state_annotation_locked(acls, annotation_id);
    if (can_delete) {
        DP_AnnotationAclEntry *entry;
        HASH_FIND_INT(acls->annotations, &annotation_id, entry);
        if (entry) {
            HASH_DEL(acls->annotations, entry);
            DP_free(entry);
        }
        return true;
    }
    else {
        return false;
    }
}

static bool handle_move(DP_AclState *acls, uint8_t user_id, bool override,
                        int source_id, int target_id)
{
    if (override) {
        return true;
    }
    if (DP_acl_state_can_use_feature(acls, DP_FEATURE_REGION_MOVE, user_id)) {
        return !DP_acl_state_layer_locked_for(acls, user_id, source_id)
            && !DP_acl_state_layer_locked_for(acls, user_id, target_id);
    }
    else {
        return false;
    }
}

static bool handle_move_region(DP_AclState *acls, DP_Message *msg,
                               uint8_t user_id, bool override)
{
    DP_MsgMoveRegion *mmr = DP_message_internal(msg);
    int layer_id = DP_msg_move_region_layer(mmr);
    return handle_move(acls, user_id, override, layer_id, layer_id);
}

static bool handle_move_rect(DP_AclState *acls, DP_Message *msg,
                             uint8_t user_id, bool override)
{
    DP_MsgMoveRect *mmr = DP_message_internal(msg);
    return handle_move(acls, user_id, override, DP_msg_move_rect_source(mmr),
                       DP_msg_move_rect_layer(mmr));
}

static bool handle_transform_region(DP_AclState *acls, DP_Message *msg,
                                    uint8_t user_id, bool override)
{
    DP_MsgTransformRegion *mtr = DP_message_internal(msg);
    return handle_move(acls, user_id, override,
                       DP_msg_transform_region_source(mtr),
                       DP_msg_transform_region_layer(mtr));
}

static bool handle_set_metadata_int(DP_AclState *acls, DP_Message *msg,
                                    uint8_t user_id, bool override)
{
    if (override) {
        return true;
    }
    DP_MsgSetMetadataInt *msmi = DP_msg_set_metadata_int_cast(msg);
    DP_Feature feature;
    switch (DP_msg_set_metadata_int_field(msmi)) {
    case DP_MSG_SET_METADATA_INT_FIELD_FRAMERATE:
    case DP_MSG_SET_METADATA_INT_FIELD_FRAME_COUNT:
        feature = DP_FEATURE_TIMELINE;
        break;
    default:
        feature = DP_FEATURE_METADATA;
        break;
    }
    return DP_acl_state_can_use_feature(acls, feature, user_id);
}

static bool handle_track_create(DP_AclState *acls, int track_id,
                                uint8_t user_id, bool override)
{
    if (override) {
        return true;
    }
    // Only operators can create tracks under a different owner.
    return (owns_id(user_id, track_id) || DP_acl_state_is_op(acls, user_id))
        && DP_acl_state_can_use_feature(acls, DP_FEATURE_TIMELINE, user_id);
}

static bool handle_command_message(DP_AclState *acls, DP_Message *msg,
                                   DP_MessageType type, uint8_t user_id,
                                   bool override)
{
    switch (type) {
    case DP_MSG_CANVAS_RESIZE:
        return override
            || DP_acl_state_can_use_feature(acls, DP_FEATURE_RESIZE, user_id);
    case DP_MSG_LAYER_CREATE:
        return handle_layer_create(
            acls, DP_msg_layer_create_id(DP_msg_layer_create_cast(msg)),
            user_id, override);
    case DP_MSG_LAYER_ATTRIBUTES:
        return override
            || can_edit_layer(acls, user_id,
                              DP_msg_layer_attributes_id(
                                  DP_msg_layer_attributes_cast(msg)));
    case DP_MSG_LAYER_RETITLE:
        return override
            || can_edit_layer(
                   acls, user_id,
                   DP_msg_layer_retitle_id(DP_msg_layer_retitle_cast(msg)));
    case DP_MSG_LAYER_ORDER:
        return override
            || DP_acl_state_can_use_feature(acls, DP_FEATURE_EDIT_LAYERS,
                                            user_id);
    case DP_MSG_LAYER_DELETE: {
        DP_MsgLayerDelete *mld = DP_msg_layer_delete_cast(msg);
        return handle_layer_delete(acls, DP_msg_layer_delete_id(mld), 0,
                                   user_id, override);
    }
    case DP_MSG_LAYER_VISIBILITY:
        return false; // Layer hiding is client-side.
    case DP_MSG_PUT_IMAGE:
        return override
            || (DP_acl_state_can_use_feature(acls, DP_FEATURE_PUT_IMAGE,
                                             user_id)
                && !DP_acl_state_layer_locked_for(
                    acls, user_id,
                    DP_msg_put_image_layer(DP_msg_put_image_cast(msg))));
    case DP_MSG_FILL_RECT:
        return override
            || (DP_acl_state_can_use_feature(acls, DP_FEATURE_PUT_IMAGE,
                                             user_id)
                && !DP_acl_state_layer_locked_for(
                    acls, user_id,
                    DP_msg_fill_rect_layer(DP_msg_fill_rect_cast(msg))));
    case DP_MSG_ANNOTATION_CREATE:
        return handle_annotation_create(acls, msg, user_id, override);
    case DP_MSG_ANNOTATION_RESHAPE:
        return handle_annotation_reshape(acls, msg, user_id, override);
    case DP_MSG_ANNOTATION_EDIT:
        return handle_annotation_edit(acls, msg, user_id, override);
    case DP_MSG_ANNOTATION_DELETE:
        return handle_annotation_delete(acls, msg, user_id, override);
    case DP_MSG_MOVE_REGION:
        return handle_move_region(acls, msg, user_id, override);
    case DP_MSG_PUT_TILE:
        return override || DP_acl_state_is_op(acls, user_id);
    case DP_MSG_CANVAS_BACKGROUND:
        return override
            || DP_acl_state_can_use_feature(acls, DP_FEATURE_BACKGROUND,
                                            user_id);
    case DP_MSG_DRAW_DABS_CLASSIC:
        return override
            || !DP_acl_state_layer_locked_for(
                   acls, user_id,
                   DP_msg_draw_dabs_classic_layer(
                       DP_msg_draw_dabs_classic_cast(msg)));
    case DP_MSG_DRAW_DABS_PIXEL:
        return override
            || !DP_acl_state_layer_locked_for(
                   acls, user_id,
                   DP_msg_draw_dabs_pixel_layer(
                       DP_msg_draw_dabs_pixel_cast(msg)));
    case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
        return override
            || !DP_acl_state_layer_locked_for(
                   acls, user_id,
                   DP_msg_draw_dabs_pixel_layer(
                       DP_msg_draw_dabs_pixel_square_cast(msg)));
    case DP_MSG_DRAW_DABS_MYPAINT:
        return override
            || (DP_acl_state_can_use_feature(acls, DP_FEATURE_MYPAINT, user_id)
                && !DP_acl_state_layer_locked_for(
                    acls, user_id,
                    DP_msg_draw_dabs_mypaint_layer(
                        DP_msg_draw_dabs_mypaint_cast(msg))));
    case DP_MSG_MOVE_RECT:
        return handle_move_rect(acls, msg, user_id, override);
    case DP_MSG_SET_METADATA_INT:
        return handle_set_metadata_int(acls, msg, user_id, override);
    case DP_MSG_LAYER_TREE_CREATE:
        return handle_layer_create(
            acls,
            DP_msg_layer_tree_create_id(DP_msg_layer_tree_create_cast(msg)),
            user_id, override);
    case DP_MSG_LAYER_TREE_ORDER:
        return override
            || can_edit_layer(acls, user_id,
                              DP_msg_layer_tree_order_root(
                                  DP_msg_layer_tree_order_cast(msg)));
    case DP_MSG_LAYER_TREE_DELETE: {
        DP_MsgLayerTreeDelete *mltd = DP_msg_layer_tree_delete_cast(msg);
        return handle_layer_delete(acls, DP_msg_layer_tree_delete_id(mltd),
                                   DP_msg_layer_tree_delete_merge_to(mltd),
                                   user_id, override);
    }
    case DP_MSG_TRANSFORM_REGION:
        return handle_transform_region(acls, msg, user_id, override);
    case DP_MSG_TRACK_CREATE:
        return handle_track_create(
            acls, DP_msg_track_create_id(DP_msg_track_create_cast(msg)),
            user_id, override);
    case DP_MSG_TRACK_RETITLE:
    case DP_MSG_TRACK_DELETE:
    case DP_MSG_TRACK_ORDER:
    case DP_MSG_KEY_FRAME_SET:
    case DP_MSG_KEY_FRAME_RETITLE:
    case DP_MSG_KEY_FRAME_LAYER_ATTRIBUTES:
    case DP_MSG_KEY_FRAME_DELETE:
        return override
            || DP_acl_state_can_use_feature(acls, DP_FEATURE_TIMELINE, user_id);
    case DP_MSG_UNDO:
        return override
            || DP_acl_state_can_use_feature(acls, DP_FEATURE_UNDO, user_id);
    default:
        return true;
    }
}

uint8_t DP_acl_state_handle(DP_AclState *acls, DP_Message *msg, bool override)
{
    DP_ASSERT(acls);
    DP_ASSERT(msg);
    DP_MessageType type = DP_message_type(msg);
    // Command messages (128 and up) need common handling.
    if (type < 128) {
        switch (type) {
        case DP_MSG_JOIN:
            return handle_join(acls, msg);
        case DP_MSG_LEAVE:
            return handle_leave(acls, msg);
        case DP_MSG_SESSION_OWNER:
            return handle_session_owner(acls, msg);
        case DP_MSG_TRUSTED_USERS:
            return handle_trusted_users(acls, msg);
        case DP_MSG_INTERNAL:
            return handle_internal(acls, msg);
        case DP_MSG_LASER_TRAIL:
            return filter_unless(
                override
                || DP_acl_state_can_use_feature(acls, DP_FEATURE_LASER,
                                                message_user_id(msg)));
        case DP_MSG_USER_ACL:
            return handle_user_acl(acls, msg, override);
        case DP_MSG_LAYER_ACL:
            return handle_layer_acl(acls, msg, override);
        case DP_MSG_FEATURE_ACCESS_LEVELS:
            return handle_feature_access_levels(acls, msg, override);
        case DP_MSG_DEFAULT_LAYER:
            return filter_unless(
                override || DP_acl_state_is_op(acls, message_user_id(msg)));
        case DP_MSG_FILTERED:
            return DP_ACL_STATE_FILTERED_BIT;
        case DP_MSG_UNDO_DEPTH:
            return filter_unless(
                override || DP_acl_state_is_op(acls, message_user_id(msg)));
        case DP_MSG_LOCAL_CHANGE:
            return filter_unless(override || message_user_id(msg) == 0);
        default:
            return 0;
        }
    }
    else {
        if (override || !acls->users.all_locked) {
            uint8_t user_id = message_user_id(msg);
            if (override || !DP_user_bit_get(acls->users.locked, user_id)) {
                return filter_unless(
                    handle_command_message(acls, msg, type, user_id, override));
            }
        }
        return DP_ACL_STATE_FILTERED_BIT;
    }
}


static int count_user_bits(const uint8_t *users)
{
    int count = 0;
    for (int i = 0; i < 256; ++i) {
        if (DP_user_bit_get(users, DP_int_to_uint8(i))) {
            ++count;
        }
    }
    return count;
}

static void set_message_user_bits(DP_UNUSED int count, uint8_t *out, void *user)
{
    const uint8_t *users = user;
    int out_index = 0;
    for (int i = 0; i < 256; ++i) {
        uint8_t user_id = DP_int_to_uint8(i);
        if (DP_user_bit_get(users, user_id)) {
            out[out_index++] = user_id;
        }
    }
    DP_ASSERT(out_index == count);
}

static void set_feature_tiers(int count, uint8_t *out, void *user)
{
    DP_ASSERT(count == DP_FEATURE_COUNT);
    const DP_AccessTier *features = user;
    for (int i = 0; i < count; ++i) {
        out[i] = (uint8_t)features[i];
    }
}

DP_Message *DP_acl_state_msg_feature_access_all_new(unsigned int context_id)
{
    DP_AccessTier tiers[DP_FEATURE_COUNT];
    for (int i = 0; i < DP_FEATURE_COUNT; ++i) {
        tiers[i] = DP_ACCESS_TIER_GUEST;
    }
    return DP_msg_feature_access_levels_new(context_id, set_feature_tiers,
                                            DP_FEATURE_COUNT, tiers);
}

static bool reset_image_push_users(
    unsigned int context_id, DP_UserBits users,
    DP_Message *(*make_message)(unsigned int, void (*)(int, uint8_t *, void *),
                                int, void *),
    bool (*push_message)(void *, DP_Message *), void *user)
{
    int count = count_user_bits(users);
    DP_Message *user_acl_message =
        make_message(context_id, set_message_user_bits, count, users);
    return push_message(user, user_acl_message);
}

bool DP_acl_state_reset_image_build(DP_AclState *acls, unsigned int context_id,
                                    unsigned int include_flags,
                                    bool (*push_message)(void *, DP_Message *),
                                    void *user)
{
    DP_ASSERT(acls);
    DP_ASSERT(push_message);

    DP_LayerAclEntry *entry, *tmp;
    bool include_exclusive =
        include_flags & DP_ACL_STATE_RESET_IMAGE_INCLUDE_LAYER_ACL_EXCLUSIVE;
    HASH_ITER(hh, acls->layers, entry, tmp) {
        DP_LayerAcl *l = &entry->layer_acl;
        uint8_t flags =
            DP_uint_to_uint8(l->tier | (l->locked ? DP_ACL_ALL_LOCKED_BIT : 0));
        int exclusive_count = count_user_bits(l->exclusive);
        bool exclusive = include_exclusive && exclusive_count != 256;
        DP_Message *layer_acl_msg = DP_msg_layer_acl_new(
            context_id, DP_int_to_uint16(entry->layer_id), flags,
            exclusive ? set_message_user_bits : NULL,
            exclusive ? exclusive_count : 0, l->exclusive);
        if (!push_message(user, layer_acl_msg)) {
            return false;
        }
    }

    DP_Message *feature_access_levels_msg = DP_msg_feature_access_levels_new(
        context_id, set_feature_tiers, DP_FEATURE_COUNT, acls->feature.tiers);
    if (!push_message(user, feature_access_levels_msg)) {
        return false;
    }

    bool include_session_owner =
        include_flags & DP_ACL_STATE_RESET_IMAGE_INCLUDE_SESSION_OWNER;
    if (include_session_owner
        && !reset_image_push_users(context_id, acls->users.operators,
                                   DP_msg_session_owner_new, push_message,
                                   user)) {
        return false;
    }

    bool include_trusted_users =
        include_flags & DP_ACL_STATE_RESET_IMAGE_INCLUDE_TRUSTED_USERS;
    if (include_trusted_users
        && !reset_image_push_users(context_id, acls->users.trusted,
                                   DP_msg_trusted_users_new, push_message,
                                   user)) {
        return false;
    }

    bool include_user_acl =
        include_flags & DP_ACL_STATE_RESET_IMAGE_INCLUDE_USER_ACL;
    if (include_user_acl
        && !reset_image_push_users(context_id, acls->users.locked,
                                   DP_msg_user_acl_new, push_message, user)) {
        return false;
    }

    return true;
}
