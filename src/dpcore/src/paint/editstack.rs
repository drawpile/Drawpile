use super::{LayerID, Layer, RootGroup, GroupLayer};

use std::convert::TryInto;

/// Make a new layer ordering with the source layer moved on top (or below) of the target layer.
/// If into_group is set to true, the source will be moved into the top of the target group.
///
/// Returns a layer reordering vector or None if move is not possible
pub fn move_ordering(root: &RootGroup, source_layer_id: LayerID, target_layer_id: LayerID, into_group: bool, below: bool) -> Option<Vec<u16>> {
    if source_layer_id == target_layer_id {
        return None;
    }

    let source_layer = root.get_layer(source_layer_id)?;
    if root.get_layer(target_layer_id).is_none() {
        return None;
    }

    fn order_source(ordering: &mut Vec<u16>, source: &Layer) {
        match source {
            Layer::Group(g) => {
                ordering.push(g.layer_count() as u16);
                ordering.push(g.metadata().id.try_into().unwrap());
                for gl in g.iter_layers() {
                    order_source(ordering, gl)
                }
            }
            _ => {
                ordering.push(0);
                ordering.push(source.metadata().id.try_into().unwrap());
            }
        }
    }

    fn order(ordering: &mut Vec<u16>, group: &GroupLayer, source: &Layer, target_layer_id: LayerID, into_group: bool, below: bool) -> u16 {
        // group layer count may change due to insertion or removal of source layer
        let mut children = group.layer_count() as u16;

        for layer in group.iter_layers() {
            let id = layer.metadata().id;

            // Source layer does not appear in its original position
            if id == source.metadata().id {
                children -= 1;
                continue;
            }

            // Prepend source layer if not inserting to group
            if !into_group && !below && layer.metadata().id == target_layer_id {
                order_source(ordering, source);
                children += 1;
            }

            match layer {
                Layer::Group(g) => {
                    let pos = ordering.len();
                    ordering.push(0xffff); // placeholder for real child count,
                    ordering.push(g.metadata().id.try_into().unwrap());

                    // Insert source layer to target group (in into_group mode)
                    let mut gc = 0;
                    if into_group && !below && g.metadata().id == target_layer_id {
                        order_source(ordering, source);
                        gc += 1;
                    }
                    gc += order(ordering, g, source, target_layer_id, into_group, below);
                    if into_group && below && g.metadata().id == target_layer_id {
                        order_source(ordering, source);
                        gc += 1;
                    }
                    ordering[pos] = gc;
                }

                Layer::Bitmap(l) => {
                    ordering.push(0);
                    ordering.push(l.metadata().id.try_into().unwrap());
                }
            }

            // Append source layer if not inserting to group
            if !into_group && below && layer.metadata().id == target_layer_id {
                order_source(ordering, source);
                children += 1;
            }
        }
        children
    }

    let mut ordering = Vec::new();
    order(&mut ordering, root.inner_ref(), source_layer, target_layer_id, into_group, below);
    Some(ordering)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::paint::{RootGroup, LayerInsertion, Color};

    #[test]
    fn test_reordering_flat() {
        let mut root = RootGroup::new(64, 64);
        root.add_bitmap_layer(1, Color::TRANSPARENT, LayerInsertion::Top);
        root.add_bitmap_layer(2, Color::TRANSPARENT, LayerInsertion::Top);
        root.add_bitmap_layer(3, Color::TRANSPARENT, LayerInsertion::Top);

        assert_eq!(
            move_ordering(&root, 0, 1, false, false),
            None
        );

        assert_eq!(
            move_ordering(&root, 1, 1, false, false),
            None
        );

        #[rustfmt::skip] // move bottom-most to the top
        assert_eq!(
            move_ordering(&root, 1, 3, false, false),
            Some(vec![
                0, 1,
                0, 3,
                0, 2
            ])
        );

        #[rustfmt::skip] // move middle to the bottom
        assert_eq!(
            move_ordering(&root, 2, 1, false, true),
            Some(vec![
                0, 3,
                0, 1,
                0, 2
            ])
        );
    }

    #[test]
    fn test_reordering_group() {
        let mut root = RootGroup::new(64, 64);
        root.add_bitmap_layer(1, Color::TRANSPARENT, LayerInsertion::Top);
        root.add_bitmap_layer(2, Color::TRANSPARENT, LayerInsertion::Top);
        root.add_group_layer(3, LayerInsertion::Top);
        root.add_bitmap_layer(4, Color::TRANSPARENT, LayerInsertion::Into(3));
        root.add_bitmap_layer(5, Color::TRANSPARENT, LayerInsertion::Into(3));
        root.add_bitmap_layer(6, Color::TRANSPARENT, LayerInsertion::Top);

        // Original ordering
        // 0, 6
        // 2, 3
        //   0, 5
        //   0, 4
        // 0, 2
        // 0, 1

        #[rustfmt::skip] // move group to the top
        assert_eq!(
            move_ordering(&root, 3, 6, false, false),
            Some(vec![
                2, 3,
                  0, 5,
                  0, 4,
                0, 6,
                0, 2,
                0, 1,
            ])
        );

        #[rustfmt::skip] // bottom layer above the group
        assert_eq!(
            move_ordering(&root, 1, 3, false, false),
            Some(vec![
                0, 6,
                0, 1,
                2, 3,
                  0, 5,
                  0, 4,
                0, 2,
            ])
        );

        #[rustfmt::skip] // bottom layer into the group
        assert_eq!(
            move_ordering(&root, 1, 3, true, false),
            Some(vec![
                0, 6,
                3, 3,
                  0, 1,
                  0, 5,
                  0, 4,
                0, 2,
            ])
        );

        #[rustfmt::skip] // bottom layer into the bottom of the group group
        assert_eq!(
            move_ordering(&root, 1, 3, true, true),
            Some(vec![
                0, 6,
                3, 3,
                  0, 5,
                  0, 4,
                  0, 1,
                0, 2,
            ])
        );

        #[rustfmt::skip] // bottom layer above a layer in the group
        assert_eq!(
            move_ordering(&root, 1, 4, false,  false),
            Some(vec![
                0, 6,
                3, 3,
                  0, 5,
                  0, 1,
                  0, 4,
                0, 2,
            ])
        );

        #[rustfmt::skip] // bottom group layer to below the group
        assert_eq!(
            move_ordering(&root, 4, 3, false,  true),
            Some(vec![
                0, 6,
                1, 3,
                  0, 5,
                0, 4,
                0, 2,
                0, 1,
            ])
        );
    }
}