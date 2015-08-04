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
#ifndef MATRIXTRANSFORM_H
#define MATRIXTRANSFORM_H

#include <QQuickItem>
#include <QVector3D>

class CanvasTransform : public QQuickTransform
{
	Q_PROPERTY(QVector3D origin READ origin	WRITE setOrigin NOTIFY originChanged)
	Q_PROPERTY(qreal translateX READ translateX WRITE setTranslateX NOTIFY xChanged)
	Q_PROPERTY(qreal translateY READ translateY WRITE setTranslateY NOTIFY yChanged)
	Q_PROPERTY(qreal scale READ scale WRITE setScale NOTIFY scaleChanged)
	Q_PROPERTY(qreal rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
	Q_PROPERTY(bool flip READ flip WRITE setFlip NOTIFY flipped)
	Q_PROPERTY(bool mirror READ mirror WRITE setMirror NOTIFY mirrored)
	Q_OBJECT
public:
	explicit CanvasTransform(QObject *parent = 0);

	void setOrigin(const QVector3D &origin);
	void setTranslateX(qreal x);
	void setTranslateY(qreal x);
	void setScale(const qreal scale);
	void setRotation(const qreal rotation);

	void setFlip(bool flip);
	void setMirror(bool mirror);

	QVector3D origin() const { return m_origin; }
	qreal translateX() const { return m_x; }
	qreal translateY() const { return m_y; }
	qreal scale() const { return m_scale; }
	qreal rotation() const { return m_rotation; }
	bool flip() const { return m_flip; }
	bool mirror() const { return m_mirror; }

	void applyTo(QMatrix4x4 *matrix) const;

signals:
	void originChanged(const QVector3D&);
	void xChanged(qreal);
	void yChanged(qreal);
	void scaleChanged(qreal);
	void rotationChanged(qreal);
	void flipped(bool);
	void mirrored(bool);

private:
	QVector3D m_origin;
	qreal m_x, m_y;
	qreal m_scale;
	qreal m_rotation;
	bool m_flip, m_mirror;
};

#endif // MATRIXTRANSFORM_H
