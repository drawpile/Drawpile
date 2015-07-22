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

#include <QAbstractListModel>
#include <QColor>
#include <QRect>

class QDataStream;
class QPainter;
class QImage;

namespace paintcore {

struct Annotation {
	int id;
	QString text;
	QRect rect;
	QColor background;

	// TODO this needs to be HTML aware
	bool isEmpty() const { return text.isEmpty(); }

	void paint(QPainter *painter) const;
	void paint(QPainter *painter, const QRectF &paintrect) const;
	QImage toImage() const;

	void toDataStream(QDataStream &out) const;
	static Annotation fromDataStream(QDataStream &in);
};

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

	bool isEmpty() const { return m_annotations.isEmpty(); }

	void addAnnotation(const Annotation &annotation);
	void addAnnotation(int id, const QRect &rect);
	void deleteAnnotation(int id);
	void reshapeAnnotation(int id, const QRect &newrect);
	void changeAnnotation(int id, const QString &newtext, const QColor &bgcolor);

	void setAnnotations(const QList<Annotation> &list);
	QList<Annotation> getAnnotations() const { return m_annotations; }

	// Transitional: these won't be needed after the migration to QML is complete
	const Annotation *getById(int id) const;
	Annotation *getById(int id);

signals:
	// Transitional
	void annotationChanged(int id);

private:
	int findById(int id) const;

	QList<Annotation> m_annotations;
};

}

#endif

