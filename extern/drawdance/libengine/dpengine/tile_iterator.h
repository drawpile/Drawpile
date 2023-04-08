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
#ifndef DPENGINE_TILE_ITERATOR_H
#define DPENGINE_TILE_ITERATOR_H
#include "tile.h"
#include <dpcommon/common.h>
#include <dpcommon/geom.h>


typedef struct DP_TileIterator {
    DP_Rect canvas;    // canvas rectangle, from (0, 0) to (width, height)
    DP_Rect dst;       // area to iterate, might exceed canvas in any direction
    DP_Rect area;      // intersection of canvas and dst
    DP_Rect tile_area; // affected tiles in tile space, not pixel space
    int col, row;      // iterator variables in tile space, not pixel space
} DP_TileIterator;

DP_TileIterator DP_tile_iterator_make(int canvas_width, int canvas_height,
                                      DP_Rect dst);

bool DP_tile_iterator_next(DP_TileIterator *ti);


typedef struct DP_TileIntoDstIterator {
    DP_Rect tile_bounds; // area within tile to iterate
    DP_Rect dst_bounds;  // area within dst to iterate
    int tile_x, tile_y;  // iterator variables, current pixel in tile
    int dst_x, dst_y;    // iterator variables, current pixel in dst
} DP_TileIntoDstIterator;

DP_TileIntoDstIterator DP_tile_into_dst_iterator_make(DP_TileIterator *ti);

bool DP_tile_into_dst_iterator_next(DP_TileIntoDstIterator *tidi);


#endif
