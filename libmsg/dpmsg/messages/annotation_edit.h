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
#ifndef DPMSG_ANNOTATION_EDIT_H
#define DPMSG_ANNOTATION_EDIT_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


#define DP_MSG_ANNOTATION_EDIT_PROTECT       0x1
#define DP_MSG_ANNOTATION_EDIT_VALIGN_CENTER 0x2
#define DP_MSG_ANNOTATION_EDIT_VALIGN_BOTTOM 0x6
#define DP_MSG_ANNOTATION_EDIT_VALIGN_MASK \
    (DP_MSG_ANNOTATION_EDIT_VALIGN_CENTER  \
     | DP_MSG_ANNOTATION_EDIT_VALIGN_BOTTOM)


typedef struct DP_MsgAnnotationEdit DP_MsgAnnotationEdit;

DP_Message *DP_msg_annotation_edit_new(unsigned int context_id,
                                       int annotation_id,
                                       uint32_t background_color, int flags,
                                       int border, const char *text,
                                       size_t text_length);

DP_Message *DP_msg_annotation_edit_deserialize(unsigned int context_id,
                                               const unsigned char *buffer,
                                               size_t length);

DP_MsgAnnotationEdit *DP_msg_annotation_edit_cast(DP_Message *msg);


int DP_msg_annotation_edit_annotation_id(DP_MsgAnnotationEdit *mae);

uint32_t DP_msg_annotation_edit_background_color(DP_MsgAnnotationEdit *mae);

int DP_msg_annotation_edit_flags(DP_MsgAnnotationEdit *mae);

bool DP_msg_annotation_edit_protect(DP_MsgAnnotationEdit *mae);

int DP_msg_annotation_edit_valign(DP_MsgAnnotationEdit *mae);

bool DP_msg_annotation_edit_valign_center(DP_MsgAnnotationEdit *mae);

bool DP_msg_annotation_edit_valign_bottom(DP_MsgAnnotationEdit *mae);

int DP_msg_annotation_edit_border(DP_MsgAnnotationEdit *mae);

const char *DP_msg_annotation_edit_text(DP_MsgAnnotationEdit *mae,
                                        size_t *out_text_length);


#endif
