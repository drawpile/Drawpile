// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/reference.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/filewrangler.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "desktop/widgets/referenceview.h"
#include <QAction>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QMenu>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <functional>

using std::placeholders::_1;

namespace docks {

ReferenceDock::ReferenceDock(QWidget *parent)
	: DockBase(
		  tr("Reference"), QString(), QIcon::fromTheme("edit-image"), parent)
{
	TitleWidget *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);

	widgets::GroupedToolButton *menuButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped);
	menuButton->setIcon(QIcon::fromTheme("application-menu"));
	menuButton->setPopupMode(QToolButton::InstantPopup);
	titlebar->addCustomWidget(menuButton);

	QMenu *menu = new QMenu(menuButton);
	menuButton->setMenu(menu);

	QAction *openAction =
		menu->addAction(QIcon::fromTheme("document-open"), tr("Open…"));
	connect(
		openAction, &QAction::triggered, this,
		&ReferenceDock::openReferenceFile);

	titlebar->addStretch(1);

	widgets::GroupedToolButton *panButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	panButton->setIcon(QIcon::fromTheme("hand"));
	panButton->setToolTip(tr("Pan image"));
	panButton->setStatusTip(panButton->toolTip());
	panButton->setCheckable(true);
	panButton->setChecked(true);
	titlebar->addCustomWidget(panButton, 3);

	widgets::GroupedToolButton *pickButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	pickButton->setIcon(QIcon::fromTheme("color-picker"));
	pickButton->setToolTip(tr("Pick color"));
	pickButton->setStatusTip(pickButton->toolTip());
	pickButton->setCheckable(true);
	titlebar->addCustomWidget(pickButton, 3);

	m_buttonGroup = new QButtonGroup(this);
	m_buttonGroup->addButton(panButton, 0);
	m_buttonGroup->addButton(pickButton, 1);

	titlebar->addStretch(1);

	QWidget *widget = new QWidget;
	widget->setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *layout = new QVBoxLayout(widget);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(3);

	QHBoxLayout *zoomLayout = new QHBoxLayout;
	zoomLayout->setContentsMargins(0, 0, 0, 0);
	zoomLayout->setSpacing(3);
	layout->addLayout(zoomLayout);

	m_zoomSlider = new KisSliderSpinBox;
	m_zoomSlider->setPrefix(tr("Zoom: "));
	m_zoomSlider->setSuffix(tr("%"));
	m_zoomSlider->setRange(
		qRound(widgets::ReferenceView::zoomMin() * 100.0),
		qRound(widgets::ReferenceView::zoomMax() * 100.0));
	m_zoomSlider->setValue(100);
	m_zoomSlider->setExponentRatio(3.0);
	m_zoomSlider->setEnabled(false);
	zoomLayout->addWidget(m_zoomSlider, 1);
	connect(
		m_zoomSlider, &KisSliderSpinBox::valueChanged, this,
		&ReferenceDock::setZoomFromSlider);

	m_zoomButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped);
	m_zoomButton->setEnabled(false);
	zoomLayout->addWidget(m_zoomButton);
	connect(
		m_zoomButton, &widgets::GroupedToolButton::clicked, this,
		&ReferenceDock::zoomResetOrFit);

	m_view = new widgets::ReferenceView;
	layout->addWidget(m_view);
	connect(
		m_view, &widgets::ReferenceView::zoomChanged, this,
		&ReferenceDock::setZoomFromView);

	updateZoomButton();
	setWidget(widget);
}

void ReferenceDock::openReferenceFile()
{
	FileWrangler(this).openReferenceImage(
		std::bind(&ReferenceDock::showReferenceImage, this, _1));
}

void ReferenceDock::showReferenceImage(const QImage &img)
{
	m_view->setImage(img);
	bool enabled = m_view->hasImage();
	m_view->setEnabled(enabled);
	m_zoomSlider->setEnabled(enabled);
	m_zoomButton->setEnabled(enabled);
}

void ReferenceDock::setZoomFromSlider(int percent)
{
	m_view->setZoom(qreal(percent) / 100.0);
}

void ReferenceDock::setZoomFromView(qreal zoom)
{
	QSignalBlocker blocker(m_zoomSlider);
	m_zoomSlider->setValue(qRound(zoom * 100.0));
	updateZoomButton();
}

void ReferenceDock::updateZoomButton()
{
	if(qFuzzyCompare(m_view->zoom(), 1.0)) {
		if(m_zoomButtonMode != ZOOM_TO_FIT) {
			m_zoomButtonMode = ZOOM_TO_FIT;
			m_zoomButton->setIcon(QIcon::fromTheme("zoom-fit-none"));
			m_zoomButton->setToolTip(tr("Zoom to fit"));
			m_zoomButton->setStatusTip(m_zoomButton->toolTip());
		}
	} else if(m_zoomButtonMode != ZOOM_RESET) {
		m_zoomButtonMode = ZOOM_RESET;
		m_zoomButton->setIcon(QIcon::fromTheme("zoom-original"));
		m_zoomButton->setToolTip(tr("Reset zoom"));
		m_zoomButton->setStatusTip(m_zoomButton->toolTip());
	}
}

void ReferenceDock::zoomResetOrFit()
{
	if(m_view->hasImage()) {
		if(m_zoomButtonMode == ZOOM_TO_FIT) {
			m_view->zoomToFit();
		} else {
			m_view->resetZoom();
		}
	}
}

}
