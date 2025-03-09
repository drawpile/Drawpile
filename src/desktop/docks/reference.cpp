// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/reference.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/filewrangler.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "desktop/widgets/referenceview.h"
#include "libclient/document.h"
#include <QAction>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QImageReader>
#include <QMenu>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <functional>

using std::placeholders::_1;
using std::placeholders::_2;

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
	openAction->setStatusTip(tr("Open a file as a reference image"));
	connect(
		openAction, &QAction::triggered, this,
		&ReferenceDock::openReferenceFile);

	m_pasteAction =
		menu->addAction(QIcon::fromTheme("edit-paste"), tr("Paste"));
	m_pasteAction->setStatusTip(
		tr("Paste a reference image from the clipboard"));
	connect(
		m_pasteAction, &QAction::triggered, this,
		&ReferenceDock::pasteReferenceImage);

	m_closeAction = menu->addAction(tr("Close"));
	m_closeAction->setStatusTip(tr("Close the current reference image"));
	connect(
		m_closeAction, &QAction::triggered, this,
		&ReferenceDock::closeReferenceImage);

	connect(menu, &QMenu::aboutToShow, this, &ReferenceDock::updateMenuActions);

	titlebar->addStretch(1);

	widgets::GroupedToolButton *pickButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	pickButton->setIcon(QIcon::fromTheme("color-picker"));
	pickButton->setToolTip(tr("Pick color"));
	pickButton->setStatusTip(pickButton->toolTip());
	pickButton->setCheckable(true);
	pickButton->setChecked(true);
	titlebar->addCustomWidget(pickButton, 3);

	widgets::GroupedToolButton *panButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	panButton->setIcon(QIcon::fromTheme("hand"));
	panButton->setToolTip(tr("Pan image"));
	panButton->setStatusTip(panButton->toolTip());
	panButton->setCheckable(true);
	titlebar->addCustomWidget(panButton, 3);

	m_buttonGroup = new QButtonGroup(this);
	m_buttonGroup->addButton(
		pickButton, int(widgets::ReferenceView::InteractionMode::Pick));
	m_buttonGroup->addButton(
		panButton, int(widgets::ReferenceView::InteractionMode::Pan));
	connect(
		m_buttonGroup, &QButtonGroup::idClicked, this,
		&ReferenceDock::setInteractionMode);

	titlebar->addStretch(1);

	QWidget *widget = new QWidget;
	widget->setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *layout = new QVBoxLayout(widget);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(3);

	QHBoxLayout *zoomLayout = new QHBoxLayout;
	zoomLayout->setContentsMargins(0, 0, 0, 0);
	zoomLayout->setSpacing(0);
	layout->addLayout(zoomLayout);

	m_resetZoomButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	m_resetZoomButton->setIcon(QIcon::fromTheme("zoom-original"));
	m_resetZoomButton->setToolTip(tr("Reset zoom"));
	m_resetZoomButton->setStatusTip(m_resetZoomButton->toolTip());
	m_resetZoomButton->setEnabled(false);
	zoomLayout->addWidget(m_resetZoomButton);

	m_zoomToFitButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_zoomToFitButton->setIcon(QIcon::fromTheme("zoom-fit-none"));
	m_zoomToFitButton->setToolTip(tr("Zoom to fit"));
	m_zoomToFitButton->setStatusTip(m_zoomToFitButton->toolTip());
	m_zoomToFitButton->setEnabled(false);
	zoomLayout->addWidget(m_zoomToFitButton);

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
		m_zoomSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&ReferenceDock::setZoomFromSlider);

	m_view = new widgets::ReferenceView;
	layout->addWidget(m_view);
	connect(
		m_resetZoomButton, &widgets::GroupedToolButton::clicked, m_view,
		&widgets::ReferenceView::resetZoom);
	connect(
		m_zoomToFitButton, &widgets::GroupedToolButton::clicked, m_view,
		&widgets::ReferenceView::zoomToFit);
	connect(
		m_view, &widgets::ReferenceView::colorPicked, this,
		&ReferenceDock::colorPicked);
	connect(
		m_view, &widgets::ReferenceView::zoomChanged, this,
		&ReferenceDock::setZoomFromView);
	connect(
		m_view, &widgets::ReferenceView::openRequested, this,
		&ReferenceDock::openReferenceFile);
	connect(
		m_view, &widgets::ReferenceView::imageDropped, this,
		&ReferenceDock::handleImageDrop);
	connect(
		m_view, &widgets::ReferenceView::pathDropped, this,
		&ReferenceDock::handlePathDrop);

	setWidget(widget);
}

QPoint ReferenceDock::scrollPosition() const
{
	QPoint p;
	if(m_view && m_view->hasImage()) {
		if(QScrollBar *hscroll = m_view->horizontalScrollBar()) {
			p.setX(hscroll->value());
		}
		if(QScrollBar *vscroll = m_view->verticalScrollBar()) {
			p.setY(vscroll->value());
		}
	}
	return p;
}

void ReferenceDock::setScrollPosition(const QPoint &p)
{
	if(m_view && m_view->hasImage()) {
		if(QScrollBar *hscroll = m_view->horizontalScrollBar()) {
			hscroll->setValue(p.x());
		}
		if(QScrollBar *vscroll = m_view->verticalScrollBar()) {
			vscroll->setValue(p.y());
		}
	}
}

void setScrollPosition(const QPoint &p);

void ReferenceDock::openReferenceFile()
{
	FileWrangler(this).openReferenceImage(
		std::bind(&ReferenceDock::showReferenceImageFromFile, this, _1, _2));
}

void ReferenceDock::updateMenuActions()
{
	m_pasteAction->setEnabled(Document::clipboardHasImageData());
	m_closeAction->setEnabled(m_view->hasImage());
}

void ReferenceDock::pasteReferenceImage()
{
	handleImageDrop(
		Document::getClipboardImageData(Document::getClipboardData()));
}

void ReferenceDock::showReferenceImageFromFile(
	const QImage &img, const QString &error)
{
	if(img.isNull() || img.size().isEmpty()) {
		utils::showCritical(
			this, tr("Error"),
			//: %1 is an error message.
			tr("Could not open reference image: %1.").arg(error));
	} else {
		showReferenceImage(img);
	}
}

void ReferenceDock::showReferenceImage(const QImage &img)
{
	m_view->setImage(img);
	bool enabled = m_view->hasImage();
	m_zoomToFitButton->setEnabled(enabled);
	m_resetZoomButton->setEnabled(enabled);
	m_zoomSlider->setEnabled(enabled);
}

void ReferenceDock::handleImageDrop(const QImage &img)
{
	if(!img.isNull() && !img.size().isEmpty()) {
		showReferenceImage(img);
	}
}

void ReferenceDock::handlePathDrop(const QString &path)
{
	QImage img;
	QString error;
	{
		QImageReader reader(path);
		if(!reader.read(&img)) {
			error = reader.errorString();
		}
	}
	showReferenceImageFromFile(img, error);
}

void ReferenceDock::closeReferenceImage()
{
	showReferenceImage(QImage());
}

void ReferenceDock::setInteractionMode(int interactionMode)
{
	m_view->setInteractionMode(
		interactionMode == int(widgets::ReferenceView::InteractionMode::Pick)
			? widgets::ReferenceView::InteractionMode::Pick
			: widgets::ReferenceView::InteractionMode::Pan);
}

void ReferenceDock::setZoomFromSlider(int percent)
{
	m_view->setZoom(qreal(percent) / 100.0);
}

void ReferenceDock::setZoomFromView(qreal zoom)
{
	QSignalBlocker blocker(m_zoomSlider);
	m_zoomSlider->setValue(qRound(zoom * 100.0));
}


}
