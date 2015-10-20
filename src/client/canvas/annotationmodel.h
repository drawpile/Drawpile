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
#ifndef PAINTCORE_ANNOTATION_MODEL_H
#define PAINTCORE_ANNOTATION_MODEL_H

#include "annotationstate.h"
#include "../shared/net/message.h"


#include <QAbstractListModel>
#include <QColor>
#include <QRect>

namespace canvas {

class AnnotationModel : public QAbstractListModel {
	Q_OBJECT
public:
	enum AnnotationRoles {
		// DisplayRole is used to get the text
		IdRole = Qt::UserRole + 1,
		RectRole,
		BgColorRole // avoid clash with Qt's own BackgroundColorRole
	};

	explicit AnnotationModel(QObject *parent=nullptr);

	int rowCount(const QModelIndex &parent=QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const;

	QHash<int, QByteArray> roleNames() const;

	Annotation::Handle annotationHandleAt(int id, const QPoint &point, qreal zoom) const;
	Annotation::Handle annotationAdjustGeometry(int id, Annotation::Handle handle, const QPoint &delta);

	bool isEmpty() const { return m_annotations.isEmpty(); }
	QList<Annotation> getAnnotations() const { return m_annotations; }
	int annotationAtPos(const QPoint &pos, qreal zoom) const;
	const Annotation *getById(int id) const;

	//! Return the IDs of annotations that have no text content
	QList<int> getEmptyIds() const;

public slots:
	void setAnnotations(const QList<Annotation> &list);
	void changeAnnotation(const Annotation &annotation);
	void deleteAnnotation(int id);

private:
	int findById(int id) const;

	QList<Annotation> m_annotations;
};

}

#endif

