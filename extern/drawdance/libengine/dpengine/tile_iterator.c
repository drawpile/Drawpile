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
#include "tile_iterator.h"
#include "tile.h"
#include <dpcommon/common.h>
#include <dpcommon/geom.h>
#include <limits.h>


DP_TileIterator DP_tile_iterator_make(int canvas_width, int canvas_height,
                                      DP_Rect dst)
{
    DP_Rect canvas = DP_rect_make(0, 0, canvas_width, canvas_height);
    DP_Rect area = DP_rect_intersection(canvas, dst);
    if (DP_rect_valid(area)) {
        int x = DP_rect_x(area);
        int y = DP_rect_y(area);
        int col = x / DP_TILE_SIZE;
        int row = y / DP_TILE_SIZE;
        int xd = x - col * DP_TILE_SIZE;
        int yd = y - row * DP_TILE_SIZE;
        DP_Rect tile_area = DP_rect_make(
            col, row, DP_tile_size_round_up(DP_rect_width(area) + xd),
            DP_tile_size_round_up(DP_rect_height(area) + yd));
        return (DP_TileIterator){canvas,
                                 dst,
                                 area,
                                 tile_area,
                                 DP_rect_left(tile_area) - 1,
                                 DP_rect_top(tile_area)};
    }
    else {
        return (DP_TileIterator){
            canvas,  dst,
            area,    (DP_Rect){INT_MAX, INT_MAX, INT_MIN, INT_MIN},
            INT_MAX, INT_MAX};
    }
}

bool DP_tile_iterator_next(DP_TileIterator *ti)
{
    DP_ASSERT(ti);
    if (ti->col < DP_rect_right(ti->tile_area)) {
        ++ti->col;
        return true;
    }
    else if (ti->row < DP_rect_bottom(ti->tile_area)) {
        ti->col = DP_rect_left(ti->tile_area);
        ++ti->row;
        return true;
    }
    else {
        return false;
    }
}


DP_TileIntoDstIterator DP_tile_into_dst_iterator_make(DP_TileIterator *ti)
{
    DP_ASSERT(ti);
    int canvas_x = ti->col * DP_TILE_SIZE;
    int canvas_y = ti->row * DP_TILE_SIZE;
    DP_Rect tile_rect =
        DP_rect_make(canvas_x, canvas_y, DP_TILE_SIZE, DP_TILE_SIZE);
    DP_Rect canvas_bounds = DP_rect_intersection(ti->area, tile_rect);
    DP_Rect tile_bounds =
        DP_rect_translate(canvas_bounds, -canvas_x, -canvas_y);
    DP_Rect dst_bounds = DP_rect_translate(
        canvas_bounds, -DP_rect_left(ti->dst), -DP_rect_top(ti->dst));
    return (DP_TileIntoDstIterator){tile_bounds,
                                    dst_bounds,
                                    DP_rect_left(tile_bounds) - 1,
                                    DP_rect_top(tile_bounds),
                                    DP_rect_left(dst_bounds) - 1,
                                    DP_rect_top(dst_bounds)};
}

bool DP_tile_into_dst_iterator_next(DP_TileIntoDstIterator *tidi)
{
    DP_ASSERT(tidi);
    if (tidi->tile_x < DP_rect_right(tidi->tile_bounds)) {
        ++tidi->tile_x;
        ++tidi->dst_x;
        return true;
    }
    else if (tidi->tile_y < DP_rect_bottom(tidi->tile_bounds)) {
        tidi->tile_x = DP_rect_left(tidi->tile_bounds);
        tidi->dst_x = DP_rect_left(tidi->dst_bounds);
        ++tidi->tile_y;
        ++tidi->dst_y;
        return true;
    }
    else {
        return false;
    }
}
