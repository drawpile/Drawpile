// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/widgets/viewstatus.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "desktop/widgets/KisAngleGauge.h"
#include "desktop/widgets/zoomslider.h"
#include "libclient/settings.h"
#include "libshared/util/qtcompat.h"

#include <QComboBox>
#include <QLineEdit>
#include <QSlider>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QHBoxLayout>
#include <QEvent>
#include <QMenu>

using libclient::settings::zoomMax;
using libclient::settings::zoomMin;
using libclient::settings::zoomSoftMax;
using libclient::settings::zoomSoftMin;

namespace widgets {

ViewStatus::ViewStatus(QWidget *parent)
	: QWidget(parent), m_updating(false)
{
	setMinimumHeight(22);
	QHBoxLayout *layout = new QHBoxLayout(this);

	layout->setContentsMargins(1, 1, 1, 1);
	layout->setSpacing(0);

	QSpacerItem *spacer = new QSpacerItem{
		0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum};
	layout->addSpacerItem(spacer);

	// View flipping
	m_viewFlip = new widgets::GroupedToolButton(this);
	m_viewFlip->setAutoRaise(true);
	m_viewFlip->setGroupPosition(widgets::GroupedToolButton::GroupLeft);

	m_viewMirror = new widgets::GroupedToolButton(this);
	m_viewMirror->setAutoRaise(true);
	m_viewMirror->setGroupPosition(widgets::GroupedToolButton::GroupRight);

	layout->addWidget(m_viewFlip);
	layout->addWidget(m_viewMirror);

	// Canvas rotation reset button
	m_rotationReset = new widgets::GroupedToolButton(this);
	m_rotationReset->setAutoRaise(true);

	layout->addSpacing(10);
	layout->addWidget(m_rotationReset);

	// Canvas compass
	m_compass = new KisAngleGauge(this);
	m_compass->setFixedSize(22, 22);
	m_compass->setIncreasingDirection(KisAngleGauge::IncreasingDirection_Clockwise);
	connect(m_compass, &KisAngleGauge::angleChanged, this, &ViewStatus::angleChanged);
	layout->addSpacing(10);
	layout->addWidget(m_compass);

	// Canvas rotation box
	m_angleBox = new QComboBox(this);
	m_angleBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	m_angleBox->setFixedWidth(m_angleBox->fontMetrics().boundingRect("9999-O--").width());
	m_angleBox->setFrame(false);
	m_angleBox->setEditable(true);
	m_angleBox->setToolTip(tr("Canvas Rotation"));

	m_angleBox->addItems({
		QStringLiteral("-135°"),
		QStringLiteral("-90°"),
		QStringLiteral("-45°"),
		QStringLiteral("0°"),
		QStringLiteral("45°"),
		QStringLiteral("90°"),
		QStringLiteral("135°"),
		QStringLiteral("180°"),
	});
	m_angleBox->setEditText(QStringLiteral("0°"));

	m_angleBox->lineEdit()->setValidator(
		new QRegularExpressionValidator(
			QRegularExpression("-?[0-9]{0,3}°?"),
			this
		)
	);
	connect(m_angleBox, &QComboBox::editTextChanged, this, &ViewStatus::angleBoxChanged);

	layout->addSpacing(4);
	layout->addWidget(m_angleBox);

	// Zoom slider
	m_zoomSlider = new ZoomSlider(this);
	m_zoomSlider->setMinimumWidth(24);
	m_zoomSlider->setMaximumWidth(200);
	m_zoomSlider->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
	m_zoomSlider->setMinimum(zoomMin * 100.0);
	m_zoomSlider->setMaximum(zoomMax * 100.0);
	m_zoomSlider->setExponentRatio(4.0);
	m_zoomSlider->setValue(100.0);
	m_zoomSlider->setSuffix("%");
	connect(m_zoomSlider, QOverload<double>::of(&KisDoubleSliderSpinBox::valueChanged), this, &ViewStatus::zoomSliderChanged);
	connect(
		m_zoomSlider, &ZoomSlider::zoomStepped, this, &ViewStatus::zoomStepped,
		Qt::QueuedConnection);

	// Zoom preset button
	m_zoomPreset = new widgets::GroupedToolButton(this);
	m_zoomPreset->setIcon(QIcon::fromTheme("zoom-fit-none"));
	m_zoomPreset->setText(tr("Zoom"));
	m_zoomPreset->setAutoRaise(true);
	m_zoomPreset->setPopupMode(QToolButton::InstantPopup);

	m_zoomsMenu = new QMenu{m_zoomPreset};
	m_zoomPreset->setMenu(m_zoomsMenu);
	for(qreal zoomLevel : libclient::settings::zoomLevels()) {
		if(zoomLevel >= zoomSoftMin && zoomLevel <= zoomSoftMax) {
			QAction *zoomAction = m_zoomsMenu->addAction(
				QStringLiteral("%1%").arg(zoomLevel * 100.0, 0, 'f', 2));
			connect(zoomAction, &QAction::triggered, this, [=] {
				emit zoomChanged(zoomLevel);
			});
		}
	}

	layout->addSpacing(10);
	layout->addWidget(m_zoomSlider);
	layout->addWidget(m_zoomPreset);

	updatePalette();
}

void ViewStatus::updatePalette()
{
#ifndef Q_OS_MACOS
	auto boxPalette = palette();
	boxPalette.setColor(QPalette::Base, boxPalette.color(QPalette::Window));
	m_angleBox->setPalette(boxPalette);
#endif
}

void ViewStatus::setActions(
	QAction *flip, QAction *mirror, QAction *rotationReset,
	const QVector<QAction *> &zoomActions)
{
	m_viewFlip->setDefaultAction(flip);
	m_viewMirror->setDefaultAction(mirror);
	m_rotationReset->setDefaultAction(rotationReset);
	m_zoomsMenu->addSeparator();
	for(QAction *zoomAction : zoomActions) {
		m_zoomsMenu->addAction(zoomAction);
	}
}

void ViewStatus::setTransformation(qreal zoom, qreal angle)
{
	m_updating = true;
	m_compass->setAngle(angle);

	const int intAngle = qRound(angle);
	const int angleCursorPos = m_angleBox->lineEdit()->cursorPosition();
	m_angleBox->setEditText(QString::number(intAngle) + QChar(0x00b0));
	m_angleBox->lineEdit()->setCursorPosition(angleCursorPos);

	const double percentZoom = zoom * 100.0;
	if(percentZoom != m_zoomSlider->value()) {
		m_zoomSlider->setValue(percentZoom);
	}
	m_updating = false;
}

void ViewStatus::zoomSliderChanged(double value)
{
	if(!m_updating) {
		emit zoomChanged(value / 100.0);
	}
}

void ViewStatus::angleBoxChanged(const QString &text)
{
	if(m_updating)
		return;

	const int suffix = text.indexOf(QChar(0x00b0));
	const auto num = suffix>0 ? compat::StringView{text}.left(suffix) : compat::StringView{text};

	bool ok;
	const int number = num.toInt(&ok);
	if(ok)
		emit angleChanged(number);
}

void ViewStatus::changeEvent(QEvent *event)
{
	QWidget::changeEvent(event);
	if(event->type() == QEvent::PaletteChange)
		updatePalette();
}

}
