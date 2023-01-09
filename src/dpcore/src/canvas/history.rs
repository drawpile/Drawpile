// This file is part of Drawpile.
// Copyright (C) 2020-2021 Calle Laakkonen
//
// Drawpile is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// As additional permission under section 7, you are allowed to distribute
// the software through an app store, even if that store has restrictive
// terms and conditions that are incompatible with the GPL, provided that
// the source is also available under the GPL with or without this permission
// through a channel without those restrictive terms and conditions.
//
// Drawpile is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Drawpile.  If not, see <https://www.gnu.org/licenses/>.

use crate::paint::{LayerStack, UserID};
use crate::protocol::message::{CommandMessage, UNDO_DEPTH};

use std::sync::Arc;

struct HistoryEntry {
    msg: CommandMessage,
    state: UndoState,
    seq_num: u32,
}

#[derive(Eq, PartialEq)]
enum UndoState {
    Done,
    Undone,
    Gone,
}

struct Savepoint {
    layerstack: Arc<LayerStack>,
    seq_num: u32,
}

pub struct History {
    history: Vec<HistoryEntry>,
    savepoints: Vec<Savepoint>,
    sequence: u32,
}

impl History {
    pub fn new() -> History {
        History {
            history: Vec::new(),
            savepoints: Vec::new(),
            sequence: 0,
        }
    }

    pub fn add(&mut self, msg: CommandMessage) {
        // Undo messages cannot be undone/redone so no point in
        // storing them in the history.
        let mut branch = false;
        let mut branch_user: UserID = 0;
        match msg {
            CommandMessage::Undo(_, _) => return,
            CommandMessage::UndoPoint(user_id) => {
                branch = true;
                branch_user = user_id;
            }
            _ => (),
        }

        self.sequence += 1;

        self.history.push(HistoryEntry {
            msg,
            state: UndoState::Done,
            seq_num: self.sequence,
        });

        if branch {
            // Adding a new UndoPoint (the start of an undoable sequence) branches the undo history.
            // All undo sequences in the old branch (i.e. all undone actions by this user) become inaccessible.
            self.history
                .iter_mut()
                .filter(|e| e.state == UndoState::Undone && e.msg.user() == branch_user)
                .for_each(|e| e.state = UndoState::Gone);

            // We can drop savepoints older than the oldest undopoint,
            // but we must keep one whose segnum is <= oldest UP seqnum
            // so the oldest undopoint remains accessible.
            if let Some(oldest) = self.oldest_undopoint_seqnum() {
                let mut iter = self.savepoints.iter().peekable();
                let mut delete_up_to = 0;
                while let Some(sp) = iter.next() {
                    if let Some(next) = iter.peek() {
                        if next.seq_num < oldest {
                            delete_up_to = sp.seq_num;
                        } else {
                            break;
                        }
                    }
                }

                if delete_up_to > 0 {
                    self.savepoints.retain(|sp| sp.seq_num > delete_up_to);
                    self.history.retain(|e| e.seq_num > delete_up_to);
                }
            }
        }
    }

    // Find the sequence number of the oldest UndoPoint that's
    // still within the protocol history depth limit.
    // Note that unreachable undopoints still count towards the limit!
    fn oldest_undopoint_seqnum(&self) -> Option<u32> {
        let mut ups = 0;
        let mut oldest: Option<u32> = None;
        for entry in self.history.iter().rev() {
            if let CommandMessage::UndoPoint(_) = entry.msg {
                ups += 1;
                oldest = Some(entry.seq_num);

                if ups >= UNDO_DEPTH {
                    break;
                }
            }
        }

        oldest
    }

    /// Undo the given user's last undoable sequence.
    /// The messages are marked as undone and a snapshot of the canvas at some point prior to the undone sequence + messages that must
    /// be replayed are returned.
    pub fn undo(&mut self, user: UserID) -> Option<(Arc<LayerStack>, Vec<CommandMessage>)> {
        // Step 1. Find the first not-undone UndoPoint belonging to this user,
        // starting from the end of the history.
        let oldest_up = self.oldest_undopoint_seqnum()?;

        let first_undopoint_seqnum = self
            .history
            .iter()
            .rev()
            .take_while(|e| e.seq_num >= oldest_up)
            .find(|e| is_undopoint(&e.msg, user) && e.state == UndoState::Done)?
            .seq_num;

        // Step 2. Find the savepoint closest to (but preceding) this undopoint
        let savepoint = self
            .savepoints
            .iter()
            .rfind(|sp| sp.seq_num <= first_undopoint_seqnum)?;

        // Step 3. Mark all messages by this user as undone, from undopoint to the end
        self.history
            .iter_mut()
            .rev()
            .filter(|e| e.msg.user() == user && e.state == UndoState::Done)
            .take_while(|e| e.seq_num >= first_undopoint_seqnum)
            .for_each(|e| e.state = UndoState::Undone);

        // Step 4. Return the savepoint and all messages to be redone
        let replay = self
            .history
            .iter()
            .filter(|e| e.seq_num > savepoint.seq_num && e.state == UndoState::Done)
            .map(|e| e.msg.clone())
            .collect();

        let layerstack = savepoint.layerstack.clone();

        // Savepoints newer than this one are no longer valid
        let retain_up_to = savepoint.seq_num;
        self.savepoints.retain(|sp| sp.seq_num <= retain_up_to);

        Some((layerstack, replay))
    }

    /// Undo the given user's last undoable sequence.
    /// The messages are marked as done and a snapshot of the canvas at some point prior to the undone sequence + messages that must
    /// be replayed are returned.
    pub fn redo(&mut self, user: UserID) -> Option<(Arc<LayerStack>, Vec<CommandMessage>)> {
        // Step 1. Find the oldest undone undopoint
        let oldest_up = self.oldest_undopoint_seqnum()?;

        let first_undopoint_seqnum = self
            .history
            .iter()
            .skip_while(|e| e.seq_num < oldest_up)
            .find(|e| is_undopoint(&e.msg, user) && e.state == UndoState::Undone)?
            .seq_num;

        // Step 2. Find the savepoint closest to (but preceding) this undopoint
        let savepoint = self
            .savepoints
            .iter()
            .rfind(|sp| sp.seq_num <= first_undopoint_seqnum)?;

        // Step 3. Mark undone messages by this user as done,
        // up until the next undopoint
        self.history
            .iter_mut()
            .skip_while(|e| e.seq_num < first_undopoint_seqnum)
            .filter(|e| e.msg.user() == user && e.state != UndoState::Gone)
            .take_while(|e| !is_any_undopoint(&e.msg) || e.seq_num == first_undopoint_seqnum)
            .for_each(|e| e.state = UndoState::Done);

        // Step 4. Return the savepoint and all messages to be redone
        let replay = self
            .history
            .iter()
            .filter(|e| e.seq_num > savepoint.seq_num && e.state == UndoState::Done)
            .map(|e| e.msg.clone())
            .collect();

        let layerstack = savepoint.layerstack.clone();

        // Savepoints newer than this one are no longer valid
        let retain_up_to = savepoint.seq_num;
        self.savepoints.retain(|sp| sp.seq_num <= retain_up_to);

        Some((layerstack, replay))
    }

    pub fn add_savepoint(&mut self, layerstack: Arc<LayerStack>) {
        if self.savepoints.last().map(|sp| sp.seq_num) != Some(self.sequence) {
            self.savepoints.push(Savepoint {
                layerstack,
                seq_num: self.sequence,
            });
        }
    }

    /// Find a savepoint at or before this position and reset history to it.
    pub fn reset_before(&mut self, pos: u32) -> Option<(Arc<LayerStack>, Vec<CommandMessage>)> {
        let savepoint = self.savepoints.iter().rfind(|sp| sp.seq_num <= pos)?;

        let replay = self
            .history
            .iter()
            .filter(|e| e.seq_num > savepoint.seq_num && e.state == UndoState::Done)
            .map(|e| e.msg.clone())
            .collect();

        let layerstack = savepoint.layerstack.clone();

        // Savepoints newer than this one are no longer valid
        let retain_up_to = savepoint.seq_num;
        self.savepoints.retain(|sp| sp.seq_num <= retain_up_to);

        Some((layerstack, replay))
    }

    /// Return the sequence number of the last message
    pub fn end(&self) -> u32 {
        self.sequence
    }

    // Clear out undo history
    pub fn truncate(&mut self) {
        self.history.clear();
        self.savepoints.clear();
    }
}

fn is_any_undopoint(msg: &CommandMessage) -> bool {
    matches!(msg, CommandMessage::UndoPoint(_))
}

/// Check that the given message is an undopoint with the given user ID
fn is_undopoint(msg: &CommandMessage, user: u8) -> bool {
    match msg {
        CommandMessage::UndoPoint(u) => *u == user,
        _ => false,
    }
}
