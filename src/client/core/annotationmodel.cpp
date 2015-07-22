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

#include <QTextDocument>
#include <QPainter>
#include <QImage>
#include <QSet> // for old-style change notifications

namespace paintcore {

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

void AnnotationModel::addAnnotation(const Annotation &annotation)
{
	// Make sure ID is unique
	if(findById(annotation.id)>=0) {
		qWarning("Cannot add annotation: ID (%d) not unique!", annotation.id);
		return;
	}

	beginInsertRows(QModelIndex(), m_annotations.size(), m_annotations.size());
	m_annotations.append(annotation);
	endInsertRows();
	emit annotationChanged(annotation.id);
}

void AnnotationModel::addAnnotation(int id, const QRect &rect)
{
	addAnnotation(Annotation {id, QString(), rect, QColor(Qt::transparent)});
}

void AnnotationModel::deleteAnnotation(int id)
{
	int idx = findById(id);
	if(idx<0) {
		qWarning("Cannot remove annotation: ID %d not found!", id);
		return;
	}

	beginRemoveRows(QModelIndex(), idx, idx);
	m_annotations.removeAt(idx);
	endRemoveRows();
	emit annotationChanged(id);
}

void AnnotationModel::reshapeAnnotation(int id, const QRect &newrect)
{
	int idx = findById(id);
	if(idx<0) {
		qWarning("Cannot reshape annotation: ID %d not found!", id);
		return;
	}

	m_annotations[idx].rect = newrect;
	emit dataChanged(index(idx), index(idx), QVector<int>() << RectRole);
	emit annotationChanged(id);
}

void AnnotationModel::changeAnnotation(int id, const QString &newtext, const QColor &bgcolor)
{
	int idx = findById(id);
	if(idx<0) {
		qWarning("Cannot change annotation: ID %d not found!", id);
		return;
	}
	m_annotations[idx].text = newtext;
	m_annotations[idx].background = bgcolor;

	emit dataChanged(index(idx), index(idx), QVector<int>() << Qt::DisplayRole << BgColorRole);
	emit annotationChanged(id);
}

void AnnotationModel::setAnnotations(const QList<Annotation> &annotations)
{
	// Old-style notification. To be removed once migration
	// away from QGraphicsScene is complete.
	QSet<int> ids;
	for(const Annotation &a : m_annotations)
		ids.insert(a.id);
	for(const Annotation &a : annotations)
		ids.insert(a.id);

	beginResetModel();
	m_annotations = annotations;
	endResetModel();

	for(int id : ids)
		emit annotationChanged(id);
}

const Annotation *AnnotationModel::getById(int id) const
{
	for(const Annotation &a : m_annotations)
		if(a.id == id)
			return &a;
	return nullptr;
}
Annotation *AnnotationModel::getById(int id)
{
	for(Annotation &a : m_annotations)
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

