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

#ifndef CANVAS_ANNOTATION_STATE_H
#define CANVAS_ANNOTATION_STATE_H

#include "../shared/net/message.h"

#include <QObject>
#include <QRect>
#include <QColor>

class QPainter;
class QImage;
class QDataStream;

namespace canvas {

struct Annotation {
	int id;
	QString text;
	QRect rect;
	QColor background;

	enum Handle {OUTSIDE, TRANSLATE, RS_TOPLEFT, RS_TOPRIGHT, RS_BOTTOMRIGHT, RS_BOTTOMLEFT, RS_TOP, RS_RIGHT, RS_BOTTOM, RS_LEFT};
	static const int HANDLE_SIZE = 10;

	// TODO this needs to be HTML aware
	bool isEmpty() const { return text.isEmpty(); }

	void paint(QPainter *painter) const;
	void paint(QPainter *painter, const QRectF &paintrect) const;
	QImage toImage() const;

	//! Get the translation handle at the point
	Handle handleAt(const QPoint &point, qreal zoom) const;

	//! Adjust annotation position or size
	Handle adjustGeometry(Handle handle, const QPoint &delta);

	void toDataStream(QDataStream &out) const;
	static Annotation fromDataStream(QDataStream &in);
};

/**
 * @brief List of annotations maintained by the StateTracker
 *
 * This is the annotation state that lives in the same thread as the state
 * tracker. For use in the UI, the annotation list is replicated in the AnnotationModel
 * that lives in the main GUI thread.
 */
class AnnotationState : public QObject {
	Q_OBJECT
public:
	AnnotationState(QObject *parent=nullptr);

	void handleAnnotationCommand(protocol::MessagePtr msg);

	QList<Annotation> getAnnotations() const { return m_annotations; }
	void setAnnotations(const QList<Annotation> &annotations);

public slots:
	//! Translate every annotation (this is used to fix positions after canvas resize)
	void offsetAll(int x, int y);

signals:
	void annotationChanged(const Annotation &annotation);
	void annotationDeleted(int id);
	void annotationsReset(const QList<Annotation> &annotations);

private:
	void addAnnotation(const Annotation &annotation);
	void deleteAnnotation(int id);
	void reshapeAnnotation(int id, const QRect &newrect);
	void changeAnnotation(int id, const QString &newtext, const QColor &bgcolor);

	int findById(int id) const;

	QList<Annotation> m_annotations;
};

}

Q_DECLARE_METATYPE(canvas::Annotation)


#endif

