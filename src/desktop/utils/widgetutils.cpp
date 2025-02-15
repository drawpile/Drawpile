// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/widgetutils.h"
#include "desktop/main.h"
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCursor>
#include <QDesktopServices>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPair>
#include <QPixmap>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>

namespace utils {

ScopedUpdateDisabler::ScopedUpdateDisabler(QWidget *widget)
	: m_widget{widget}
	, m_wasEnabled{widget && widget->updatesEnabled()}
{
	if(m_wasEnabled) {
		widget->setUpdatesEnabled(false);
	}
}

ScopedUpdateDisabler::~ScopedUpdateDisabler()
{
	if(m_wasEnabled) {
		m_widget->setUpdatesEnabled(true);
	}
}


EncapsulatedLayout::EncapsulatedLayout()
	: QHBoxLayout()
{
	setDirection(LeftToRight);
}

QSizePolicy::ControlTypes EncapsulatedLayout::controlTypes() const
{
	if(m_controlTypes) {
		return *m_controlTypes;
	} else {
		return QHBoxLayout::controlTypes();
	}
}

void EncapsulatedLayout::setControlTypes(QSizePolicy::ControlTypes types)
{
	m_controlTypes = types;
}

// On macOS only, `QBoxLayout::effectiveMargins` adds extra hidden margins
// to the layout. This is purportedly to avoid the macOS widget decorations
// being cropped (or, I suppose, overdrawing onto other widgets), but it really
// only succeeds in breaking grid alignment since (a) controls in modern macOS
// don’t have giant shadows like they used to, and (b) the layouts are nested
// and so have plenty of space for decorations.
// The best way to deal with this is to rewrite all of Qt layouts from scratch
// (and probably also the macOS style), which is a bit beyond the scope of this
// project, so this workaround is fine.
#ifdef Q_OS_MACOS
int EncapsulatedLayout::heightForWidth(int width) const
{
	return adjustHeightForWidth(QHBoxLayout::heightForWidth(width));
}

void EncapsulatedLayout::invalidate()
{
	m_dirty = true;
	QHBoxLayout::invalidate();
}

QSize EncapsulatedLayout::maximumSize() const
{
	auto maxSize = adjustSizeHint(QHBoxLayout::maximumSize());

	if(alignment() & Qt::AlignHorizontal_Mask) {
		maxSize.setWidth(QLAYOUTSIZE_MAX);
	}
	if(alignment() & Qt::AlignVertical_Mask) {
		maxSize.setHeight(QLAYOUTSIZE_MAX);
	}

	return maxSize;
}

int EncapsulatedLayout::minimumHeightForWidth(int width) const
{
	return adjustHeightForWidth(QHBoxLayout::minimumHeightForWidth(width));
}

QSize EncapsulatedLayout::minimumSize() const
{
	return adjustSizeHint(QHBoxLayout::minimumSize());
}

// Since we override the size hint to stop the layout from growing, we
// also have to undo `QBoxLayout` correcting all the widget geometry for the
// extra fake layout margins that no longer exist.
void EncapsulatedLayout::setGeometry(const QRect &rect)
{
	recoverEffectiveMargins();

	const auto c = count();

	// Since QHBoxLayout does not expect anything to meddle with the geometry of
	// the widgets that it controls, it is necessary to restore the original
	// geometries or else widgets will start marching in the direction of any
	// asymmetrical margin correction every time the layout resizes. This
	// suggests that reusing QBoxLayout is probably a really truly bad idea.
	if(m_topMargin || m_bottomMargin) {
		for(auto i = 0; i < c; ++i) {
			auto *item = itemAt(i);
			if(auto *widget = item->widget()) {
				auto oldGeometry =
					widget->property("encapsulatedLayoutGeometry").toRect();
				if(oldGeometry.isValid()) {
					widget->setGeometry(oldGeometry);
				}
			}
		}
	}

	// This fixes the geometry of the layout box
	QHBoxLayout::setGeometry(rect.adjusted(
		-m_leftMargin, -m_topMargin, -m_rightMargin, -m_bottomMargin));

	// This fixes the position and size of all the widgets inside the layout box
	// that were adjusted with the extra `QBoxLayoutPrivate::effectiveMargins`
	//
	// TODO: Adjusting the left (and probably right) margin causes e.g.
	// checkboxes to misalign with other ones in the same form layout that are
	// not using `EncapsulatedLayout`. It also causes required whitespace on the
	// right side of inline widgets to disappear. The approach being used here
	// is cursed so it could just be implemented incorrectly, but it is also
	// possible `QBoxLayout` does something different depending on the direction
	// of the layout. Since this component is horizontal-only for now anyway,
	// just only adjust the top/bottom since those definitely need fixing.
	if(m_topMargin || m_bottomMargin) {
		for(auto i = 0; i < c; ++i) {
			auto *item = itemAt(i);
			if(auto *widget = item->widget()) {
				widget->setProperty(
					"encapsulatedLayoutGeometry", widget->geometry());
				item->setGeometry(item->geometry().adjusted(
					0, -m_topMargin, 0, m_topMargin + m_bottomMargin));
			}
		}
	}
}

QSize EncapsulatedLayout::sizeHint() const
{
	return adjustSizeHint(QHBoxLayout::sizeHint());
}

int EncapsulatedLayout::adjustHeightForWidth(int height) const
{
	if(height == -1) {
		return height;
	}

	recoverEffectiveMargins();
	return height - m_topMargin - m_bottomMargin;
}

QSize EncapsulatedLayout::adjustSizeHint(QSize size) const
{
	recoverEffectiveMargins();
	size.rwidth() -= m_leftMargin + m_rightMargin;
	size.rheight() -= m_topMargin + m_bottomMargin;

	return size;
}

void EncapsulatedLayout::recoverEffectiveMargins() const
{
	if(m_dirty) {
		m_dirty = false;
	} else {
		return;
	}

	m_leftMargin = 0;
	m_rightMargin = 0;
	m_topMargin = 0;
	m_bottomMargin = 0;

	// This is basically `QBoxLayoutPrivate::effectiveMargins`
	const auto c = count();
	auto *first = itemAt(0);
	auto *last = itemAt(c - 1);
	if(direction() == QBoxLayout::RightToLeft) {
		std::swap(first, last);
	}
	if(auto *widget = first->widget()) {
		m_leftMargin = first->geometry().left() - widget->geometry().left();
	}
	if(auto *widget = last->widget()) {
		m_rightMargin = last->geometry().right() - widget->geometry().right();
	}
	for(auto i = 0; i < c; ++i) {
		auto *item = itemAt(i);
		if(auto *widget = item->widget()) {
			const auto itemGeom = item->geometry();
			const auto widgetGeom = widget->geometry();
			m_topMargin = qMax(m_topMargin, itemGeom.top() - widgetGeom.top());
			m_bottomMargin =
				qMax(m_bottomMargin, widgetGeom.bottom() - itemGeom.bottom());
		}
	}
}
#endif


KineticScroller::KineticScroller(
	QAbstractScrollArea *parent, Qt::ScrollBarPolicy horizontalScrollBarPolicy,
	Qt::ScrollBarPolicy verticalScrollBarPolicy, int kineticScrollGesture,
	int kineticScrollThreshold, int kineticScrollHideBars)
	: QObject(parent)
	, m_scrollArea(parent)
	, m_horizontalScrollBarPolicy(horizontalScrollBarPolicy)
	, m_verticalScrollBarPolicy(verticalScrollBarPolicy)
	, m_kineticScrollGesture(int(desktop::settings::KineticScrollGesture::None))
	, m_kineticScrollSensitivity(-1)
	, m_kineticScrollHideBars(false)
{
	reset(
		kineticScrollGesture, toSensitivity(kineticScrollThreshold),
		kineticScrollHideBars);
}

void KineticScroller::setVerticalScrollBarPolicy(
	Qt::ScrollBarPolicy verticalScrollBarPolicy)
{
	if(verticalScrollBarPolicy != m_verticalScrollBarPolicy) {
		m_verticalScrollBarPolicy = verticalScrollBarPolicy;
		if(!isKineticScrollingEnabled(m_kineticScrollGesture) ||
		   !m_kineticScrollHideBars) {
			m_scrollArea->setVerticalScrollBarPolicy(verticalScrollBarPolicy);
		}
	}
}

void KineticScroller::setKineticScrollGesture(int kineticScrollGesture)
{
	if(kineticScrollGesture != m_kineticScrollGesture) {
		reset(
			kineticScrollGesture, m_kineticScrollSensitivity,
			m_kineticScrollHideBars);
	}
}

void KineticScroller::setKineticScrollThreshold(int kineticScrollThreshold)
{
	int kineticScrollSensitivity = toSensitivity(kineticScrollThreshold);
	if(kineticScrollSensitivity != m_kineticScrollSensitivity) {
		reset(
			m_kineticScrollGesture, kineticScrollSensitivity,
			m_kineticScrollHideBars);
	}
}

void KineticScroller::setKineticScrollHideBars(bool kineticScrollHideBars)
{
	if(kineticScrollHideBars != m_kineticScrollHideBars) {
		reset(
			m_kineticScrollGesture, m_kineticScrollSensitivity,
			kineticScrollHideBars);
	}
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-3.0-or-later
// SDPX—SnippetName: Based on the kinetic scroll event filter from Krita
bool KineticScroller::eventFilter(QObject *watched, QEvent *event)
{
	switch(event->type()) {
	case QEvent::Enter:
		QScroller::ungrabGesture(m_scrollArea);
		break;
	case QEvent::Leave: {
		QScroller::ScrollerGestureType gestureType;
		if(isKineticScrollingEnabled(m_kineticScrollGesture, &gestureType)) {
			QScroller::grabGesture(
				m_scrollArea, QScroller::ScrollerGestureType(gestureType));
		}
		break;
	}
	default:
		break;
	}
	return QObject::eventFilter(watched, event);
}

void KineticScroller::reset(
	int kineticScrollGesture, int kineticScrollSensitivity,
	bool kineticScrollHideBars)
{
	ScopedUpdateDisabler disabler(m_scrollArea);
	QScroller::ScrollerGestureType gestureType;
	bool isEnabled =
		isKineticScrollingEnabled(kineticScrollGesture, &gestureType);
	bool wasEnabled = isKineticScrollingEnabled(m_kineticScrollGesture);
	if(isEnabled) {
		if(wasEnabled) {
			disableKineticScrolling();
		}
		enableKineticScrolling(
			gestureType, kineticScrollSensitivity, kineticScrollHideBars);
	} else {
		disableKineticScrolling();
	}
	m_kineticScrollGesture = kineticScrollGesture;
	m_kineticScrollSensitivity = kineticScrollSensitivity;
	m_kineticScrollHideBars = kineticScrollHideBars;
}

void KineticScroller::enableKineticScrolling(
	QScroller::ScrollerGestureType gestureType, int kineticScrollSensitivity,
	bool kineticScrollHideBars)
{
	if(kineticScrollHideBars) {
		m_scrollArea->setHorizontalScrollBarPolicy(
			Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
		m_scrollArea->setVerticalScrollBarPolicy(
			Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
	} else if(gestureType != QScroller::TouchGesture) {
		m_scrollArea->setHorizontalScrollBarPolicy(m_horizontalScrollBarPolicy);
		m_scrollArea->setVerticalScrollBarPolicy(m_verticalScrollBarPolicy);
		setEventFilter(m_scrollArea->horizontalScrollBar(), true);
		setEventFilter(m_scrollArea->verticalScrollBar(), true);
	}

	setScrollPerPixel(true);

	QScroller *scroller = QScroller::scroller(m_scrollArea);
	// XXX: this leaks memory, see QTBUG-82355.
	QScroller::grabGesture(m_scrollArea, gestureType);
	scroller->setScrollerProperties(
		getScrollerPropertiesForSensitivity(kineticScrollSensitivity));
}

void KineticScroller::disableKineticScrolling()
{
	m_scrollArea->setHorizontalScrollBarPolicy(m_horizontalScrollBarPolicy);
	m_scrollArea->setVerticalScrollBarPolicy(m_verticalScrollBarPolicy);
	setEventFilter(m_scrollArea->horizontalScrollBar(), false);
	setEventFilter(m_scrollArea->verticalScrollBar(), false);
	if(QScroller::hasScroller(m_scrollArea)) {
		QScroller::ungrabGesture(m_scrollArea);
	}
}

void KineticScroller::setScrollPerPixel(bool scrollPerPixel)
{
	QAbstractItemView *itemView =
		qobject_cast<QAbstractItemView *>(m_scrollArea);
	if(itemView) {
		itemView->setVerticalScrollMode(
			scrollPerPixel ? QAbstractItemView::ScrollPerPixel
						   : QAbstractItemView::ScrollPerItem);
	}
}

void KineticScroller::setEventFilter(QObject *target, bool install)
{
	if(target) {
		if(install) {
			target->installEventFilter(this);
		} else {
			target->removeEventFilter(this);
		}
	}
}

QScrollerProperties KineticScroller::getScrollerPropertiesForSensitivity(
	int kineticScrollSensitivity)
{
	QScrollerProperties properties;

	// DragStartDistance seems to be based on meter per second; though
	// it's not explicitly documented, other QScroller values are in
	// that metric. To start kinetic scrolling, with minimal
	// sensitivity, we expect a drag of 10 mm, with minimum sensitivity
	// any > 0 mm.
	float mm = 0.001f;
	float resistance = 1.0f - (kineticScrollSensitivity / 100.0f);
	float mousePressEventDelay = 1.0f - 0.75f * resistance;
	float resistanceCoefficient = 10.0f;
	float dragVelocitySmoothFactor = 1.0f;
	float minimumVelocity = 0.0f;
	float axisLockThresh = 1.0f;
	float maximumClickThroughVelocity = 0.0f;
	float flickAccelerationFactor = 1.5f;
	float overshootDragResistanceFactor = 0.1f;
	float overshootDragDistanceFactor = 0.3f;
	float overshootScrollDistanceFactor = 0.1f;
	float overshootScrollTime = 0.4f;

	properties.setScrollMetric(
		QScrollerProperties::DragStartDistance,
		resistance * resistanceCoefficient * mm);
	properties.setScrollMetric(
		QScrollerProperties::DragVelocitySmoothingFactor,
		dragVelocitySmoothFactor);
	properties.setScrollMetric(
		QScrollerProperties::MinimumVelocity, minimumVelocity);
	properties.setScrollMetric(
		QScrollerProperties::AxisLockThreshold, axisLockThresh);
	properties.setScrollMetric(
		QScrollerProperties::MaximumClickThroughVelocity,
		maximumClickThroughVelocity);
	properties.setScrollMetric(
		QScrollerProperties::MousePressEventDelay, mousePressEventDelay);
	properties.setScrollMetric(
		QScrollerProperties::AcceleratingFlickSpeedupFactor,
		flickAccelerationFactor);

	properties.setScrollMetric(
		QScrollerProperties::VerticalOvershootPolicy,
		QScrollerProperties::OvershootAlwaysOn);
	properties.setScrollMetric(
		QScrollerProperties::OvershootDragResistanceFactor,
		overshootDragResistanceFactor);
	properties.setScrollMetric(
		QScrollerProperties::OvershootDragDistanceFactor,
		overshootDragDistanceFactor);
	properties.setScrollMetric(
		QScrollerProperties::OvershootScrollDistanceFactor,
		overshootScrollDistanceFactor);
	properties.setScrollMetric(
		QScrollerProperties::OvershootScrollTime, overshootScrollTime);

	return properties;
}

int KineticScroller::toSensitivity(int kineticScrollThreshold)
{
	return 100 - qBound(0, kineticScrollThreshold, 100);
}

// SPDX-SnippetEnd

bool KineticScroller::isKineticScrollingEnabled(
	int kineticScrollGesture, QScroller::ScrollerGestureType *outGestureType)
{
	QScroller::ScrollerGestureType gestureType;
	switch(kineticScrollGesture) {
	case int(desktop::settings::KineticScrollGesture::LeftClick):
		gestureType = QScroller::LeftMouseButtonGesture;
		break;
	case int(desktop::settings::KineticScrollGesture::MiddleClick):
		gestureType = QScroller::MiddleMouseButtonGesture;
		break;
	case int(desktop::settings::KineticScrollGesture::RightClick):
		gestureType = QScroller::RightMouseButtonGesture;
		break;
	case int(desktop::settings::KineticScrollGesture::Touch):
		gestureType = QScroller::TouchGesture;
		break;
	default:
		return false;
	}
	if(outGestureType) {
		*outGestureType = gestureType;
	}
	return true;
}


// TODO: This needs to update when the style changes
static int indentSize(QWidget *widget)
{
	QStyle *style = widget->style();
	QStyleOption option;
	return style->subElementRect(QStyle::SE_CheckBoxContents, &option, nullptr)
		.left();
}

FormNote::FormNote(
	const QString &text, bool indent, const QIcon &icon, bool link)
	: QWidget()
	, m_text(text)
	, m_icon(icon)
	, m_link(link)
	, m_indent(indent ? indentSize(this) : 0)
{
	setContentsMargins(0, 0, 0, 0);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	if(m_link) {
		setCursor(Qt::PointingHandCursor);
	}
}

QSize FormNote::sizeHint() const
{
	return QSize(m_indent + m_iconSize + m_width, m_height);
}

QSize FormNote::minimumSizeHint() const
{
	return QSize(1, m_height);
}

void FormNote::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	QPainter painter(this);

	if(m_link) {
		QFont font = painter.font();
		font.setUnderline(true);
		painter.setFont(font);
	} else {
		painter.setOpacity(0.7);
	}

	if(!m_icon.isNull()) {
		m_icon.paint(&painter, QRect(0, 0, m_iconSize, m_iconSize));
	}

	painter.setPen(palette().color(QPalette::Text));
	painter.drawText(
		QRect(m_indent + m_iconSize, 0, m_width, m_height), m_text);
}

void FormNote::resizeEvent(QResizeEvent *event)
{
	if(!m_icon.isNull()) {
		m_iconSize =
			style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
		m_indent = style()->pixelMetric(
			QStyle::PM_LayoutHorizontalSpacing, nullptr, this);
	}
	int width = event->size().width() - m_indent - m_iconSize;
	QSize size = QFontMetrics(font())
					 .boundingRect(
						 QRect(0, 0, width <= 0 ? 0xffff : width, 0xffff),
						 Qt::TextWordWrap, m_text)
					 .size();
	if(size != QSize(m_width, m_height)) {
		m_width = size.width();
		m_height = qMax(m_iconSize, size.height());
		setFixedHeight(m_height);
	}
}

void FormNote::mousePressEvent(QMouseEvent *event)
{
	if(m_link && event->button() == Qt::LeftButton) {
		event->accept();
		emit linkClicked();
	}
}

void FormNote::mouseDoubleClickEvent(QMouseEvent *event)
{
	mousePressEvent(event);
}


void showWindow(QWidget *widget, bool maximized, bool isMainWindow)
{
	// On platforms with a single main window (Android, Emscripten), we want to
	// show it always in full screen, since there's no point in ever moving it.
#ifdef SINGLE_MAIN_WINDOW
	if(isMainWindow) {
		widget->showFullScreen();
		return;
	}
#else
	Q_UNUSED(isMainWindow);
#endif

	// On Android, we rarely want small windows unless it's like a simple
	// message box or something. Anything more complex is probably better off
	// being a full-screen window, which is also more akin to how Android's
	// native UI works. This wrapper takes care of that very common switch.
#ifdef Q_OS_ANDROID
	Q_UNUSED(maximized);
	widget->showMaximized();
#else
	if(maximized) {
		widget->showMaximized();
	} else {
		widget->show();
	}
#endif
}

void setWidgetRetainSizeWhenHidden(QWidget *widget, bool retainSize)
{
	QSizePolicy sp = widget->sizePolicy();
	sp.setRetainSizeWhenHidden(retainSize);
	widget->setSizePolicy(sp);
}

bool moveIfOnScreen(QWidget *widget, const QPoint &pos)
{
	for(QScreen *screen : dpApp().screens()) {
		if(screen->availableGeometry().contains(pos)) {
			widget->move(pos);
			return true;
		}
	}
	return false;
}

bool setGeometryIfOnScreen(QWidget *widget, const QRect &geometry)
{
	if(!geometry.isEmpty()) {
		// Make sure at least half of the window is on screen.
		int requiredWidth = qMax(1, geometry.width() / 2);
		int requiredHeight = qMax(1, geometry.height() / 2);
		for(QScreen *screen : dpApp().screens()) {
			QRect r = screen->availableGeometry().intersected(geometry);
			if(r.width() >= requiredWidth && r.height() >= requiredHeight) {
				widget->setGeometry(geometry);
				return true;
			}
		}
	}
	return false;
}

QRect moveRectToFit(const QRect &subjectRect, const QRect &boundingRect)
{
	QRect rect = subjectRect;

	if(rect.x() + rect.width() > boundingRect.x() + boundingRect.width()) {
		rect.moveRight(boundingRect.x() + boundingRect.width() - 1);
	} else if(rect.x() < boundingRect.x()) {
		rect.moveLeft(boundingRect.x());
	}

	if(rect.y() + rect.height() > boundingRect.y() + boundingRect.height()) {
		rect.moveBottom(boundingRect.y() + boundingRect.height() - 1);
	} else if(rect.y() < boundingRect.y()) {
		rect.moveTop(boundingRect.y());
	}

	return rect;
}

QRectF moveRectToFitF(const QRectF &subjectRect, const QRectF &boundingRect)
{
	QRectF rect = subjectRect;

	if(rect.x() + rect.width() > boundingRect.x() + boundingRect.width()) {
		rect.moveRight(boundingRect.x() + boundingRect.width() - 1.0);
	} else if(rect.x() < boundingRect.x()) {
		rect.moveLeft(boundingRect.x());
	}

	if(rect.y() + rect.height() > boundingRect.y() + boundingRect.height()) {
		rect.moveBottom(boundingRect.y() + boundingRect.height() - 1.0);
	} else if(rect.y() < boundingRect.y()) {
		rect.moveTop(boundingRect.y());
	}

	return rect;
}

void initSortingHeader(QHeaderView *header, int sortColumn, Qt::SortOrder order)
{
	header->setSortIndicator(sortColumn, order);
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
	header->setSortIndicatorClearable(true);
#endif
}

KineticScroller *bindKineticScrolling(QAbstractScrollArea *scrollArea)
{
	return bindKineticScrollingWith(
		scrollArea, Qt::ScrollBarAsNeeded, Qt::ScrollBarAsNeeded);
}

KineticScroller *bindKineticScrollingWith(
	QAbstractScrollArea *scrollArea,
	Qt::ScrollBarPolicy horizontalScrollBarPolicy,
	Qt::ScrollBarPolicy verticalScrollBarPolicy)
{
	if(scrollArea) {
		desktop::settings::Settings &settings = dpApp().settings();
		KineticScroller *scroller = new KineticScroller(
			scrollArea, horizontalScrollBarPolicy, verticalScrollBarPolicy,
			settings.kineticScrollGesture(), settings.kineticScrollThreshold(),
			settings.kineticScrollHideBars());
		settings.bindKineticScrollGesture(
			scroller, &KineticScroller::setKineticScrollGesture);
		settings.bindKineticScrollThreshold(
			scroller, &KineticScroller::setKineticScrollThreshold);
		settings.bindKineticScrollHideBars(
			scroller, &KineticScroller::setKineticScrollHideBars);
		return scroller;
	} else {
		return nullptr;
	}
}

QFormLayout *addFormSection(QBoxLayout *layout)
{
	QFormLayout *formLayout = new QFormLayout;
	layout->addLayout(formLayout);
	return formLayout;
}

static int getSpacerSpacingFrom(const QWidget *parent, const QStyle *style)
{
	int rawSpacing =
		style->pixelMetric(QStyle::PM_LayoutVerticalSpacing, nullptr, parent);
	// This can be negative on macOS apparently, so we use a measured value.
	return rawSpacing < 0 ? 13 : rawSpacing * 3 / 2;
}

static int getSpacerSpacing(QLayout *layout)
{
	QWidget *parent = layout->parentWidget();
	return getSpacerSpacingFrom(parent, parent->style());
}

static int getSpacing(
	bool lastIsEmpty, bool currentIsEmpty, QSizePolicy::ControlTypes last,
	QSizePolicy::ControlTypes current, Qt::Orientation orientation,
	QWidget *parent)
{
	const QStyle *style = parent ? parent->style() : QApplication::style();

	// Post-spacer
	if(lastIsEmpty && !(current & QSizePolicy::Line)) {
		return 0;
	}

	// Spacer
	if(currentIsEmpty) {
		return getSpacerSpacingFrom(parent, style);
	}

	// Widget or layout
	int spacing = style->combinedLayoutSpacing(
		last, current, orientation, nullptr, parent);

	// TODO: This is all really just hacks for the Fusion style that should
	// probably be going into a proxy style
	if(spacing == -1) {
		const QStyle::PixelMetric pm =
			(orientation == Qt::Horizontal ? QStyle::PM_LayoutHorizontalSpacing
										   : QStyle::PM_LayoutVerticalSpacing);

		spacing = style->pixelMetric(pm, nullptr, parent);

		if(orientation == Qt::Vertical) {
			if((last | current) & QSizePolicy::Line) {
				spacing *= 2;
			} else if((last & current & QSizePolicy::RadioButton)) {
				spacing = 0;
			} else if((last & current & QSizePolicy::CheckBox)) {
				spacing /= 2;
			} else if(
				(last & QSizePolicy::CheckBox) &&
				(current & QSizePolicy::ComboBox)) {
				spacing /= 2;
			}
		}
	}

	return spacing;
}

void addFormSpacer(QLayout *layout, QSizePolicy::Policy vPolicy)
{
	QSpacerItem *spacer = new QSpacerItem(
		0, getSpacerSpacing(layout), QSizePolicy::Minimum, vPolicy);
	layout->addItem(spacer);
}

QFrame *makeSeparator()
{
	QFrame *separator = new QFrame;
	separator->setForegroundRole(QPalette::Dark);
	separator->setFrameShape(QFrame::HLine);
	return separator;
}

void addFormSeparator(QBoxLayout *layout)
{
	addFormSpacer(layout);
	layout->addWidget(makeSeparator());
	addFormSpacer(layout);
}

EncapsulatedLayout *encapsulate(const QString &label, QWidget *child)
{
	auto *layout = new EncapsulatedLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	auto text = label.split("%1");

	if(text.size() > 0 && !text.first().isEmpty()) {
		auto *left = new QLabel(text.first());
		left->setBuddy(child);
		layout->addWidget(left);
	}

	layout->addWidget(child);

	if(text.size() > 1 && !text.at(1).isEmpty()) {
		auto *right = new QLabel(text.at(1));
		right->setBuddy(child);
		layout->addWidget(right);
	}

	layout->addStretch();

	return layout;
}

EncapsulatedLayout *indent(QWidget *child)
{
	EncapsulatedLayout *layout = new EncapsulatedLayout;
	layout->setSpacing(0);
	layout->setContentsMargins(indentSize(child), 0, 0, 0);
	layout->addWidget(child);
	return layout;
}

FormNote *
formNote(const QString &text, QSizePolicy::ControlType type, const QIcon &icon)
{
	return new FormNote(
		text, type == QSizePolicy::CheckBox || type == QSizePolicy::RadioButton,
		icon, false);
}

void setSpacingControlType(
	EncapsulatedLayout *layout, QSizePolicy::ControlTypes type)
{
	layout->setControlTypes(type);
}

void setSpacingControlType(QWidget *widget, QSizePolicy::ControlType type)
{
	QSizePolicy policy = widget->sizePolicy();
	policy.setControlType(type);
	widget->setSizePolicy(policy);
}

QButtonGroup *addRadioGroup(
	QFormLayout *form, const QString &label, bool horizontal,
	std::initializer_list<std::pair<const QString &, int>> items)
{
	QButtonGroup *group = new QButtonGroup(form->parentWidget());

	EncapsulatedLayout *layout = nullptr;
	if(horizontal) {
		layout = new EncapsulatedLayout;
		layout->setSpacing(getSpacing(
			false, false, QSizePolicy::RadioButton, QSizePolicy::RadioButton,
			Qt::Horizontal, form->parentWidget()));
	}

	bool first = true;
	for(const auto [text, id] : items) {
		auto *button = new QRadioButton(text);
		group->addButton(button, id);
		if(horizontal) {
			layout->addWidget(button);
		} else {
			form->addRow(first ? label : nullptr, button);
			first = false;
		}
	}

	if(horizontal) {
		layout->addStretch();
		form->addRow(label, layout);
	}

	return group;
}

QCheckBox *addCheckable(
	const QString &accessibleName, EncapsulatedLayout *layout, QWidget *child)
{
	QCheckBox *check = new QCheckBox;
	check->setAccessibleName(accessibleName);
	if(layout->count() > 0) {
		QLabel *label = qobject_cast<QLabel *>(layout->itemAt(0)->widget());
		if(label) {
			check->setText(label->text());
			layout->removeWidget(label);
			delete label;
		}
	}
	layout->insertWidget(0, check);
	child->setEnabled(false);
	QObject::connect(check, &QCheckBox::toggled, child, &QWidget::setEnabled);
	setSpacingControlType(layout, QSizePolicy::CheckBox);
	return check;
}

QLabel *makeIconLabel(const QIcon &icon, QWidget *parent)
{
	QLabel *label = new QLabel;
	QWidget *widget = parent ? parent : label;
	QStyle *style = widget->style();
	int labelSize =
		style->pixelMetric(QStyle::PM_LargeIconSize, nullptr, widget);
	label->setPixmap(icon.pixmap(labelSize));
	label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	return label;
}

QMessageBox *makeMessage(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText, QMessageBox::Icon icon,
	QMessageBox::StandardButtons buttons)
{
	QMessageBox *msgbox = new QMessageBox(parent);
	msgbox->setAttribute(Qt::WA_DeleteOnClose);
	msgbox->setWindowModality(Qt::ApplicationModal);
	msgbox->setIcon(icon);
	msgbox->setWindowTitle(title);
	msgbox->setText(text);
	msgbox->setInformativeText(informativeText);
	msgbox->setStandardButtons(buttons);
	return msgbox;
}

QMessageBox *makeQuestion(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText)
{
	return makeMessage(
		parent, title, text, informativeText, QMessageBox::Question,
		QMessageBox::Yes | QMessageBox::No);
}

QMessageBox *makeInformation(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText)
{
	return makeMessage(
		parent, title, text, informativeText, QMessageBox::Information,
		QMessageBox::Ok);
}

QMessageBox *makeInformationWithSaveButton(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText)
{
	return makeMessage(
		parent, title, text, informativeText, QMessageBox::Information,
		QMessageBox::Save);
}

QMessageBox *makeWarning(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText)
{
	return makeMessage(
		parent, title, text, informativeText, QMessageBox::Warning,
		QMessageBox::Ok);
}

QMessageBox *makeCritical(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText)
{
	return makeMessage(
		parent, title, text, informativeText, QMessageBox::Critical,
		QMessageBox::Ok);
}

QMessageBox *showQuestion(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText)
{
	QMessageBox *msgbox = makeQuestion(parent, title, text, informativeText);
	msgbox->show();
	return msgbox;
}

QMessageBox *showInformation(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText)
{
	QMessageBox *msgbox = makeInformation(parent, title, text, informativeText);
	msgbox->show();
	return msgbox;
}

QMessageBox *showWarning(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText)
{
	QMessageBox *msgbox = makeWarning(parent, title, text, informativeText);
	msgbox->show();
	return msgbox;
}

QMessageBox *showCritical(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText)
{
	QMessageBox *msgbox = makeCritical(parent, title, text, informativeText);
	msgbox->show();
	return msgbox;
}

QString makeActionShortcutText(QString text, const QKeySequence &shortcut)
{
	static const QRegularExpression acceleratorRegex(QStringLiteral("&([^&])"));
	text.replace(acceleratorRegex, QStringLiteral("\\1"));

	// In languages with non-latin alphabets, it's a common
	// convention to add a keyboard shortcut like this:
	// English: &File
	// Japanese: ファイル(&F)
	int i = text.lastIndexOf('(');
	if(i > 0) {
		text.truncate(i);
	}

	if(shortcut.isEmpty()) {
		return text;
	} else {
		//: This makes an action and a keyboard shortcut, like "Undo
		//: (Ctrl+Z)". %1 is the action, %2 is the shortcut. You only
		//: need to change this if your language uses different spaces
		//: or parentheses, otherwise just leave it as-is.
		return QCoreApplication::translate("MainWindow", "%1 (%2)")
			.arg(text, shortcut.toString(QKeySequence::NativeText));
	}
}

namespace {
static void getInputTextWith(
	QWidget *parent, const QString &title, const QString &label,
	const QString &text, const std::function<void(const QString &)> &fn,
	QLineEdit::EchoMode echoMode)
{
	QInputDialog *dlg = new QInputDialog(parent);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setWindowTitle(title);
	dlg->setLabelText(label);
	dlg->setInputMode(QInputDialog::TextInput);
	dlg->setTextEchoMode(echoMode);
	dlg->setTextValue(text);
	QObject::connect(dlg, &QInputDialog::textValueSelected, parent, fn);
	dlg->show();
}
}

void getInputText(
	QWidget *parent, const QString &title, const QString &label,
	const QString &text, const std::function<void(const QString &)> &fn)
{
	getInputTextWith(parent, title, label, text, fn, QLineEdit::Normal);
}

void getInputPassword(
	QWidget *parent, const QString &title, const QString &label,
	const QString &text, const std::function<void(const QString &)> &fn)
{
	getInputTextWith(parent, title, label, text, fn, QLineEdit::Password);
}

void getInputInt(
	QWidget *parent, const QString &title, const QString &label, int value,
	int minValue, int maxValue, const std::function<void(int)> &fn)
{
	QInputDialog *dlg = new QInputDialog(parent);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setWindowTitle(title);
	dlg->setLabelText(label);
	dlg->setInputMode(QInputDialog::IntInput);
	dlg->setIntValue(value);
	dlg->setIntMinimum(minValue);
	dlg->setIntMaximum(maxValue);
	QObject::connect(dlg, &QInputDialog::intValueSelected, parent, fn);
	dlg->show();
}

bool openOrQuestionUrl(QWidget *parent, const QUrl &url)
{
	if(!url.isValid() || url.isLocalFile() || url.isRelative()) {
		return false;
	}

	QString scheme = url.scheme();
	if((scheme.compare(QStringLiteral("http"), Qt::CaseInsensitive) != 0 &&
		scheme.compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0)) {
		return false;
	}

	QString host = url.host();
	if(host.isEmpty()) {
		return false;
	}

	if(host.compare(QStringLiteral("drawpile.net"), Qt::CaseInsensitive) == 0 ||
	   host.endsWith(QStringLiteral(".drawpile.net"), Qt::CaseInsensitive)) {
		QDesktopServices::openUrl(url);
	} else {
		qDebug("unsafe");
		QMessageBox *box = makeQuestion(
			parent, QCoreApplication::translate("LinkCheck", "Open Link"),
			QCoreApplication::translate(
				"LinkCheck",
				"This link will take you to the website at \"%1\", which is "
				"not run by Drawpile. Are you sure you want to go there?")
				.arg(host));
		box->setTextFormat(Qt::PlainText);
		box->button(QMessageBox::Yes)
			->setText(
				QCoreApplication::translate("LinkCheck", "Yes, visit website"));
		box->button(QMessageBox::No)
			->setText(QCoreApplication::translate("LinkCheck", "No, cancel"));
		QObject::connect(
			box, &QMessageBox::accepted, parent,
			std::bind(&QDesktopServices::openUrl, url));
		box->show();
	}
	return true;
}

QIcon makeColorIcon(int size, const QColor &color)
{
	int half = size / 2;
	QPixmap pixmap(size, size);
	pixmap.fill(color);
	if(color.alpha() != 255) {
		QPainter painter{&pixmap};
		painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
		painter.fillRect(0, 0, half, half, Qt::gray);
		painter.fillRect(half, 0, half, half, Qt::white);
		painter.fillRect(0, half, half, half, Qt::white);
		painter.fillRect(half, half, half, half, Qt::gray);
	}
	return QIcon(pixmap);
}

QIcon makeColorIconFor(const QWidget *parent, const QColor &color)
{
	Q_ASSERT(parent);
	return makeColorIcon(
		parent->style()->pixelMetric(QStyle::PM_ButtonIconSize), color);
}

}
