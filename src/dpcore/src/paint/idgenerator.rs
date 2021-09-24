// This file is part of Drawpile.
// Copyright (C) 2021 Calle Laakkonen
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

// This could be made generic with num_traits
pub struct IDGenerator {
    used_ids: Vec<u8>,
    next_id: Option<u8>,
}

impl IDGenerator {
    pub fn new(first_id: u8, used_ids: Vec<u8>) -> Self {
        assert!(!used_ids.contains(&first_id));
        Self {
            used_ids,
            next_id: Some(first_id),
        }
    }

    pub fn take_next(&mut self) -> Option<u8> {
        if let Some(id) = self.next_id {
            self.used_ids.push(id);

            let mut next_id = id.wrapping_add(1);

            while next_id != id && self.used_ids.contains(&next_id) {
                next_id = next_id.wrapping_add(1);
            }

            if next_id != id {
                self.next_id = Some(next_id);
            } else {
                self.next_id = None;
            }

            Some(id)
        } else {
            None
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_id_generation() {
        let mut idgen = IDGenerator::new(1, vec![2, 4]);
        assert_eq!(idgen.take_next(), Some(1));
        assert_eq!(idgen.take_next(), Some(3));
        assert_eq!(idgen.take_next(), Some(5));
        assert_eq!(idgen.take_next(), Some(6));
    }

    #[test]
    fn test_id_wraparound() {
        let mut idgen = IDGenerator::new(1, vec![2]);
        assert_eq!(idgen.take_next(), Some(1));
        assert_eq!(idgen.take_next(), Some(3));
        idgen.next_id = Some(255);
        assert_eq!(idgen.take_next(), Some(255));
        assert_eq!(idgen.take_next(), Some(0));
        assert_eq!(idgen.take_next(), Some(4));
    }
}
