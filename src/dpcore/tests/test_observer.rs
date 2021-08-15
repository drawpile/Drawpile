// This file is part of Drawpile.
// Copyright (C) 2020 Calle Laakkonen
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

use dpcore::canvas::{CanvasObserver, CanvasState, ObservableCanvasState};
use dpcore::paint::{AoE, Size};
use dpcore::protocol::message::{CanvasResizeMessage, CommandMessage};

use std::cell::RefCell;
use std::mem;
use std::rc::Rc;

#[test]
fn test_canvas_state_observation() {
    let mut canvas = ObservableCanvasState::new(CanvasState::new());

    let observer = Rc::new(RefCell::new(TestObserver {
        changed: AoE::Nothing,
    }));

    canvas.add_observer(observer.clone());

    assert_eq!(observer.borrow().changed, AoE::Nothing);
    assert_eq!(canvas.observer_count(), 1);

    canvas.receive_message(&CommandMessage::CanvasResize(
        1,
        CanvasResizeMessage {
            top: 0,
            right: 100,
            bottom: 100,
            left: 0,
        },
    ));

    assert_eq!(observer.borrow().changed, AoE::Resize(0, 0, Size::new(0, 0)));

    drop(observer);
    assert_eq!(canvas.observer_count(), 1);

    // missing observer is not noticed until the next notification
    canvas.receive_message(&CommandMessage::CanvasResize(
        1,
        CanvasResizeMessage {
            top: 100,
            right: 0,
            bottom: 0,
            left: 200,
        },
    ));

    assert_eq!(canvas.observer_count(), 0);
}

struct TestObserver {
    changed: AoE,
}

impl CanvasObserver for TestObserver {
    fn changed(&mut self, aoe: &AoE) {
        let changed = mem::replace(&mut self.changed, AoE::Nothing);
        self.changed = changed.merge(aoe.clone());
    }
}
