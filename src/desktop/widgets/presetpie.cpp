/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#include "presetpie.h"
#include "core/layer.h"
#include "core/shapes.h"
#include "core/brush.h"
#include "core/floodfill.h"
#include "tools/tool.h"
#include "utils/icon.h"
#include "utils/customshortcutmodel.h"

#include <ColorWheel>

#include <QPainter>
#include <QMouseEvent>
#include <QtMath>
#include <QTransform>
#include <QSettings>

namespace widgets {

using paintcore::Point;

PresetPie::PresetPie(QWidget *parent)
	: QWidget(parent), m_slice(-1)
{
	setMouseTracking(true);
	m_color = new color_widgets::ColorWheel(this);

	connect(m_color, &color_widgets::ColorWheel::colorSelected, this, &PresetPie::colorChanged);

	QSettings cfg;
	cfg.beginGroup("tools");
	cfg.beginReadArray("presets");
	for(int i=0;i<SLICES;++i) {
		cfg.setArrayIndex(i);
		m_preset[i] = tools::ToolProperties::load(cfg);
	}
}

void PresetPie::showAt(const QPoint &p)
{
	Q_ASSERT(parentWidget()); // TODO use screen size if used without a parent widget
	const QSize size = parentWidget()->size();
	QPoint topLeft = p - QPoint(width()/2, height()/2);

	// Constrain within the parent widget
	if(topLeft.x() < 0)
		topLeft.setX(0);
	else if(topLeft.x()+width() > size.width())
		topLeft.setX(size.width() - width());

	if(topLeft.y() < 0)
		topLeft.setY(0);
	else if(topLeft.y()+height() > size.height())
		topLeft.setY(size.height() - height());

	move(topLeft);
	show();
}

void PresetPie::showAtCursor()
{
	if(isVisible())
		hide();
	else
		showAt(parentWidget()->mapFromGlobal(QCursor::pos()));
}

void PresetPie::setColor(const QColor &c)
{
	m_color->setColor(c);
}

void PresetPie::setToolPreset(int slice, const tools::ToolProperties &props)
{
	Q_ASSERT(slice >= 0 && slice < SLICES);
	m_preset[slice] = props;
	m_preview = QPixmap();

	QSettings cfg;
	cfg.beginGroup("tools");
	cfg.beginWriteArray("presets");
	cfg.setArrayIndex(slice);
	props.save(cfg);

	update();
}

void PresetPie::paintEvent(QPaintEvent *)
{
	if(m_preview.isNull())
		redrawPreview();

	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	p.setPen(Qt::NoPen);

	const int d = qMin(width(), height());
	const QRect rect { (width() - d)/2, (height() - d)/2, d, d };
	const int slices = 8;

	// Draw the background
	p.drawPixmap(rect, m_preview);

	// Highlight active slice
	if(m_slice>=0) {
		p.setBrush(QColor(255, 255, 255, 128));
		p.drawPie(rect, (-m_slice-0.5)*(360/slices)*16, 16*360/slices);
	}
}

int PresetPie::sliceAt(const QPoint &p)
{
	const int r = qMin(width(), height()) / 2;
	const int slices = 8;
	const QPoint center {width() / 2, height() / 2};

	// Is the pointer over a slice
	const QPoint dPos = p - center;
	const int dist2 = dPos.x()*dPos.x() + dPos.y()*dPos.y();
	if(dist2 < r*r) {
		qreal a = qRadiansToDegrees(qAtan2(dPos.y(), dPos.x())) + (360 / SLICES / 2);
		if(a<0)
			a = 360+a;

		int slice = a * slices / 360;
		Q_ASSERT(slice>=0 && slice<SLICES);
		return slice;
	}

	return -1;
}

void PresetPie::mouseMoveEvent(QMouseEvent *e)
{
	const int prevSlice = m_slice;
	m_slice = sliceAt(e->pos());

	if(m_slice != prevSlice)
		update();
}

void PresetPie::mouseReleaseEvent(QMouseEvent *e)
{
	m_slice = sliceAt(e->pos());

	if(m_slice>=0) {
		Q_ASSERT(m_slice < SLICES);
		if(e->button() == Qt::MiddleButton) {
			emit presetRequest(m_slice);

		} else if(selectPreset(m_slice)) {
			hide();
		}
	}
}

void PresetPie::assignSelectedPreset()
{
	if(isVisible() && m_slice>=0)
		emit presetRequest(m_slice);
}

void PresetPie::assignPreset(int i)
{
	if(i >= 0 && i < SLICES)
		emit presetRequest(i);
}

bool PresetPie::selectPreset(int i)
{
	if(i >= 0 && i < SLICES && m_preset[i].toolType() >= 0) {
		emit toolSelected(m_preset[i]);
		return true;
	}
	return false;
}

void PresetPie::leaveEvent(QEvent *e)
{
	QWidget::leaveEvent(e);
	hide();
}

void PresetPie::resizeEvent(QResizeEvent *e)
{
	const int d = qMin(e->size().width(), e->size().height()) * 0.4;
	m_color->resize(d, d);
	m_color->move(e->size().width()/2 - d/2, e->size().height()/2 - d/2);
}

static void drawBrushPreview(const tools::ToolProperties &tool, qreal angle, paintcore::Layer &layer)
{
	Q_ASSERT(layer.width()>0 && layer.width() == layer.height());
	const qreal r = layer.width() / 2.0;
	paintcore::PointVector pv;

	// Generate tool specific shape
	const QRectF shapeRect(r*0.5, -r/8, r*0.4, r/4);

	switch(tool.toolType()) {
	case tools::Tool::LINE:
		pv << Point(shapeRect.left(), 0, 1) << Point(shapeRect.right(), 0, 1);
		break;
	case tools::Tool::RECTANGLE: pv = paintcore::shapes::rectangle(shapeRect); break;
	case tools::Tool::ELLIPSE: pv = paintcore::shapes::ellipse(shapeRect); break;
	default:
		pv = paintcore::shapes::sampleStroke(shapeRect);
	}

	// Rotate and position the shape
	QTransform t = QTransform().translate(r, r).rotate(angle);
	QPointF center;
	for(paintcore::Point &p : pv) {
		t.map(p.x(), p.y(), &p.rx(), &p.ry());
		center += p;
	}
	center /= pv.size();

	// Initialize preview brush
	paintcore::Brush brush;
	brush.setSize(tool.intValue("size", 1));
	brush.setSize2(tool.intValue("pressuresize", false) ? 1 : brush.size1());
	brush.setOpacity(tool.intValue("opacity", 100) / 100.0);
	brush.setOpacity2(tool.boolValue("pressureopacity", false) ? 0 : brush.opacity1());
	brush.setHardness(tool.intValue("hardness", 100) / 100.0);
	brush.setHardness2(tool.boolValue("pressurehardness", false) ? 0 : brush.hardness1());
	brush.setSpacing(tool.intValue("spacing", 15));
	brush.setSmudge(tool.intValue("smudge", 0) / 100.0);
	brush.setIncremental(tool.boolValue("incremental", true));

	switch(tool.toolType()) {
	case tools::Tool::PEN:
		brush.setSubpixel(false);
		break;
	case tools::Tool::ERASER:
		brush.setSubpixel(tool.boolValue("hardedge", true));
		brush.setBlendingMode(paintcore::BlendMode::MODE_ERASE);
		break;
	case tools::Tool::BRUSH:
	case tools::Tool::SMUDGE:
		brush.setSubpixel(true);
		break;
	}

	// Draw the shape
	paintcore::StrokeState ss(brush);
	for(int i=1;i<pv.size();++i) {
		layer.drawLine(0, brush, pv[i-1], pv[i], ss);
	}
	layer.mergeSublayer(0);

}

void PresetPie::redrawPreview()
{
	const int d = qMin(width(), height());
	paintcore::Layer layer(nullptr, 0, QString(), QColor(200, 200, 200), QSize(d, d));

	for(int slice=0;slice<SLICES;++slice) {
		switch(m_preset[slice].toolType()) {
		case tools::Tool::PEN:
		case tools::Tool::BRUSH:
		case tools::Tool::SMUDGE:
		case tools::Tool::ERASER:
		case tools::Tool::LINE:
		case tools::Tool::RECTANGLE:
		case tools::Tool::ELLIPSE:
			drawBrushPreview(m_preset[slice], slice * (360.0 / SLICES), layer);
			break;
		}
	}

	// Draw the pie menu shape mask
	QImage mask(d, d, QImage::Format_ARGB32);
	mask.fill(Qt::white);
	{
		QPainter mp(&mask);
		mp.setRenderHint(QPainter::Antialiasing);
		mp.setCompositionMode(QPainter::CompositionMode_Source);
		mp.setBrush(Qt::transparent);
		mp.setPen(Qt::NoPen);
		mp.drawEllipse(0, 0, d, d);
	}

	// Convert the preview layer to an image and paint widget shape mask
	QImage preview = layer.toImage();
	QPainter p(&preview);
	p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
	p.drawImage(0, 0, mask);

	p.setCompositionMode(QPainter::CompositionMode_SourceOver);

	// Draw non-brush type tool icons
	for(int slice=0;slice<SLICES;++slice) {
		const qreal r = d * 0.35;
		const qreal a = qDegreesToRadians(slice * (360.0 / SLICES));
		const QPointF point = QPointF(qCos(a), qSin(a)) * r + QPointF(d/2, d/2);
		if(m_preset[slice].toolType()<0) {
			const qreal dr = d * 0.15;
			const QRectF rect(point - QPointF(dr, dr), point + QPointF(dr, dr));


			QKeySequence assignKey = CustomShortcutModel::getShorcut("assignpreset");

			QFont f;
			f.setPixelSize(10);
			p.setFont(f);
			p.drawText(rect,
				Qt::AlignCenter|Qt::AlignHCenter,
				tr("Assign with %1\nor middle click").arg(assignKey.toString(QKeySequence::NativeText))
				);
			continue;
		}

		QIcon icon;
		switch(m_preset[slice].toolType()) {
		case tools::Tool::FLOODFILL: icon = icon::fromTheme("fill-color"); break;
		case tools::Tool::ANNOTATION: icon = icon::fromTheme("draw-text"); break;
		case tools::Tool::PICKER: icon = icon::fromTheme("color-picker"); break;
		case tools::Tool::LASERPOINTER: icon = icon::fromTheme("cursor-arrow"); break;
		case tools::Tool::SELECTION: icon = icon::fromTheme("select-rectangular"); break;
		case tools::Tool::POLYGONSELECTION: icon = icon::fromTheme("edit-select-lasso"); break;
		}

		if(!icon.isNull()) {
			const QPixmap iconpix = icon.pixmap(66, 66);
			p.drawPixmap(point - QPointF(iconpix.width()/2, iconpix.height()/2), iconpix);
		}
	}

	m_preview = QPixmap::fromImage(preview);
}

}

