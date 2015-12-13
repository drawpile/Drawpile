/*
  Drawpile - a collaborative drawing program.

  Copyright (C) 2015 Calle Laakkonen

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

#include "annotationstate.h"
#include "../shared/net/annotation.h"

#include <QPainter>
#include <QImage>
#include <QDataStream>
#include <QTextDocument>

namespace canvas {

AnnotationState::AnnotationState(QObject *parent)
	: QObject(parent)
{
}

void AnnotationState::offsetAll(int x, int y)
{
	if(x || y) {
		QPoint offset(x,y);
		for(const Annotation &a : m_annotations) {
			reshapeAnnotation(a.id, a.rect.translated(offset));
		}
	}
}

void AnnotationState::handleAnnotationCommand(protocol::MessagePtr msg)
{
	switch(msg->type()) {
	case protocol::MSG_ANNOTATION_CREATE: {
		const auto &cmd = msg.cast<const protocol::AnnotationCreate>();
		addAnnotation(Annotation { cmd.id(), QString(), QRect(cmd.x(), cmd.y(), cmd.w(), cmd.h()), QColor(Qt::transparent) });
		break;
	}
	case protocol::MSG_ANNOTATION_RESHAPE: {
			const auto &cmd = msg.cast<const protocol::AnnotationReshape>();
			reshapeAnnotation(cmd.id(), QRect(cmd.x(), cmd.y(), cmd.w(), cmd.h()));
			break;
	}
	case protocol::MSG_ANNOTATION_EDIT: {
			const auto &cmd = msg.cast<const protocol::AnnotationEdit>();
			changeAnnotation(cmd.id(), cmd.text(), QColor::fromRgba(cmd.bg()));
			break;
	}
	case protocol::MSG_ANNOTATION_DELETE: {
			const auto &cmd = msg.cast<const protocol::AnnotationDelete>();
			deleteAnnotation(cmd.id());
			break;
	}
	default: qFatal("unhandled annotation command: %d", msg->type());
	}
}

void AnnotationState::setAnnotations(const QList<Annotation> &annotations)
{
	m_annotations = annotations;
	emit annotationsReset(annotations);
}

void AnnotationState::addAnnotation(const Annotation &annotation)
{
	// Make sure ID is unique
	if(findById(annotation.id)>=0) {
		qWarning("Cannot add annotation: ID (%d) not unique!", annotation.id);
		return;
	}

	m_annotations.append(annotation);
	emit annotationChanged(annotation);
}

void AnnotationState::deleteAnnotation(int id)
{
	int idx = findById(id);
	if(idx<0) {
		qWarning("Cannot remove annotation: ID %d not found!", id);
		return;
	}

	m_annotations.removeAt(idx);
	emit annotationDeleted(id);
}

void AnnotationState::reshapeAnnotation(int id, const QRect &newrect)
{
	int idx = findById(id);
	if(idx<0) {
		qWarning("Cannot reshape annotation: ID %d not found!", id);
		return;
	}

	m_annotations[idx].rect = newrect;
	emit annotationChanged(m_annotations[idx]);
}

void AnnotationState::changeAnnotation(int id, const QString &newtext, const QColor &bgcolor)
{
	int idx = findById(id);
	if(idx<0) {
		qWarning("Cannot change annotation: ID %d not found!", id);
		return;
	}
	m_annotations[idx].text = newtext;
	m_annotations[idx].background = bgcolor;
	emit annotationChanged(m_annotations[idx]);
}

int AnnotationState::findById(int id) const
{
	for(int i=0;i<m_annotations.size();++i)
		if(m_annotations.at(i).id == id)
			return i;
	return -1;
}

void Annotation::paint(QPainter *painter) const
{
	paint(painter, rect);
}

void Annotation::paint(QPainter *painter, const QRectF &paintrect) const
{
	painter->save();
	painter->translate(paintrect.topLeft());

	const QRectF rect0(QPointF(), paintrect.size());

	painter->fillRect(rect0, background);

	QTextDocument doc;
	doc.setHtml(text);
	doc.setTextWidth(rect0.width());
	doc.drawContents(painter, rect0);

	painter->restore();
}

QImage Annotation::toImage() const
{
	QImage img(rect.size(), QImage::Format_ARGB32);
	img.fill(0);
	QPainter painter(&img);
	paint(&painter, QRectF(0, 0, rect.width(), rect.height()));
	return img;
}

/**
 * Note. Assumes point is inside the text box.
 */
Annotation::Handle Annotation::handleAt(const QPoint &point, qreal zoom) const
{
	const qreal H = qMax(qreal(HANDLE_SIZE), HANDLE_SIZE / zoom);

	const QRectF R = QRectF(rect.x()-H/2, rect.y()-H/2, rect.width()+H, rect.height()+H);

	if(!R.contains(point))
		return OUTSIDE;

	QPointF p = point - R.topLeft();

	if(p.x() < H) {
		if(p.y() < H)
			return RS_TOPLEFT;
		else if(p.y() > R.height()-H)
			return RS_BOTTOMLEFT;
		return RS_LEFT;
	} else if(p.x() > R.width() - H) {
		if(p.y() < H)
			return RS_TOPRIGHT;
		else if(p.y() > R.height()-H)
			return RS_BOTTOMRIGHT;
		return RS_RIGHT;
	} else if(p.y() < H)
		return RS_TOP;
	else if(p.y() > R.height()-H)
		return RS_BOTTOM;

	return TRANSLATE;
}

Annotation::Handle Annotation::adjustGeometry(Handle handle, const QPoint &delta)
{
	switch(handle) {
	case OUTSIDE: return handle;
	case TRANSLATE: rect.translate(delta); break;
	case RS_TOPLEFT: rect.adjust(delta.x(), delta.y(), 0, 0); break;
	case RS_TOPRIGHT: rect.adjust(0, delta.y(), delta.x(), 0); break;
	case RS_BOTTOMRIGHT: rect.adjust(0, 0, delta.x(), delta.y()); break;
	case RS_BOTTOMLEFT: rect.adjust(delta.x(), 0, 0, delta.y()); break;
	case RS_TOP: rect.adjust(0, delta.y(), 0, 0); break;
	case RS_RIGHT: rect.adjust(0, 0, delta.x(), 0); break;
	case RS_BOTTOM: rect.adjust(0, 0, 0, delta.y()); break;
	case RS_LEFT: rect.adjust(delta.x(), 0, 0, 0); break;
	}

	if(rect.left() > rect.right() || rect.top() > rect.bottom()) {

		if(rect.left() > rect.right()) {
			switch(handle) {
				case RS_TOPLEFT: handle = RS_TOPRIGHT; break;
				case RS_TOPRIGHT: handle = RS_TOPLEFT; break;
				case RS_BOTTOMRIGHT: handle = RS_BOTTOMLEFT; break;
				case RS_BOTTOMLEFT: handle = RS_BOTTOMRIGHT; break;
				case RS_LEFT: handle = RS_RIGHT; break;
				case RS_RIGHT: handle = RS_LEFT; break;
				default: break;
			}
		}
		if(rect.top() > rect.bottom()) {
			switch(handle) {
				case RS_TOPLEFT: handle = RS_BOTTOMLEFT; break;
				case RS_TOPRIGHT: handle = RS_BOTTOMRIGHT; break;
				case RS_BOTTOMRIGHT: handle = RS_TOPRIGHT; break;
				case RS_BOTTOMLEFT: handle = RS_TOPRIGHT; break;
				case RS_TOP: handle = RS_BOTTOM; break;
				case RS_BOTTOM: handle = RS_TOP; break;
				default: break;
			}
		}

		rect = rect.normalized();
	}

	return handle;
}

void Annotation::toDataStream(QDataStream &out) const
{
	// Write ID
	out << quint16(id);

	// Write position and size
	out << rect;

	// Write content
	out << background;
	out << text;
}

Annotation Annotation::fromDataStream(QDataStream &in)
{
	quint16 id;
	in >> id;

	QRect rect;
	in >> rect;

	QColor color;
	in >> color;

	QString text;
	in >> text;
	return Annotation {id, text, rect, color};
}

}

