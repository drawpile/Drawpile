// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/widgetutils.h"
#include "desktop/main.h"
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCursor>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHeaderView>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPair>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWidget>

namespace utils {

ScopedOverrideCursor::ScopedOverrideCursor()
	: ScopedOverrideCursor(QCursor(Qt::WaitCursor))
{
}

ScopedOverrideCursor::ScopedOverrideCursor(const QCursor &cursor)
{
	QApplication::setOverrideCursor(cursor);
}

ScopedOverrideCursor::~ScopedOverrideCursor()
{
	QApplication::restoreOverrideCursor();
}

ScopedUpdateDisabler::ScopedUpdateDisabler(QWidget *widget)
	: m_widget{widget}
	, m_wasEnabled{widget->updatesEnabled()}
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


// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-3.0-or-later
// SDPX—SnippetName: Kinetic scroll event filter from Krita
KisKineticScrollerEventFilter::KisKineticScrollerEventFilter(
	QScroller::ScrollerGestureType gestureType, QAbstractScrollArea *parent)
	: QObject(parent)
	, m_scrollArea(parent)
	, m_gestureType(gestureType)
{
}

bool KisKineticScrollerEventFilter::eventFilter(QObject *watched, QEvent *event)
{
	switch(event->type()) {
	case QEvent::Enter:
		QScroller::ungrabGesture(m_scrollArea);
		break;
	case QEvent::Leave:
		QScroller::grabGesture(m_scrollArea, m_gestureType);
		break;
	default:
		break;
	}
	return QObject::eventFilter(watched, event);
}
// SPDX-SnippetEnd


void showWindow(QWidget *widget, bool maximized)
{
	// On Android, we rarely want small windows unless it's like a simple
	// message box or something. Anything more complex is probably better off
	// being a full-screen window, which is also more akin to how Android's
	// native UI works. This wrapper takes care of that very common switch.
#ifdef Q_OS_ANDROID
	Q_UNUSED(maximized);
	widget->showFullScreen();
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

void initSortingHeader(QHeaderView *header)
{
	header->setSortIndicator(-1, Qt::AscendingOrder);
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
	header->setSortIndicatorClearable(true);
#endif
}

static QPair<bool, QScroller::ScrollerGestureType>
getKineticScrollEnabledGesture(const desktop::settings::Settings &settings)
{
	switch(settings.kineticScrollGesture()) {
	case int(desktop::settings::KineticScrollGesture::LeftClick):
		return {true, QScroller::LeftMouseButtonGesture};
	case int(desktop::settings::KineticScrollGesture::MiddleClick):
		return {true, QScroller::MiddleMouseButtonGesture};
	case int(desktop::settings::KineticScrollGesture::RightClick):
		return {true, QScroller::RightMouseButtonGesture};
	case int(desktop::settings::KineticScrollGesture::Touch):
		return {true, QScroller::TouchGesture};
	default:
		return {false, QScroller::ScrollerGestureType(0)};
	}
}

void initKineticScrolling(QAbstractScrollArea *scrollArea)
{
	const desktop::settings::Settings &settings = dpApp().settings();
	auto [enabled, gestureType] = getKineticScrollEnabledGesture(settings);

	// SPDX-SnippetBegin
	// SPDX-License-Identifier: GPL-3.0-or-later
	// SDPX—SnippetName: Kinetic scroll setup from Krita
	if(scrollArea && enabled) {
		int sensitivity =
			100 - qBound(0, settings.kineticScrollThreshold(), 100);
		bool hideScrollBars = settings.kineticScrollHideBars();
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

		if(hideScrollBars) {
			scrollArea->setVerticalScrollBarPolicy(
				Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
			scrollArea->setHorizontalScrollBarPolicy(
				Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
		} else if(gestureType != QScroller::TouchGesture) {
			auto *filter =
				new KisKineticScrollerEventFilter(gestureType, scrollArea);
			scrollArea->horizontalScrollBar()->installEventFilter(filter);
			scrollArea->verticalScrollBar()->installEventFilter(filter);
		}

		QAbstractItemView *itemView =
			qobject_cast<QAbstractItemView *>(scrollArea);
		if(itemView) {
			itemView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
		}

		QScroller *scroller = QScroller::scroller(scrollArea);
		// TODO: this leaks memory, see QTBUG-82355.
		QScroller::grabGesture(scrollArea, gestureType);

		QScrollerProperties properties;

		// DragStartDistance seems to be based on meter per second; though
		// it's not explicitly documented, other QScroller values are in
		// that metric. To start kinetic scrolling, with minimal
		// sensitivity, we expect a drag of 10 mm, with minimum sensitivity
		// any > 0 mm.
		const float mm = 0.001f;
		const float resistance = 1.0f - (sensitivity / 100.0f);
		const float mousePressEventDelay = 1.0f - 0.75f * resistance;

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

		scroller->setScrollerProperties(properties);
	}
	// SPDX-SnippetEnd
}

bool isKineticScrollingBarsHidden()
{
	const desktop::settings::Settings &settings = dpApp().settings();
	return getKineticScrollEnabledGesture(settings).first &&
		   settings.kineticScrollHideBars();
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

// TODO: This needs to update when the style changes
static int indentSize(QWidget *widget)
{
	QStyle *style = widget->style();
	QStyleOption option;
	return style->subElementRect(QStyle::SE_CheckBoxContents, &option, nullptr)
		.left();
}

EncapsulatedLayout *indent(QWidget *child)
{
	EncapsulatedLayout *layout = new EncapsulatedLayout;
	layout->setSpacing(0);
	layout->setContentsMargins(indentSize(child), 0, 0, 0);
	layout->addWidget(child);
	return layout;
}

// QLabel refuses to behave, it will just take up excessive vertical space when
// word wrap is enabled despite telling it not to. So we make our own widget
// that just draws some slightly opaque, wrapped text sanely.
class Note final : public QWidget {
public:
	Note(const QString &text, bool indent, const QIcon &icon)
		: QWidget()
		, m_text(text)
		, m_indent(indent ? indentSize(this) : 0)
		, m_icon(icon)
	{
		setContentsMargins(0, 0, 0, 0);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	}

	QSize sizeHint() const override
	{
		return QSize(m_indent + m_iconSize + m_width, m_height);
	}

	QSize minimumSizeHint() const override { return QSize(1, m_height); }

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter painter(this);
		painter.setOpacity(0.7);
		if(!m_icon.isNull()) {
			m_icon.paint(&painter, QRect(0, 0, m_iconSize, m_iconSize));
		}
		painter.setPen(palette().color(QPalette::Text));
		painter.drawText(
			QRect(m_indent + m_iconSize, 0, m_width, m_height), m_text);
	}

	void resizeEvent(QResizeEvent *event) override
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

private:
	QString m_text;
	int m_indent;
	QIcon m_icon;
	int m_iconSize = 0;
	int m_width = 1;
	int m_height = 1;
};

QWidget *
formNote(const QString &text, QSizePolicy::ControlType type, const QIcon &icon)
{
	return new Note(
		text, type == QSizePolicy::CheckBox || type == QSizePolicy::RadioButton,
		icon);
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

// TODO: This needs to update when the style changes
static int checkBoxLabelSpacing(const QWidget *widget)
{
	const QStyle *style = widget->style();

	// This has to be done the stupid way instead of just asking for the pixel
	// metric because at least QCommonStyle has an off-by-one error from using
	// `QRect::right` without correct for that being an inclusive coordinate
	QStyleOption option;
	int l =
		style->subElementRect(QStyle::SE_CheckBoxIndicator, &option, nullptr)
			.width();
	int r = style->subElementRect(QStyle::SE_CheckBoxContents, &option, nullptr)
				.left();

	return r - l;
}

QCheckBox *addCheckable(
	const QString &accessibleName, EncapsulatedLayout *layout, QWidget *child)
{
	QCheckBox *check = new QCheckBox;
	check->setAccessibleName(accessibleName);
	layout->insertWidget(0, check);
	// TODO: This needs to update when the style changes
	layout->insertSpacing(1, utils::checkBoxLabelSpacing(child));
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

}
