/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/
#ifndef DP_PROTO_ANNOTATION_H
#define DP_PROTO_ANNOTATION_H

namespace protocol {

//! An annotation message
/**
 * This message type describes an annotation to the image.
 */
class Annotation {
	public:
		enum {LEFT, RIGHT, CENTER, FILL};
		Annotation() : id(0), textcolor("#000"), textalpha(255),
		backgroundcolor("#fff"), bgalpha(0),
		justify(LEFT), bold(false), italic(false), size(20) { }

		int id;
		QRect rect;
		QString text;
		QString textcolor;
		uchar textalpha;
		QString backgroundcolor;
		uchar bgalpha;
		int justify;
		bool bold;
		bool italic;
		QString font;
		int size;
};

}

#endif

