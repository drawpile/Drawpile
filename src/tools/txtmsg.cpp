/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "txtmsg.h"
#include "../shared/net/message.h"
#include "../shared/net/layer.h"
#include "../shared/net/image.h"
#include "../shared/net/pen.h"
#include "../shared/net/annotation.h"
#include "../shared/net/undo.h"
#include "../shared/net/meta.h"
#include "../shared/net/meta2.h"
#include "../shared/net/recording.h"

#include "../client/core/blendmodes.h"

#include <QTextStream>

using namespace protocol;

static inline QString COLOR(ulong c) {
	return QStringLiteral("#%1").arg(c, 8, 16, QChar('0'));
}

void userJoinTxt(const UserJoin *msg, QTextStream &out)
{
	out << "# META: joinuser " << msg->contextId() << " " << msg->name() << "\n";
}

void userLeaveTxt(const UserLeave *msg, QTextStream &out)
{
	out << "# META: userleave " << msg->contextId() << "\n";
}

void sessionOwnerTxt(const SessionOwner *msg, QTextStream &out)
{
	out << "# META: SessionOwner: ";
	for(uint8_t id : msg->ids())
		out << id << ", ";

	out << "\n";
}

void chatTxt(const Chat *msg, QTextStream &out)
{
	for(const QString line : msg->message().split('\n'))
		out << "# META chat " << msg->contextId() << " " << line << "\n";
}

void userAclTxt(const UserACL *msg, QTextStream &out)
{
	out << "# META: UserACL: ";
	for(uint8_t id : msg->ids())
		out << id << ", ";

	out << "\n";
}

void layerAclTxt(const LayerACL *msg, QTextStream &out)
{
	out << "# META: layeracl " << msg->contextId()
		<< " layer " << msg->id()
		<< " locked=" << (msg->locked() ? "true" : "false")
		<< " exclusive=";
	for(uint8_t u : msg->exclusive())
		out << u << " ";

	out << "\n";
}

void sessionAclTxt(const SessionACL *msg, QTextStream &out)
{
	out << "# META: SessionACL flags:";
	if(msg->isSessionLocked())
		out << " LOCK_SESSION";
	if(msg->isLockedByDefault())
		out << " LOCK_DEFAULT";
	if(msg->isLayerControlLocked())
		out << " LOCK_LAYERCTRL";
	if(msg->isOwnLayers())
		out << " LOCK_OWNLAYERS";
	if(msg->isImagesLocked())
		out << " LOCK_IMAGES";
	out << "\n";
}

void intervalTxt(const Interval *msg, QTextStream &out)
{
	out << "# META: interval " << msg->milliseconds() << " ms.\n";
}

void laserTrailTxt(const LaserTrail *msg, QTextStream &out)
{
	out << "# META: LaserTrail "
		<< msg->contextId()
		<< " color: " << COLOR(msg->color())
		<< " persistence: " << msg->persistence()
		<< "\n";
}

void movePointerTxt(const MovePointer *msg, QTextStream &out)
{
	out << "# META: MovePointer "
		<< msg->contextId()
		<< " " << msg->x()
		<< " " << msg->y()
		<< "\n";
}

void markerTxt(const Marker *msg, QTextStream &out)
{
	for(const QString line : msg->text().split('\n'))
		out << "# META marker[" << msg->contextId() << "]: " << line << "\n";
}

void canvasResizeTxt(const CanvasResize *msg, QTextStream &out)
{
	out << "resize "
		<<	msg->contextId()
		<< " " << msg->top()
		<< " " << msg->right()
		<< " " << msg->bottom()
		<< " " << msg->left()
		<< "\n";
}

void layerCreateTxt(const LayerCreate *msg, QTextStream &out)
{
	out << "newlayer "
		<< msg->contextId()
		<< " " << msg->id()
		<< " " << msg->source()
		<< " " << COLOR	(msg->fill());
	if((msg->flags() & LayerCreate::FLAG_COPY))
		out << " copy";
	if((msg->flags() & LayerCreate::FLAG_INSERT))
		out << " insert";
	out << " " << msg->title()
		<< "\n";
}

void layerAttrTxt(const LayerAttributes *msg, QTextStream &out)
{
	out << "layerattr "
		<< msg->contextId()
		<< " " << msg->id()
		<< " " << "opacity=" << (msg->opacity() / 255.0)
		<< " " << "blend=" << paintcore::findBlendMode(msg->blend()).svgname
		<< "\n";
}

void layerRetitleTxt(const LayerRetitle *msg, QTextStream &out)
{
	out << "retitlelayer "
		<< msg->contextId()
		<< " " << msg->id()
		<< " " << msg->title()
		<< "\n";
}

void layerOrderTxt(const LayerOrder *msg, QTextStream &out)
{
	out << "reorderlayers " << msg->contextId();
	for(uint8_t id : msg->order())
		out << " " << id;
	out << "\n";
}

void layerDeleteTxt(const LayerDelete *msg, QTextStream &out)
{
	out << "deletelayer "
		<< msg->contextId()
		<< " " << msg->id();
	if( msg->merge())
		out << " merge";
	out << "\n";
}

void layerVisibilityTxt(const LayerVisibility *msg, QTextStream &out)
{
	out << "LayerVisibility "
		<< msg->contextId()
		<< " " << msg->id()
		<< " " << (msg->visible() ? "visible" : "hidden")
		<< "\n";
}

void putImageTxt(const PutImage *msg, QTextStream &out)
{
	out << "inlineimage " << msg->width() << " " << msg->height() << "\n";

	QByteArray imagedata = msg->image().toBase64();
	const int STEP=64;
	for(int i=0;i<imagedata.length();i+=STEP) {
		out << imagedata.mid(i, STEP);
		out << "\n";
	}
	out << "==end==\n";
	out << "putimage "
		<< msg->contextId()
		<< " " << msg->layer()
		<< " " << msg->x()
		<< " " << msg->y()
		<< " " << paintcore::findBlendMode(msg->blendmode()).svgname
		<< " -\n";
}

void fillRectTxt(const FillRect *msg, QTextStream &out)
{
	out << "fillrect "
		<< msg->contextId()
		<< " " << msg->layer()
		<< " " << msg->x()
		<< " " << msg->y()
		<< " " << msg->width()
		<< " " << msg->height()
		<< " " << COLOR(msg->color())
		;
	if(msg->blend() != 255)
		out << " " << paintcore::findBlendMode(msg->blend()).svgname;
	out << "\n";
}

void toolChangeTxt(const ToolChange *msg, QTextStream &out)
{
	out << "ctx "
		<< msg->contextId()
		<< " layer=" << msg->layer()
		<< " blend=" << paintcore::findBlendMode(msg->blend()).svgname
		<< " hardedge=" << (msg->mode() & TOOL_MODE_SUBPIXEL ? "false" : "true")
		<< " incremental=" << (msg->mode() & TOOL_MODE_INCREMENTAL ? "true" : "false")
		<< " spacing=" << msg->spacing()
		<< " sizeh=" << msg->size_h()
		<< " sizel=" << msg->size_l()
		<< " resmudge=" << msg->resmudge()
		<< " color=" << COLOR(msg->color());

	if(msg->hard_h() == msg->hard_l())
		out << " hard=" << (msg->hard_h() / 255.0);
	else
		out << " hardh=" << (msg->hard_h() / 255.0) << " hardl=" << (msg->hard_l() / 255.0);

	if(msg->opacity_h() == msg->opacity_l())
		out << " opacity=" << (msg->opacity_h() / 255.0);
	else
		out << " opacityh=" << (msg->opacity_h() / 255.0) << " opacityl=" << (msg->opacity_l() / 255.0);

	if(msg->smudge_h() == msg->smudge_l())
		out << " smudge=" << (msg->smudge_h() / 255.0);
	else
		out << " smudgeh=" << (msg->smudge_h() / 255.0) << " smudgel=" << (msg->smudge_l() / 255.0);

	out << "\n";
}

void penMoveTxt(const PenMove *msg, QTextStream &out)
{
	out << "move " << msg->contextId();

	for(const PenPoint &p : msg->points()) {
		out
			<< " " << p.x / 4.0
			<< " " << p.y / 4.0;
		if(p.p < 0xffff)
			out << " " << p.p / double(0xffff);
		out << ";";
	}
	out << "\n";
}

void penUpTxt(const PenUp *msg, QTextStream &out)
{
	out << "penup " << msg->contextId() << "\n";
}

void annotationCreateTxt(const AnnotationCreate *msg, QTextStream &out)
{
	out << "addannotation "
		<< msg->contextId()
		<< " " << msg->id()
		<< " " << msg->x()
		<< " " << msg->y()
		<< " " << msg->w()
		<< " " << msg->h()
		<< "\n";
}

void annotationReshapeTxt(const AnnotationReshape *msg, QTextStream &out)
{
	out << "reshapeannotation "
		<< msg->contextId()
		<< " " << msg->id()
		<< " " << msg->x()
		<< " " << msg->y()
		<< " " << msg->w()
		<< " " << msg->h()
		<< "\n";
}

void annotationEditTxt(const AnnotationEdit *msg, QTextStream &out)
{
	out << "annotate "
		<< msg->contextId()
		<< " " << msg->id()
		<< " " << COLOR(msg->bg())
		<< "\n";

	for(const QString &line : msg->text().split('\n')) {
		if(line == "endannotate")
			out << "(endannotate)\n";
		else
			out << line << "\n";
	}
	out << "endannotate\n";
}

void annotationDeleteTxt(const AnnotationDelete *msg, QTextStream &out)
{
	out << "deleteannotation "
		<< msg->contextId()
		<< " " << msg->id()
		<< "\n";
}

void undoPointTxt(const UndoPoint *msg, QTextStream &out)
{
	out << "undopoint " << msg->contextId() << "\n";
}

void undoTxt(const Undo *msg, QTextStream &out)
{
	if(msg->isRedo())
		out << "redo " << msg->contextId() << "\n";
	else
		out << "undo " << msg->contextId() << "\n";
}

void messageToText(const Message *msg, QTextStream &out)
{
	switch(msg->type()) {
	// Meta messages
	case MSG_USER_JOIN: userJoinTxt(static_cast<const UserJoin*>(msg), out); break;
	case MSG_USER_LEAVE: userLeaveTxt(static_cast<const UserLeave*>(msg), out); break;
	case MSG_SESSION_OWNER: sessionOwnerTxt(static_cast<const SessionOwner*>(msg), out); break;
	case MSG_CHAT: chatTxt(static_cast<const Chat*>(msg), out); break;

	case MSG_INTERVAL: intervalTxt(static_cast<const Interval*>(msg), out); break;
	case MSG_LASERTRAIL: laserTrailTxt(static_cast<const LaserTrail*>(msg), out); break;
	case MSG_MOVEPOINTER: movePointerTxt(static_cast<const MovePointer*>(msg), out); break;
	case MSG_MARKER: markerTxt(static_cast<const Marker*>(msg), out); break;
	case MSG_USER_ACL: userAclTxt(static_cast<const UserACL*>(msg), out); break;
	case MSG_LAYER_ACL: layerAclTxt(static_cast<const LayerACL*>(msg), out); break;

	// Command messages
	case MSG_UNDOPOINT: undoPointTxt(static_cast<const UndoPoint*>(msg), out); break;
	case MSG_CANVAS_RESIZE: canvasResizeTxt(static_cast<const CanvasResize*>(msg), out); break;
	case MSG_LAYER_CREATE: layerCreateTxt(static_cast<const LayerCreate*>(msg), out); break;
	case MSG_LAYER_ATTR: layerAttrTxt(static_cast<const LayerAttributes*>(msg), out); break;
	case MSG_LAYER_RETITLE: layerRetitleTxt(static_cast<const LayerRetitle*>(msg), out); break;
	case MSG_LAYER_ORDER: layerOrderTxt(static_cast<const LayerOrder*>(msg), out); break;
	case MSG_LAYER_DELETE: layerDeleteTxt(static_cast<const LayerDelete*>(msg), out); break;
	case MSG_LAYER_VISIBILITY: layerVisibilityTxt(static_cast<const LayerVisibility*>(msg), out); break;

	case MSG_PUTIMAGE: putImageTxt(static_cast<const PutImage*>(msg), out); break;
	case MSG_FILLRECT:fillRectTxt(static_cast<const FillRect*>(msg), out); break;

	case MSG_TOOLCHANGE: toolChangeTxt(static_cast<const ToolChange*>(msg), out); break;
	case MSG_PEN_MOVE: penMoveTxt(static_cast<const PenMove*>(msg), out); break;
	case MSG_PEN_UP: penUpTxt(static_cast<const PenUp*>(msg), out); break;

	case MSG_ANNOTATION_CREATE: annotationCreateTxt(static_cast<const AnnotationCreate*>(msg), out); break;
	case MSG_ANNOTATION_RESHAPE: annotationReshapeTxt(static_cast<const AnnotationReshape*>(msg), out); break;
	case MSG_ANNOTATION_EDIT: annotationEditTxt(static_cast<const AnnotationEdit*>(msg), out); break;
	case MSG_ANNOTATION_DELETE:annotationDeleteTxt(static_cast<const AnnotationDelete*>(msg), out); break;

	case MSG_UNDO: undoTxt(static_cast<const Undo*>(msg), out); break;

	default:
		out << "# WARNING: Unhandled " << (msg->isCommand() ? "command" : "meta") << " message type " << msg->type() << "\n";
	}
}
