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

#include "polylineitem.h"

#include <QSGGeometryNode>
#include <QSGFlatColorMaterial>
#include <QSGSimpleMaterialShader>

PolyLineItem::PolyLineItem(QQuickItem *parent)
	: QQuickItem(parent), m_color(Qt::red), m_lineWidth(1), m_style(LaserScan),
	  m_pointsChanged(false), m_colorChanged(false), m_lineWidthChanged(false), m_styleChanged(false)
{
	setFlag(ItemHasContents, true);
	startTimer(40);
}

void PolyLineItem::setPoints(const QVector<QPointF> &points)
{
	qDebug("pointsChanged");
	m_points = points;
	m_pointsChanged = true;
	emit pointsChanged();
	update();
}

void PolyLineItem::setColor(const QColor &color)
{
	if(m_color != color) {
		m_color = color;
		m_colorChanged = true;
		update();
		emit colorChanged();
	}
}

void PolyLineItem::setLineWidth(int w)
{
	w = qMax(1, w);
	if(m_lineWidth != w) {
		m_lineWidth = w;
		m_lineWidthChanged = true;
		update();
		emit lineWidthChanged(w);
	}
}

void PolyLineItem::setLineStyle(Style style)
{
	if(m_style != style) {
		m_style = style;
		update();
		emit lineStyleChanged();
	}
}

void PolyLineItem::timerEvent(QTimerEvent *)
{
	// This advances the shader animation
	update();
}

struct BlinkyMaterialState {
	QColor color;
	int offset;
};

class LaserScanMaterial : public QSGSimpleMaterialShader<BlinkyMaterialState>
{
	QSG_DECLARE_SIMPLE_SHADER(LaserScanMaterial, BlinkyMaterialState)
public:
	const char *vertexShader() const {
		return
		"attribute highp vec4 vertex;\n"
		"uniform highp mat4 qt_Matrix;\n"
		"void main() {\n"
		"    gl_Position = qt_Matrix * vertex;\n"
		"}";
	}

	const char *fragmentShader() const {
		return
		"uniform lowp float qt_Opacity;\n"
		"uniform lowp vec4 color;\n"
		"uniform lowp int offset;\n"
		"void main() {\n"
			"float w = (int(gl_FragCoord.y + offset) & 2) ? 1 : 0.8;\n"
			"gl_FragColor = color * qt_Opacity * w;\n"
		"}";
	}

	QList<QByteArray> attributes() const {
		return QList<QByteArray>() << "vertex";
	}

	void updateState(const BlinkyMaterialState *state, const BlinkyMaterialState *oldstate) {
		if(!oldstate || oldstate->color != state->color)
			program()->setUniformValue("color", state->color.redF(), state->color.greenF(), state->color.blueF(), state->color.alphaF());
		program()->setUniformValue("offset", state->offset);
	}
};

class MarchingAntsMaterial : public QSGSimpleMaterialShader<BlinkyMaterialState>
{
	QSG_DECLARE_SIMPLE_SHADER(MarchingAntsMaterial, BlinkyMaterialState)
public:
	const char *vertexShader() const {
		return
		"attribute highp vec4 vertex;\n"
		"uniform highp mat4 qt_Matrix;\n"
		"void main() {\n"
		"    gl_Position = qt_Matrix * vertex;\n"
		"}";
	}

	const char *fragmentShader() const {
		return
		"uniform lowp float qt_Opacity;\n"
		"uniform lowp vec4 color;\n"
		"uniform lowp int offset;\n"
		"void main() {\n"
			"float w = ((int(gl_FragCoord.x + gl_FragCoord.y + offset) & 15) < 7) ? 1 : 0.6;\n"
			"gl_FragColor = color * qt_Opacity * w;\n"
		"}";
	}

	QList<QByteArray> attributes() const {
		return QList<QByteArray>() << "vertex";
	}

	void updateState(const BlinkyMaterialState *state, const BlinkyMaterialState *oldstate) {
		if(!oldstate || oldstate->color != state->color)
			program()->setUniformValue("color", state->color.redF(), state->color.greenF(), state->color.blueF(), state->color.alphaF());
		program()->setUniformValue("offset", state->offset);
	}
};

static QSGSimpleMaterial<BlinkyMaterialState> *createBlinkyStyle(PolyLineItem::Style style) {
	switch(style) {
	case PolyLineItem::LaserScan: return LaserScanMaterial::createMaterial();
	case PolyLineItem::MarchingAnts: return MarchingAntsMaterial::createMaterial();
	}
	return nullptr;
}

QSGNode *PolyLineItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
	QSGGeometryNode *node = nullptr;
	QSGGeometry *geometry = nullptr;

	if(!oldNode) {
		node = new QSGGeometryNode;
		geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), m_points.size());
		geometry->setLineWidth(m_lineWidth);
		geometry->setDrawingMode(GL_LINE_STRIP);
		node->setGeometry(geometry);
		node->setFlag(QSGNode::OwnsGeometry);

		//QSGFlatColorMaterial *material = new QSGFlatColorMaterial;
		auto *material = createBlinkyStyle(m_style);
		material->state()->color = m_color;
		material->state()->offset = 0;
		node->setMaterial(material);
		node->setFlag(QSGNode::OwnsMaterial);
		m_pointsChanged = true;
	} else {
		node = static_cast<QSGGeometryNode*>(oldNode);
		geometry = node->geometry();

		if(m_lineWidthChanged) {
			geometry->setLineWidth(m_lineWidth);
			m_lineWidthChanged = false;
		}

		auto *material = static_cast<QSGSimpleMaterial<BlinkyMaterialState>*>(node->material());

		if(m_colorChanged) {
			material->state()->color = m_color;
			m_colorChanged = false;
		}

		// animate on every frame
		material->state()->offset += 1;
		node->markDirty(QSGNode::DirtyMaterial);
	}

	if(m_pointsChanged) {
		geometry->allocate(m_points.size());
		QSGGeometry::Point2D *vertices = geometry->vertexDataAsPoint2D();
		const QRectF bounds = boundingRect();
		for(int i=0;i<m_points.size();++i) {
			vertices[i].set(bounds.x() + m_points.at(i).x(), bounds.y() + m_points.at(i).y());
		}
		node->markDirty(QSGNode::DirtyGeometry);
		m_pointsChanged = false;
	}

	return node;
}
