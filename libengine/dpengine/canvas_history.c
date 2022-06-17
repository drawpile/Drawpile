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
#include "affected_area.h"
#include "canvas_state.h"
#include "dpmsg/messages/undo.h"
#include <dpcommon/atomic.h>
#include <dpcommon/conversions.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpmsg/message.h>
#include <dpmsg/messages/internal.h>
#include <dpmsg/messages/undo_point.h>


#define INITIAL_CAPACITY              1024
#define EXPAND_CAPACITY(OLD_CAPACITY) ((OLD_CAPACITY)*2)

#define UNDO_DEPTH_LIMIT 30

#define MAX_FALLBEHIND 10000

typedef enum DP_ForkAction {
    DP_FORK_ACTION_CONCURRENT,
    DP_FORK_ACTION_ALREADY_DONE,
    DP_FORK_ACTION_ROLLBACK,
} DP_ForkAction;

typedef enum DP_Undo {
    DP_UNDO_DONE,
    DP_UNDO_UNDONE,
    DP_UNDO_GONE,
} DP_Undo;

typedef struct DP_CanvasHistoryEntry {
    DP_Undo undo;
    DP_Message *msg;
    DP_CanvasState *state;
} DP_CanvasHistoryEntry;

typedef struct DP_ForkEntry {
    DP_Message *msg;
    DP_AffectedArea aa;
} DP_ForkEntry;

struct DP_CanvasHistory {
    DP_Mutex *mutex;
    DP_CanvasState *current_state;
    int offset;
    int capacity;
    int used;
    DP_CanvasHistoryEntry *entries;
    struct {
        int start;
        int fallbehind;
        DP_Queue queue;
    } fork;
    struct {
        DP_CanvasHistorySavePointFn fn;
        void *user;
    } save_point;
    DP_Atomic local_pen_down;
};


static void set_fork_start(DP_CanvasHistory *ch)
{
    DP_ASSERT(ch->used > 0);
    DP_ASSERT(ch->offset >= 0);
    ch->fork.start = ch->offset + ch->used - 1;
}

static void dispose_fork_entry(void *element)
{
    DP_ForkEntry *fe = element;
    DP_message_decref(fe->msg);
}

static void clear_fork_entries(DP_CanvasHistory *ch)
{
    DP_ASSERT(ch);
    DP_debug("Clearing %zu fork entries", ch->fork.queue.used);
    DP_queue_clear(&ch->fork.queue, sizeof(DP_ForkEntry), dispose_fork_entry);
}

static void push_fork_entry_inc(DP_CanvasHistory *ch, DP_Message *msg)
{
    DP_ASSERT(ch);
    DP_ASSERT(msg);
    DP_ForkEntry *fe = DP_queue_push(&ch->fork.queue, sizeof(DP_ForkEntry));
    *fe = (DP_ForkEntry){DP_message_incref(msg),
                         DP_affected_area_make(msg, ch->current_state)};
}

static DP_Message *peek_fork_entry_message(DP_CanvasHistory *ch)
{
    DP_ASSERT(ch);
    DP_ASSERT(ch->fork.queue.used != 0);
    DP_ForkEntry *fe = DP_queue_peek(&ch->fork.queue, sizeof(DP_ForkEntry));
    DP_ASSERT(fe);
    return fe->msg;
}

static void shift_fork_entry_nodec(DP_CanvasHistory *ch)
{
    DP_queue_shift(&ch->fork.queue);
}

static bool fork_entry_concurrent_with(void *element, void *user)
{
    DP_ForkEntry *fe = element;
    DP_AffectedArea *aa = user;
    return DP_affected_area_concurrent_with(aa, &fe->aa);
}

static bool fork_entries_concurrent_with(DP_CanvasHistory *ch, DP_Message *msg)
{
    DP_ASSERT(ch);
    DP_ASSERT(msg);
    DP_AffectedArea aa = DP_affected_area_make(msg, ch->current_state);
    return DP_queue_all(&ch->fork.queue, sizeof(DP_ForkEntry),
                        fork_entry_concurrent_with, &aa);
}


static void call_save_point_fn(DP_CanvasHistory *ch, int history_index,
                               DP_CanvasState *cs)
{
    DP_CanvasHistorySavePointFn save_point_fn = ch->save_point.fn;
    if (save_point_fn) {
        save_point_fn(cs, history_index, ch->save_point.user);
    }
}

static void set_initial_entry(DP_CanvasHistory *ch, DP_CanvasState *cs)
{
    ch->entries[0] = (DP_CanvasHistoryEntry){
        DP_UNDO_DONE, DP_msg_undo_point_new(0), DP_canvas_state_incref(cs)};
    call_save_point_fn(ch, 0, cs);
}

static bool is_undo_point_entry(DP_CanvasHistoryEntry *entry)
{
    return DP_message_type(entry->msg) == DP_MSG_UNDO_POINT;
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
        // Only undo points are allowed to have savepoints.
        if (!is_undo_point_entry(entry)) {
            DP_ASSERT(!entry->state);
        }
        else if (entry->state) {
            have_savepoint = true;
        }
    }
    // There must exist at least one savepoint.
    DP_ASSERT(have_savepoint);
    // If the local fork contains entries, it must also be consistent.
    if (ch->fork.queue.used != 0) {
        // Fork start can't be beyond the truncation point.
        DP_ASSERT(ch->fork.start >= ch->offset);
    }
#endif
}


DP_CanvasHistory *
DP_canvas_history_new(DP_CanvasHistorySavePointFn save_point_fn,
                      void *save_point_user)
{
    DP_Mutex *mutex = DP_mutex_new();
    if (!mutex) {
        return NULL;
    }

    DP_CanvasHistory *ch = DP_malloc(sizeof(*ch));
    DP_CanvasState *cs = DP_canvas_state_new();
    size_t entries_size = sizeof(*ch->entries) * INITIAL_CAPACITY;

    *ch = (DP_CanvasHistory){mutex,
                             cs,
                             0,
                             INITIAL_CAPACITY,
                             1,
                             DP_malloc(entries_size),
                             {0, 0, DP_QUEUE_NULL},
                             {save_point_fn, save_point_user},
                             DP_ATOMIC_INIT(0)};

    DP_queue_init(&ch->fork.queue, INITIAL_CAPACITY, sizeof(DP_ForkEntry));
    set_initial_entry(ch, cs);
    validate_history(ch);
    return ch;
}


static void dispose_entry(DP_CanvasHistoryEntry *entry)
{
    DP_message_decref(entry->msg);
    DP_canvas_state_decref_nullable(entry->state);
}

static void truncate_history(DP_CanvasHistory *ch, int until)
{
    DP_debug("Truncating %d out of %d history entries (offset %d, fork %d)",
             until, ch->used, ch->offset,
             ch->fork.queue.used == 0 ? -1 : ch->fork.start);
    DP_ASSERT(until <= ch->used);
    DP_ASSERT(ch->fork.queue.used == 0 || ch->fork.start >= ch->offset + until);
    DP_CanvasHistoryEntry *entries = ch->entries;
    for (int i = 0; i < until; ++i) {
        dispose_entry(&entries[i]);
    }
    ch->used -= until;
    ch->offset += until;
    size_t size = sizeof(*entries) * DP_int_to_size(ch->used);
    memmove(entries, entries + until, size);
}

void DP_canvas_history_free(DP_CanvasHistory *ch)
{
    if (ch) {
        clear_fork_entries(ch);
        DP_queue_dispose(&ch->fork.queue);
        truncate_history(ch, ch->used);
        DP_free(ch->entries);
        DP_canvas_state_decref(ch->current_state);
        DP_mutex_free(ch->mutex);
        DP_free(ch);
    }
}

bool DP_canvas_history_local_pen_down(DP_CanvasHistory *ch)
{
    DP_ASSERT(ch);
    return DP_atomic_get(&ch->local_pen_down);
}

void DP_canvas_history_local_pen_down_set(DP_CanvasHistory *ch,
                                          bool local_pen_down)
{
    DP_ASSERT(ch);
    DP_atomic_set(&ch->local_pen_down, local_pen_down);
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


static void reset_to_state_noinc(DP_CanvasHistory *ch, DP_CanvasState *cs)
{
    set_current_state_noinc(ch, cs);
    clear_fork_entries(ch);
    truncate_history(ch, ch->used);
    set_initial_entry(ch, cs);
    ch->used = 1;
    ch->offset = 0;
    validate_history(ch);
}

void DP_canvas_history_reset(DP_CanvasHistory *ch)
{
    // Hard reset: erase history and start from an empty canvas state. Is used
    // when the history gets too large. After the reset, there will usually be
    // a canvas resize command followed by layer creation and put tile commands
    // to restore the canvas as it was before the reset. Kinda like git squash.
    reset_to_state_noinc(ch, DP_canvas_state_new());
}

void DP_canvas_history_soft_reset(DP_CanvasHistory *ch)
{
    // Soft reset: erase history, but keep the current canvas state.
    // Is used to avoid resetting the whole session when a new user joins and
    // the server doesn't have the history available, which is the case for the
    // built-in, non-dedicated Drawpile server. It sends a hard reset only to
    // the newly joined client, all the other clients get a soft reset so that
    // they can't undo beyond the point that the new client joined at.
    reset_to_state_noinc(ch, DP_canvas_state_incref(ch->current_state));
}

static bool handle_internal(DP_CanvasHistory *ch, DP_MsgInternal *mi)
{
    DP_MsgInternalType internal_type = DP_msg_internal_type(mi);
    switch (internal_type) {
    case DP_MSG_INTERNAL_TYPE_RESET:
        DP_canvas_history_reset(ch);
        return true;
    case DP_MSG_INTERNAL_TYPE_SOFT_RESET:
        DP_canvas_history_soft_reset(ch);
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
    DP_ASSERT(DP_message_type(msg) != DP_MSG_UNDO);
    ensure_append_capacity(ch);
    int index = ch->used;
    ch->entries[index] =
        (DP_CanvasHistoryEntry){DP_UNDO_DONE, DP_message_incref(msg), NULL};
    ch->used = index + 1;
    return index;
}


static void make_savepoint(DP_CanvasHistory *ch, int index)
{
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < ch->used);
    // Don't make savepoints while a local fork is present, since the local
    // state may be incongruent with what the server thinks is happening.
    if (ch->fork.queue.used == 0) {
        DP_CanvasState *cs = ch->current_state;
        ch->entries[index].state = DP_canvas_state_incref(cs);
        call_save_point_fn(ch, ch->offset + index, cs);
    }
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
        if (is_undo_point_entry(entry)) {
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
        if (is_undo_point_entry(&entries[i])) {
            ++depth;
        }
    }
    return i;
}

static void handle_undo_point(DP_CanvasHistory *ch, int index)
{
    make_savepoint(ch, index);
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
        if (is_undo_point_entry(entry)) {
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
        if (is_undo_point_entry(entry)) {
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
            if (is_undo_point_entry(entry) && undo != DP_UNDO_GONE) {
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
    DP_canvas_state_decref_nullable(prev);
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

static int search_savepoint_index(DP_CanvasHistory *ch, int target_index)
{
    DP_CanvasHistoryEntry *entries = ch->entries;
    for (int i = target_index; i >= 0; --i) {
        if (entries[i].state) {
            return i;
        }
    }
    return -1;
}

static void replay_fork(void *element, void *user)
{
    DP_ForkEntry *fe = element;
    DP_Message *msg = fe->msg;
    DP_MessageType type = DP_message_type(msg);
    if (type != DP_MSG_UNDO && type != DP_MSG_UNDO_POINT) {
        DP_CanvasState **cs = ((void **)user)[1];
        DP_DrawContext *dc = ((void **)user)[2];
        *cs = replay_drawing_command(*cs, dc, msg);
        DP_CanvasHistory *ch = ((void **)user)[0];
        validate_history(ch);
    }
}

static bool replay_from(DP_CanvasHistory *ch, DP_DrawContext *dc,
                        int target_index)
{
    int start_index = search_savepoint_index(ch, target_index);
    if (start_index < 0) {
        DP_error_set("Can't find savepoint at or before %d", target_index);
        return false;
    }

    DP_CanvasHistoryEntry *entries = ch->entries;
    DP_CanvasState *cs = DP_canvas_state_incref(entries[start_index].state);
    DP_ASSERT(cs);

    int used = ch->used;
    for (int i = start_index + 1; i < used; ++i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        if (entry->undo == DP_UNDO_DONE) {
            cs = replay(cs, dc, entry);
            validate_history(ch);
        }
    }

    if (ch->fork.queue.used != 0) {
        DP_ASSERT(ch->fork.start >= start_index + ch->offset);
        set_fork_start(ch);
        DP_queue_each(&ch->fork.queue, sizeof(DP_ForkEntry), replay_fork,
                      (void *[]){ch, &cs, dc});
    }

    set_current_state_noinc(ch, cs);
    return true;
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

    int target_index = is_redo ? redo(ch, context_id) : undo(ch, context_id);
    return target_index >= 0 ? replay_from(ch, dc, target_index) : false;
}


static bool handle_drawing_command(DP_CanvasHistory *ch, DP_DrawContext *dc,
                                   DP_Message *msg)
{
    DP_CanvasState *next = DP_canvas_state_handle(ch->current_state, dc, msg);
    if (next) {
        set_current_state_noinc(ch, next);
        return true;
    }
    else {
        return false;
    }
}


static bool handle_command(DP_CanvasHistory *ch, DP_DrawContext *dc,
                           DP_Message *msg, DP_MessageType type, int index)
{
    DP_ASSERT(DP_message_type(msg) == type);
    switch (type) {
    case DP_MSG_UNDO_POINT:
        handle_undo_point(ch, index);
        return true;
    case DP_MSG_UNDO:
        return handle_undo(ch, dc, msg);
    default:
        return handle_drawing_command(ch, dc, msg);
    }
}

static DP_ForkAction reconcile_remote_command(DP_CanvasHistory *ch,
                                              DP_Message *msg,
                                              DP_MessageType type)
{
    DP_ASSERT(DP_message_type(msg) == type);

    size_t used = ch->fork.queue.used;
    if (used == 0) {
        DP_debug("Concurrent: fork is empty");
        return DP_FORK_ACTION_CONCURRENT;
    }

    DP_Message *peeked_msg = peek_fork_entry_message(ch);
    if (DP_message_context_id(msg) == DP_message_context_id(peeked_msg)) {
        if (DP_message_equals(msg, peeked_msg)) {
            shift_fork_entry_nodec(ch);
            DP_message_decref(peeked_msg);
            if (used == 1) {
                DP_debug("Fork fallbehind cleared");
                ch->fork.fallbehind = 0;
            }
            if (type == DP_MSG_UNDO || type == DP_MSG_UNDO_POINT) {
                // Undo operations aren't handled locally.
                DP_debug("Concurrent: fork received expected undo message");
                return DP_FORK_ACTION_CONCURRENT;
            }
            else {
                DP_debug("Already done: fork received expected message");
                return DP_FORK_ACTION_ALREADY_DONE;
            }
        }
        else {
            DP_debug("Rollback: fork got %d (%s) instead of %d (%s)",
                     (int)DP_message_type(peeked_msg),
                     DP_message_type_enum_name(DP_message_type(peeked_msg)),
                     (int)type, DP_message_type_enum_name(type));
            ch->fork.fallbehind = 0;
            clear_fork_entries(ch);
            return DP_FORK_ACTION_ROLLBACK;
        }
    }
    else if (++ch->fork.fallbehind >= MAX_FALLBEHIND) {
        DP_debug("Rollback: fork fallbehind %d >= max fallbehind %d",
                 ch->fork.fallbehind, MAX_FALLBEHIND);
        ch->fork.fallbehind = 0;
        clear_fork_entries(ch);
        return DP_FORK_ACTION_ROLLBACK;
    }
    else if (fork_entries_concurrent_with(ch, msg)) {
        DP_debug("Concurrent: fork not in conflict with received message");
        return DP_FORK_ACTION_CONCURRENT;
    }
    else {
        DP_debug("Rollback: non-concurrent fork");
        // Avoid a rollback storm by clearing the local fork, but not when
        // drawing is in progress, since that would cause a feedback loop.
        if (!DP_canvas_history_local_pen_down(ch)) {
            clear_fork_entries(ch);
        }
        return DP_FORK_ACTION_ROLLBACK;
    }
}

static bool handle_remote_command(DP_CanvasHistory *ch, DP_DrawContext *dc,
                                  DP_Message *msg, DP_MessageType type)
{
    DP_ASSERT(DP_message_type(msg) == type);

    int index = type == DP_MSG_UNDO ? -1 : append_to_history(ch, msg);

    DP_ForkAction fork_action = reconcile_remote_command(ch, msg, type);
    switch (fork_action) {
    case DP_FORK_ACTION_CONCURRENT:
        return handle_command(ch, dc, msg, type, index);
    case DP_FORK_ACTION_ROLLBACK:
        return replay_from(ch, dc, ch->fork.start - ch->offset);
    default:
        DP_ASSERT(fork_action == DP_FORK_ACTION_ALREADY_DONE);
        return true;
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

    bool ok = type == DP_MSG_INTERNAL
                ? handle_internal(ch, DP_msg_internal_cast(msg))
                : handle_remote_command(ch, dc, msg, type);
    validate_history(ch);
    return ok;
}


bool DP_canvas_history_handle_local(DP_CanvasHistory *ch, DP_DrawContext *dc,
                                    DP_Message *msg)
{
    DP_ASSERT(ch);
    DP_ASSERT(msg);

    DP_MessageType type = DP_message_type(msg);
    DP_debug("Local command %d %s", (int)type, DP_message_type_enum_name(type));

    if (ch->fork.queue.used == 0) {
        set_fork_start(ch);
        // TODO: create a savepoint here?
    }
    push_fork_entry_inc(ch, msg);

    bool ok = type == DP_MSG_UNDO || type == DP_MSG_UNDO_POINT
           || handle_drawing_command(ch, dc, msg);
    validate_history(ch);
    return ok;
}
