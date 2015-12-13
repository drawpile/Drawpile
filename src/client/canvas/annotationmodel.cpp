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
#include "annotationmodel.h"
#include "../shared/net/annotation.h"

#include <QTextDocument>
#include <QPainter>
#include <QImage>
#include <QSet> // for old-style change notifications

namespace canvas {

AnnotationModel::AnnotationModel(QObject *parent)
	: QAbstractListModel(parent)
{
}

int AnnotationModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_annotations.size();
}

QVariant AnnotationModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && index.row() >= 0 && index.row() < m_annotations.size()) {
		const Annotation &a = m_annotations.at(index.row());
		switch(role) {
			case Qt::DisplayRole: return a.text;
			case IdRole: return a.id;
			case RectRole: return a.rect;
			case BgColorRole: return a.background;
			default: break;
		}
	}
	return QVariant();
}

QHash<int, QByteArray> AnnotationModel::roleNames() const
{
	QHash<int, QByteArray> roles;
	roles[Qt::DisplayRole] = "display";
	roles[IdRole] = "annotationId";
	roles[RectRole] = "rect";
	roles[BgColorRole] = "background";
	return roles;
}

void AnnotationModel::deleteAnnotation(int id)
{
	int idx = findById(id);
	if(idx<0) {
		qWarning("Cannot remove annotation #%d from model: not found!", id);
		return;
	}

	beginRemoveRows(QModelIndex(), idx, idx);
	m_annotations.removeAt(idx);
	endRemoveRows();
}

void AnnotationModel::changeAnnotation(const Annotation &annotation)
{
	int idx = findById(annotation.id);
	if(idx<0) {
		// Annotation does not yet exist: create it now
		beginInsertRows(QModelIndex(), m_annotations.size(), m_annotations.size());
		m_annotations.append(annotation);
		endInsertRows();
	} else {
		// Update an existing annotation
		m_annotations[idx] = annotation;

		emit dataChanged(index(idx), index(idx), QVector<int>() << RectRole << Qt::DisplayRole << BgColorRole);
	}
}

void AnnotationModel::setAnnotations(const QList<Annotation> &annotations)
{
	beginResetModel();
	m_annotations = annotations;
	endResetModel();
}

const Annotation *AnnotationModel::getById(int id) const
{
	for(const Annotation &a : m_annotations)
		if(a.id == id)
			return &a;
	return nullptr;
}

int AnnotationModel::findById(int id) const
{
	for(int i=0;i<m_annotations.size();++i)
		if(m_annotations.at(i).id == id)
			return i;
	return -1;
}

/**
 * @brief Find the annotation at the given coordinates.
 * @param pos point in canvas coordinates
 * @return annotation ID or 0 if none found
 */
int AnnotationModel::annotationAtPos(const QPoint &pos, qreal zoom) const
{
	const int H = qRound(qMax(qreal(Annotation::HANDLE_SIZE), Annotation::HANDLE_SIZE / zoom) / 2.0);
	for(const Annotation &a : m_annotations) {
		if(a.rect.adjusted(-H, -H, H, H).contains(pos))
			return a.id;
	}
	return 0;
}

Annotation::Handle AnnotationModel::annotationHandleAt(int id, const QPoint &point, qreal zoom) const
{
	const Annotation *a = getById(id);
	if(a)
		return a->handleAt(point, zoom);
	return Annotation::OUTSIDE;
}

Annotation::Handle AnnotationModel::annotationAdjustGeometry(int id, Annotation::Handle handle, const QPoint &delta)
{
	for(int idx=0;idx<m_annotations.size();++idx) {
		if(m_annotations.at(idx).id == id) {
			handle = m_annotations[idx].adjustGeometry(handle, delta);
			emit dataChanged(index(idx), index(idx), QVector<int>() << RectRole);
			return handle;
		}
	}
	return Annotation::OUTSIDE;
}

QList<int> AnnotationModel::getEmptyIds() const
{
	QList<int> ids;
	for(const Annotation &a : m_annotations) {
		if(a.isEmpty())
			ids << a.id;
	}
	return ids;
}

}

