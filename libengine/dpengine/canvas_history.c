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
#include "canvas_history.h"
#include "canvas_state.h"
#include "dpmsg/messages/undo.h"
#include <dpcommon/conversions.h>
#include <dpcommon/threading.h>
#include <dpmsg/message.h>
#include <dpmsg/messages/internal.h>
#include <dpmsg/messages/undo_point.h>


#define INITIAL_CAPACITY              1024
#define EXPAND_CAPACITY(OLD_CAPACITY) ((OLD_CAPACITY)*2)

#define UNDO_DEPTH_LIMIT 30

typedef enum DP_Undo
{
    DP_UNDO_DONE,
    DP_UNDO_UNDONE,
    DP_UNDO_GONE,
} DP_Undo;

typedef struct DP_CanvasHistoryEntry {
    DP_Undo undo;
    DP_Message *msg;
    DP_CanvasState *state;
} DP_CanvasHistoryEntry;

struct DP_CanvasHistory {
    DP_Mutex *mutex;
    DP_CanvasState *current_state;
    int capacity;
    int used;
    DP_CanvasHistoryEntry *entries;
};


static void set_initial_entry(DP_CanvasHistory *ch, DP_CanvasState *cs)
{
    ch->entries[0] = (DP_CanvasHistoryEntry){
        DP_UNDO_DONE, DP_msg_undo_point_new(0), DP_canvas_state_incref(cs)};
}

static void validate_history(DP_CanvasHistory *ch)
{
#ifdef NDEBUG
    (void)ch; // Validation only happens in debug mode.
#else
    DP_CanvasHistoryEntry *entries = ch->entries;
    int used = ch->used;
    bool have_savepoint = false;
    for (int i = 0; i < used; ++i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        DP_Message *msg = entry->msg;
        DP_ASSERT(msg); // Message must not be null.
        DP_MessageType type = DP_message_type(msg);
        DP_ASSERT(type != DP_MSG_UNDO); // Undos and redos aren't historized.
        // Undo points must have an associated savepoint to
        // roll back to, other messages must not have one.
        if (type == DP_MSG_UNDO_POINT) {
            DP_ASSERT(entry->state);
            have_savepoint = true;
        }
        else {
            DP_ASSERT(!entry->state);
        }
    }
    // There must exist at least one savepoint.
    DP_ASSERT(have_savepoint);
#endif
}


DP_CanvasHistory *DP_canvas_history_new(void)
{
    DP_Mutex *mutex = DP_mutex_new();
    if (!mutex) {
        return NULL;
    }

    DP_CanvasHistory *ch = DP_malloc(sizeof(*ch));
    DP_CanvasState *cs = DP_canvas_state_new();
    size_t entries_size = sizeof(*ch->entries) * INITIAL_CAPACITY;

    *ch = (DP_CanvasHistory){mutex, cs, INITIAL_CAPACITY, 1,
                             DP_malloc(entries_size)};
    set_initial_entry(ch, cs);
    validate_history(ch);
    return ch;
}


static void dispose_entry(DP_CanvasHistoryEntry *entry)
{
    DP_message_decref(entry->msg);
    if (entry->state) {
        DP_canvas_state_decref(entry->state);
    }
}

static void truncate_history(DP_CanvasHistory *ch, int until)
{
    DP_debug("Truncating %d out of %d history entries", until, ch->used);
    DP_ASSERT(until <= ch->used);
    DP_CanvasHistoryEntry *entries = ch->entries;
    for (int i = 0; i < until; ++i) {
        dispose_entry(&entries[i]);
    }
    ch->used -= until;
    size_t size = sizeof(*entries) * DP_int_to_size(ch->used);
    memmove(entries, entries + until, size);
}

void DP_canvas_history_free(DP_CanvasHistory *ch)
{
    if (ch) {
        truncate_history(ch, ch->used);
        DP_free(ch->entries);
        DP_canvas_state_decref(ch->current_state);
        DP_mutex_free(ch->mutex);
        DP_free(ch);
    }
}

DP_CanvasState *DP_canvas_history_compare_and_get(DP_CanvasHistory *ch,
                                                  DP_CanvasState *prev)
{
    DP_ASSERT(ch);
    DP_Mutex *mutex = ch->mutex;
    DP_MUTEX_MUST_LOCK(mutex);
    DP_CanvasState *next = ch->current_state;
    DP_CanvasState *cs = next == prev ? NULL : DP_canvas_state_incref(next);
    DP_MUTEX_MUST_UNLOCK(mutex);
    return cs;
}

static void set_current_state_noinc(DP_CanvasHistory *ch, DP_CanvasState *next)
{
    DP_CanvasState *current = ch->current_state;
    DP_Mutex *mutex = ch->mutex;
    DP_MUTEX_MUST_LOCK(mutex);
    ch->current_state = next;
    DP_MUTEX_MUST_UNLOCK(mutex);
    DP_canvas_state_decref(current);
}


static void handle_reset(DP_CanvasHistory *ch)
{
    DP_CanvasState *cs = DP_canvas_state_new();
    set_current_state_noinc(ch, cs);
    truncate_history(ch, ch->used);
    set_initial_entry(ch, cs);
    ch->used = 1;
    validate_history(ch);
}

static bool handle_internal(DP_CanvasHistory *ch, DP_MsgInternal *mi)
{
    DP_MsgInternalType internal_type = DP_msg_internal_type(mi);
    switch (internal_type) {
    case DP_MSG_INTERNAL_TYPE_RESET:
        handle_reset(ch);
        return true;
    default:
        DP_error_set("Unhandled internal message type: %d", (int)internal_type);
        return false;
    }
}


static void ensure_append_capacity(DP_CanvasHistory *ch)
{
    int old_capacity = ch->capacity;
    if (ch->used == old_capacity) {
        int new_capacity = EXPAND_CAPACITY(old_capacity);
        size_t new_size = sizeof(*ch->entries) * DP_int_to_size(new_capacity);
        DP_debug("Resizing history capacity to %d entries", ch->capacity);
        ch->entries = DP_realloc(ch->entries, new_size);
        ch->capacity = new_capacity;
    }
}

static int append_to_history(DP_CanvasHistory *ch, DP_Message *msg)
{
    ensure_append_capacity(ch);
    int index = ch->used;
    ch->entries[index] =
        (DP_CanvasHistoryEntry){DP_UNDO_DONE, DP_message_incref(msg), NULL};
    ch->used = index + 1;
    return index;
}


static void make_save_point(DP_CanvasHistory *ch, int index)
{
    ch->entries[index].state = DP_canvas_state_incref(ch->current_state);
}

static int mark_undone_actions_gone(DP_CanvasHistory *ch, int index,
                                    int *out_depth)
{
    unsigned int context_id = DP_message_context_id(ch->entries[index].msg);
    DP_CanvasHistoryEntry *entries = ch->entries;
    int i = index - 1;
    int depth = 1;
    for (; i >= 0 && depth < UNDO_DEPTH_LIMIT; --i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        if (entry->state) {
            ++depth;
        }

        DP_Message *msg = entry->msg;
        if (DP_message_context_id(msg) == context_id) {
            if (entry->undo == DP_UNDO_GONE) {
                break; // Everything beyond this point is already gone.
            }
            else if (entry->undo == DP_UNDO_UNDONE) {
                entry->undo = DP_UNDO_GONE;
            }
        }
    }
    *out_depth = depth;
    return i;
}

static int find_first_unreachable_index(DP_CanvasHistory *ch, int i, int depth)
{
    DP_CanvasHistoryEntry *entries = ch->entries;
    for (; i >= 0 && depth < UNDO_DEPTH_LIMIT; --i) {
        if (entries[i].state) {
            ++depth;
        }
    }
    return i;
}

static void handle_undo_point(DP_CanvasHistory *ch, DP_Message *msg)
{
    int index = append_to_history(ch, msg);
    make_save_point(ch, index);
    int depth;
    int i = mark_undone_actions_gone(ch, index, &depth);
    int first_unreachable_index = find_first_unreachable_index(ch, i, depth);
    if (first_unreachable_index > 0) {
        truncate_history(ch, first_unreachable_index);
    }
}


static int find_first_undo_point(DP_CanvasHistory *ch, unsigned int context_id,
                                 int *out_depth)
{
    DP_CanvasHistoryEntry *entries = ch->entries;
    int i;
    int depth = 0;
    for (i = ch->used - 1; i >= 0 && depth <= UNDO_DEPTH_LIMIT; --i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        if (entry->state) {
            ++depth;
            if (entry->undo == DP_UNDO_DONE
                && DP_message_context_id(entry->msg) == context_id) {
                break;
            }
        }
    }
    *out_depth = depth;
    return i;
}

static void mark_entries_undone(DP_CanvasHistory *ch, unsigned int context_id,
                                int undo_start)
{
    DP_CanvasHistoryEntry *entries = ch->entries;
    int used = ch->used;
    for (int i = undo_start; i < used; ++i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        if (entry->undo == DP_UNDO_DONE
            && DP_message_context_id(entry->msg) == context_id) {
            entry->undo = DP_UNDO_UNDONE;
        }
    }
}

static int undo(DP_CanvasHistory *ch, unsigned int context_id)
{
    int depth;
    int undo_start = find_first_undo_point(ch, context_id, &depth);
    if (depth > UNDO_DEPTH_LIMIT) {
        DP_error_set("Undo by user %u beyond history limit", context_id);
        return -1;
    }
    else if (undo_start < 0) {
        DP_error_set("Nothing found to undo for user %u", context_id);
        return -1;
    }
    else {
        mark_entries_undone(ch, context_id, undo_start);
        return undo_start;
    }
}


static int find_oldest_redo_point(DP_CanvasHistory *ch, unsigned int context_id,
                                  int *out_depth)
{
    DP_CanvasHistoryEntry *entries = ch->entries;
    int redo_start = -1;
    int depth = 0;
    for (int i = ch->used - 1; i >= 0 && depth <= UNDO_DEPTH_LIMIT; --i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        if (entry->state) {
            ++depth;
            if (DP_message_context_id(entry->msg) == context_id) {
                DP_Undo undo = entry->undo;
                if (undo == DP_UNDO_UNDONE) {
                    redo_start = i;
                }
                else if (undo == DP_UNDO_DONE) {
                    break;
                }
            }
        }
    }
    *out_depth = depth;
    return redo_start;
}

static void mark_entries_redone(DP_CanvasHistory *ch, unsigned int context_id,
                                int redo_start)
{
    DP_CanvasHistoryEntry *entries = ch->entries;
    entries[redo_start].undo = DP_UNDO_DONE;
    int used = ch->used;
    for (int i = redo_start + 1; i < used; ++i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        if (DP_message_context_id(entry->msg) == context_id) {
            DP_Undo undo = entry->undo;
            if (entry->state && undo != DP_UNDO_GONE) {
                break;
            }
            else if (undo == DP_UNDO_UNDONE) {
                entry->undo = DP_UNDO_DONE;
            }
        }
    }
}

static int redo(DP_CanvasHistory *ch, unsigned int context_id)
{
    int depth;
    int redo_start = find_oldest_redo_point(ch, context_id, &depth);
    if (depth > UNDO_DEPTH_LIMIT) {
        DP_error_set("Redo by user %u beyond history limit", context_id);
        return -1;
    }
    else if (redo_start < 0) {
        DP_error_set("Nothing found to redo for user %u", context_id);
        return -1;
    }
    else {
        mark_entries_redone(ch, context_id, redo_start);
        return redo_start;
    }
}


static void replay_undo_point(DP_CanvasState *cs, DP_CanvasHistoryEntry *entry)
{
    DP_CanvasState *prev = entry->state;
    entry->state = DP_canvas_state_incref(cs);
    DP_canvas_state_decref(prev);
}

static DP_CanvasState *
replay_drawing_command(DP_CanvasState *cs, DP_DrawContext *dc, DP_Message *msg)
{
    DP_CanvasState *next = DP_canvas_state_handle(cs, dc, msg);
    if (next) {
        DP_canvas_state_decref(cs);
        return next;
    }
    else {
        DP_warn("Error replaying drawing command: %s", DP_error());
        return cs;
    }
}

static DP_CanvasState *replay(DP_CanvasState *cs, DP_DrawContext *dc,
                              DP_CanvasHistoryEntry *entry)
{
    DP_Message *msg = entry->msg;
    switch (DP_message_type(msg)) {
    case DP_MSG_UNDO_POINT:
        replay_undo_point(cs, entry);
        return cs;
    default:
        return replay_drawing_command(cs, dc, msg);
    }
}

static void replay_from(DP_CanvasHistory *ch, DP_DrawContext *dc, int start)
{
    DP_CanvasHistoryEntry *entries = ch->entries;
    DP_CanvasState *cs = DP_canvas_state_incref(entries[start].state);
    DP_ASSERT(cs);

    int used = ch->used;
    for (int i = start + 1; i < used; ++i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        if (entry->undo == DP_UNDO_DONE) {
            cs = replay(cs, dc, entry);
            validate_history(ch);
        }
    }

    set_current_state_noinc(ch, cs);
}


static bool handle_undo(DP_CanvasHistory *ch, DP_DrawContext *dc,
                        DP_Message *msg)
{
    DP_MsgUndo *mu = DP_msg_undo_cast(msg);
    bool is_redo = DP_msg_undo_is_redo(mu);
    unsigned int context_id = DP_msg_undo_override_id(mu);
    if (context_id == 0) {
        context_id = DP_message_context_id(msg);
    }

    int i = is_redo ? redo(ch, context_id) : undo(ch, context_id);
    if (i >= 0) {
        replay_from(ch, dc, i);
        return true;
    }
    else {
        return false;
    }
}


static bool handle_drawing_command(DP_CanvasHistory *ch, DP_DrawContext *dc,
                                   DP_Message *msg)
{
    append_to_history(ch, msg);
    DP_CanvasState *next = DP_canvas_state_handle(ch->current_state, dc, msg);
    if (next) {
        set_current_state_noinc(ch, next);
        return true;
    }
    else {
        return false;
    }
}


bool DP_canvas_history_handle(DP_CanvasHistory *ch, DP_DrawContext *dc,
                              DP_Message *msg)
{
    DP_ASSERT(ch);
    DP_ASSERT(msg);
    DP_MessageType type = DP_message_type(msg);
    DP_debug("History command %d %s", (int)type,
             DP_message_type_enum_name(type));
    bool ok;
    switch (type) {
    case DP_MSG_INTERNAL:
        ok = handle_internal(ch, DP_msg_internal_cast(msg));
        break;
    case DP_MSG_UNDO_POINT:
        handle_undo_point(ch, msg);
        ok = true;
        break;
    case DP_MSG_UNDO:
        ok = handle_undo(ch, dc, msg);
        break;
    default:
        ok = handle_drawing_command(ch, dc, msg);
        break;
    }
    validate_history(ch);
    return ok;
}
