// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/config/colorwheelconfig.h"
#include "libclient/config/config.h"
#include <QScopedValueRollback>
#include <QtColorWidgets/color_dialog.hpp>
#include <QtColorWidgets/color_wheel.hpp>

namespace config {

ColorWheelConfig::ColorWheelConfig(Config *cfg, QObject *parent)
	: QObject(parent)
	, m_cfg(cfg)
{
	CFG_BIND_SET(
		cfg, ColorWheelShape, this, ColorWheelConfig::setShapeFromConfig);
	CFG_BIND_SET(
		cfg, ColorWheelAngle, this, ColorWheelConfig::setAngleFromConfig);
	CFG_BIND_SET(
		cfg, ColorWheelMirror, this, ColorWheelConfig::setMirrorFromConfig);
	m_updating = false;
	updateWheelConfig();
}

void ColorWheelConfig::setType(int type)
{
	int shape, angle;
	switch(type) {
	case TYPE_ROTATING_TRIANGLE:
		shape = SHAPE_TRIANGLE;
		angle = ANGLE_ROTATING;
		break;
	case TYPE_ROTATING_SQUARE:
		shape = SHAPE_SQUARE;
		angle = ANGLE_ROTATING;
		break;
	case TYPE_UPRIGHT_TRIANGLE:
		shape = SHAPE_TRIANGLE;
		angle = ANGLE_FIXED;
		break;
	case TYPE_SIDEWAYS_TRIANGLE:
		shape = SHAPE_TRIANGLE;
		angle = ANGLE_FIXED_SIDEWAYS;
		break;
	case TYPE_FIXED_SQUARE:
		shape = SHAPE_SQUARE;
		angle = ANGLE_FIXED;
		break;
	default:
		qWarning("Unknown color wheel config type %d", type);
		return;
	}

	if(shape != m_shape || angle != m_angle) {
		m_updating = true;
		m_shape = shape;
		m_angle = angle;
		m_cfg->setColorWheelShape(shape);
		m_cfg->setColorWheelAngle(angle);
		m_updating = false;
		updateWheelConfig();
	}
}

void ColorWheelConfig::applyToColorDialog(color_widgets::ColorDialog *dlg) const
{
	if(dlg) {
		dlg->setWheelShape(
			color_widgets::ColorWheel::ShapeEnum(m_wheelSelectorShape));
		dlg->setWheelAngle(m_wheelFixedSelectorAngle);
		dlg->setWheelRotating(m_wheelRotatingSelector);
		dlg->setWheelMirrored(m_wheelMirroredSelector);
	}
}

void ColorWheelConfig::applyToColorWheel(color_widgets::ColorWheel *wheel) const
{
	if(wheel) {
		wheel->setSelectorShape(
			color_widgets::ColorWheel::ShapeEnum(m_wheelSelectorShape));
		wheel->setFixedSelectorAngle(m_wheelFixedSelectorAngle);
		wheel->setRotatingSelector(m_wheelRotatingSelector);
		wheel->setMirroredSelector(m_wheelMirroredSelector);
	}
}

void ColorWheelConfig::setShapeFromConfig(int shape)
{
	if(m_updating) {
		m_shape = shape;
	} else if(shape != m_shape) {
		m_shape = shape;
		updateWheelConfig();
	}
}

void ColorWheelConfig::setAngleFromConfig(int angle)
{
	if(m_updating) {
		m_angle = angle;
	} else if(angle != m_angle) {
		m_angle = angle;
		updateWheelConfig();
	}
}

void ColorWheelConfig::setMirrorFromConfig(bool mirror)
{
	if(m_updating) {
		m_mirror = mirror;
	} else if(mirror != m_mirror) {
		m_mirror = mirror;
		updateWheelConfig();
	}
}

void ColorWheelConfig::updateWheelConfig()
{
	bool square = m_shape != SHAPE_TRIANGLE;
	if(square) {
		m_wheelSelectorShape = int(color_widgets::ColorWheel::ShapeSquare);
	} else {
		m_wheelSelectorShape = int(color_widgets::ColorWheel::ShapeTriangle);
	}

	switch(m_angle) {
	case ANGLE_ROTATING:
		m_wheelRotatingSelector = true;
		m_wheelMirroredSelector = m_mirror;
		m_wheelFixedSelectorAngle = 0;
		if(square) {
			m_type = TYPE_ROTATING_SQUARE;
		} else {
			m_type = TYPE_ROTATING_TRIANGLE;
		}
		break;
	case ANGLE_FIXED_SIDEWAYS:
		if(square) {
			// We don't have this option for the fixed square, so just fall
			// through to the fixed square case.
		} else {
			m_type = TYPE_SIDEWAYS_TRIANGLE;
			m_wheelRotatingSelector = false;
			m_wheelMirroredSelector = !m_mirror; // Point right by default.
			m_wheelFixedSelectorAngle = 0;
			break;
		}
		Q_FALLTHROUGH();
	default:
		m_wheelRotatingSelector = false;
		m_wheelMirroredSelector = m_mirror;
		if(square) {
			m_type = TYPE_FIXED_SQUARE;
			m_wheelFixedSelectorAngle = 0;
		} else {
			m_type = TYPE_UPRIGHT_TRIANGLE;
			m_wheelFixedSelectorAngle = 150;
		}
		break;
	}

	Q_EMIT wheelConfigChanged();
}

}
