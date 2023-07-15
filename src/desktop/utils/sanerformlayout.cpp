// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/utils/sanerformlayout.h"
#include "desktop/utils/qtguicompat.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDebug>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOption>
#include <QWidget>
#include <Qt>

namespace utils {

// TODO: This needs to update when the style changes
int checkBoxLabelSpacing(const QWidget *widget)
{
	const auto *style = widget->style();

	// This has to be done the stupid way instead of just asking for the pixel
	// metric because at least QCommonStyle has an off-by-one error from using
	// `QRect::right` without correct for that being an inclusive coordinate
	QStyleOption option;
	const auto l = style->subElementRect(QStyle::SE_CheckBoxIndicator, &option, nullptr).width();
	const auto r = style->subElementRect(QStyle::SE_CheckBoxContents, &option, nullptr).left();

	return r - l;
}

QCheckBox *checkable(const QString &accessibleName, EncapsulatedLayout *layout, QWidget *child)
{
	auto *check = new QCheckBox;
	check->setAccessibleName(accessibleName);
	layout->insertWidget(0, check);
	// TODO: This needs to update when the style changes
	layout->insertSpacing(1, utils::checkBoxLabelSpacing(child));
	child->setEnabled(false);
	QObject::connect(check, &QCheckBox::toggled, child, &QWidget::setEnabled);
	setSpacingControlType(layout, QSizePolicy::CheckBox);
	return check;
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
static auto indentSize(QWidget *widget)
{
	const auto *style = widget->style();
	QStyleOption option;
	return style->subElementRect(QStyle::SE_CheckBoxContents, &option, nullptr).left();
}

EncapsulatedLayout *indent(QWidget *child)
{
	auto *layout = new EncapsulatedLayout;
	layout->setSpacing(0);
	layout->setContentsMargins(indentSize(child), 0, 0, 0);
	layout->addWidget(child);
	return layout;
}

EncapsulatedLayout *note(const QString &text, QSizePolicy::ControlType type)
{
	auto *label = new QLabel(text);
	label->setWordWrap(true);
	label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	label->setGraphicsEffect(new QGraphicsOpacityEffect);

	// This is required to not make the label take up excessive vertical space.
	auto font = QFont();
	font.setPointSize(font.pointSize());
	label->setFont(font);

	utils::setSpacingControlType(label, type);

	switch (type) {
	case QSizePolicy::CheckBox:
	case QSizePolicy::RadioButton:
		return utils::indent(label);
	default: {
		auto *layout = new EncapsulatedLayout;
		layout->setContentsMargins(0, 0, 0, 0);
		layout->addWidget(label);
		layout->setControlTypes(QSizePolicy::CheckBox);
		return layout;
	}
	}
}

QFrame *makeSeparator()
{
	auto *separator = new QFrame;
	separator->setForegroundRole(QPalette::Dark);
	separator->setFrameShape(QFrame::HLine);
	return separator;
}

QLabel *makeIconLabel(QStyle::StandardPixmap icon, QStyle::PixelMetric size, QWidget *parent)
{
	auto *label = new QLabel;
	auto *widget = parent ? parent : label;
	auto *style = widget->style();
	auto labelIcon = style->standardIcon(icon, nullptr, widget);
	auto labelSize = style->pixelMetric(size, nullptr, widget);
	label->setPixmap(labelIcon.pixmap(labelSize));
	label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	return label;
}

void setSpacingControlType(EncapsulatedLayout *layout, QSizePolicy::ControlTypes type)
{
	layout->setControlTypes(type);
}

void setSpacingControlType(QWidget *widget, QSizePolicy::ControlType type)
{
	auto policy = widget->sizePolicy();
	policy.setControlType(type);
	widget->setSizePolicy(policy);
}

static auto getSpacing(
	bool lastIsEmpty,
	bool currentIsEmpty,
	QSizePolicy::ControlTypes last,
	QSizePolicy::ControlTypes current,
	Qt::Orientation orientation,
	QWidget *parent
)
{
	const auto *style = parent ? parent->style() : QApplication::style();

	// Post-spacer
	if (lastIsEmpty && !(current & QSizePolicy::Line)) {
		return 0;
	}

	// Spacer
	if (currentIsEmpty) {
		auto spacing = style->pixelMetric(QStyle::PM_LayoutVerticalSpacing, nullptr, parent);
		if (spacing == -1) {
			// TODO: This is measured from the General pane in macOS System
			// Preferences and then adjusted down to end up being roughly 18
			// between controls
			spacing = 13;
		} else {
			spacing = spacing * 3 / 2;
		}
		return spacing;
	}

	// Widget or layout
	auto spacing = style->combinedLayoutSpacing(
		last, current, orientation, nullptr, parent
	);

	// TODO: This is all really just hacks for the Fusion style that should
	// probably be going into a proxy style
	if (spacing == -1) {
		const auto pm = (orientation == Qt::Horizontal
			? QStyle::PM_LayoutHorizontalSpacing
			: QStyle::PM_LayoutVerticalSpacing);

		spacing = style->pixelMetric(pm, nullptr, parent);

		if (orientation == Qt::Vertical) {
			if ((last | current) & QSizePolicy::Line) {
				spacing *= 2;
			} else if ((last & current & QSizePolicy::RadioButton)) {
				spacing = 0;
			} else if ((last & current & QSizePolicy::CheckBox)) {
				spacing /= 2;
			} else if ((last & QSizePolicy::CheckBox) && (current & QSizePolicy::ComboBox)) {
				spacing /= 2;
			}
		}
	}

	return spacing;
}

QSizePolicy::ControlTypes EncapsulatedLayout::controlTypes() const
{
	if (m_controlTypes) {
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

	if (alignment() & Qt::AlignHorizontal_Mask) {
		maxSize.setWidth(QLAYOUTSIZE_MAX);
	}
	if (alignment() & Qt::AlignVertical_Mask) {
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
	if (m_topMargin || m_bottomMargin) {
		for (auto i = 0; i < c; ++i) {
			auto *item = itemAt(i);
			if (auto *widget = item->widget()) {
				auto oldGeometry = widget->property("encapsulatedLayoutGeometry").toRect();
				if (oldGeometry.isValid()) {
					widget->setGeometry(oldGeometry);
				}
			}
		}
	}

	// This fixes the geometry of the layout box
	QHBoxLayout::setGeometry(rect.adjusted(
		-m_leftMargin,
		-m_topMargin,
		-m_rightMargin,
		-m_bottomMargin
	));

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
	if (m_topMargin || m_bottomMargin) {
		for (auto i = 0; i < c; ++i) {
			auto *item = itemAt(i);
			if (auto *widget = item->widget()) {
				widget->setProperty("encapsulatedLayoutGeometry", widget->geometry());
				item->setGeometry(item->geometry().adjusted(
					0, -m_topMargin, 0, m_topMargin + m_bottomMargin
				));
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
	if (height == -1) {
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
	if (m_dirty) {
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
	if (direction() == QBoxLayout::RightToLeft) {
		std::swap(first, last);
	}
	if (auto *widget = first->widget()) {
		m_leftMargin = first->geometry().left() - widget->geometry().left();
	}
	if (auto *widget = last->widget()) {
		m_rightMargin = last->geometry().right() - widget->geometry().right();
	}
	for (auto i = 0; i < c; ++i) {
		auto *item = itemAt(i);
		if (auto *widget = item->widget()) {
			const auto itemGeom = item->geometry();
			const auto widgetGeom = widget->geometry();
			m_topMargin = qMax(m_topMargin, itemGeom.top() - widgetGeom.top());
			m_bottomMargin = qMax(m_bottomMargin, widgetGeom.bottom() - itemGeom.bottom());
		}
	}
}
#endif

SanerFormLayout::SanerFormLayout(QWidget *parent)
	: QGridLayout(parent)
{
	// Spacers will be manually added for vertical spacing and forced for
	// horizontal spacing since Qt has no idea what it is doing and will break
	// the layout otherwise
	setVerticalSpacing(0);
	setHorizontalSpacing(style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing, nullptr, parent));

	setColumnStretch(1, 2);
	setColumnStretch(2, 0);
}

static void sanitizeAsideRows(int &row, int &rowSpan, int rowCount)
{
	if (row >= rowCount) {
		qWarning() << "SanerFormLayout::addAside: row" << row << "is out of bounds";
		row = qMax(0, rowCount - 1);
	}

	bool autoRowSpan = (rowSpan == -1);

	if (!autoRowSpan && row + rowSpan > rowCount) {
		qWarning() << "SanerFormLayout::addAside: rowSpan" << rowSpan << "is out of bounds";
		autoRowSpan = true;
	}

	if (autoRowSpan) {
		rowSpan = rowCount - row;
	}
}

static inline int gridRow(int row) { return row * 2; }
static inline int gridRowSpan(int rowSpan) { return qMax(1, rowSpan * 2 - 1); }

void SanerFormLayout::addAside(QLayout *layout, int row, int rowSpan)
{
	sanitizeAsideRows(row, rowSpan, rowCount());
	addLayout(layout, gridRow(row), 2, gridRowSpan(rowSpan), 1);
	invalidate();
}

void SanerFormLayout::addAside(QWidget *widget, int row, int rowSpan)
{
	sanitizeAsideRows(row, rowSpan, rowCount());
	addWidget(widget, gridRow(row), 2, gridRowSpan(rowSpan), 1);
	invalidate();
}


QButtonGroup *SanerFormLayout::addRadioGroup(const QString &label, bool horizontal, std::initializer_list<std::pair<const QString &, int>> items, int colSpan)
{
	auto *group = new QButtonGroup(this);

	EncapsulatedLayout *layout = nullptr;
	if (horizontal) {
		layout = new EncapsulatedLayout;
		layout->setSpacing(getSpacing(
			false, false,
			QSizePolicy::RadioButton,
			QSizePolicy::RadioButton,
			Qt::Horizontal,
			parentWidget()
		));
	}

	auto first = true;
	for (const auto [text, id] : items) {
		auto *button = new QRadioButton(text);
		group->addButton(button, id);
		if (horizontal) {
			layout->addWidget(button);
		} else {
			addRow(first ? label : nullptr, button, 1, colSpan);
			first = false;
		}
	}

	if (horizontal) {
		layout->addStretch();
		addRow(label, layout, 1, colSpan);
	}

	return group;
}

void SanerFormLayout::addRow(const QString &label, QLayout *layout, int rowSpan, int colSpan, Qt::Alignment alignment)
{
	QLabel *labelWidget = nullptr;
	if (!label.isEmpty()) {
		labelWidget = new QLabel(label);
	}
	addRow(labelWidget, layout, rowSpan, colSpan, alignment);
}

void SanerFormLayout::addRow(const QString &label, QWidget *widget, int rowSpan, int colSpan, Qt::Alignment alignment)
{
	QLabel *labelWidget = nullptr;
	if (!label.isEmpty()) {
		labelWidget = new QLabel(label);
		labelWidget->setBuddy(widget);
	}
	addRow(labelWidget, widget, rowSpan, colSpan, alignment);
}

void SanerFormLayout::addRow(QWidget *label, QLayout *layout, int rowSpan, int colSpan, Qt::Alignment alignment)
{
	const auto row = addRowSpacing();
	if (label) {
		label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
		label->setAttribute(Qt::WA_PaintUnclipped);
		addWidget(label, row, 0, labelAlignment());
	}
	addLayout(layout, row, 1, gridRowSpan(rowSpan), colSpan, alignment);
	invalidate();
}

void SanerFormLayout::addRow(QWidget *label, QWidget *widget, int rowSpan, int colSpan, Qt::Alignment alignment)
{
	const auto row = addRowSpacing();
	if (label) {
		label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
		addWidget(label, row, 0, labelAlignment());
	}
	addWidget(widget, row, 1, gridRowSpan(rowSpan), colSpan, alignment);
	invalidate();
}

QFrame *SanerFormLayout::addSeparator()
{
	const auto row = addRowSpacing();
	auto *separator = makeSeparator();
	addWidget(separator, row, 0, 1, 3);
	invalidate();
	return separator;
}

void SanerFormLayout::addSpacer(QSizePolicy::Policy vPolicy, int colSpan)
{
	auto *spacer = new QSpacerItem(0, 0, QSizePolicy::Fixed, vPolicy);
	const auto row = addRowSpacing();
	addItem(spacer, row, 0, 1, colSpan);
	invalidate();
}

void SanerFormLayout::addSpanningRow(QLayout *layout, Qt::Alignment alignment)
{
	const auto row = addRowSpacing();
	addLayout(layout, row, 0, 1, 3, alignment);
	invalidate();
}

void SanerFormLayout::addSpanningRow(QWidget *widget, Qt::Alignment alignment)
{
	const auto row = addRowSpacing();
	addWidget(widget, row, 0, 1, 3, alignment);
	invalidate();
}

Qt::Orientations SanerFormLayout::expandingDirections() const
{
	return Qt::Horizontal;
}

void SanerFormLayout::invalidate()
{
	m_dirtyLayout = true;
	m_dirtySpacers = true;
	QGridLayout::invalidate();
}

QSize SanerFormLayout::maximumSize() const
{
	if (m_dirtySpacers) {
		m_dirtySpacers = false;
		const_cast<SanerFormLayout *>(this)->updateLayout(true, false);
	}
	return QGridLayout::maximumSize();
}

QSize SanerFormLayout::minimumSize() const
{
	if (m_dirtySpacers) {
		m_dirtySpacers = false;
		const_cast<SanerFormLayout *>(this)->updateLayout(true, false);
	}
	return QGridLayout::minimumSize();
}

void SanerFormLayout::setGeometry(const QRect &rect)
{
	if (m_dirtyLayout || rect != geometry()) {
		m_dirtyLayout = false;
		// Spacers have to be updated before `QGridLayout::setGeometry` because
		// it caches sizes and so will give wrong values for `cellRect` when
		// relayout occurs due to a resize
		if (m_dirtySpacers) {
			m_dirtySpacers = false;
			updateLayout(true, false);
		}
		QGridLayout::setGeometry(rect);
		updateLayout(false, true);
	}
}

QSize SanerFormLayout::sizeHint() const
{
	if (m_dirtySpacers) {
		m_dirtySpacers = false;
		const_cast<SanerFormLayout *>(this)->updateLayout(true, false);
	}
	return QGridLayout::sizeHint();
}

SanerFormLayout::FieldGrowthPolicy SanerFormLayout::fieldGrowthPolicy() const
{
	auto policy = m_fieldGrowthPolicy;
	if (policy == -1) {
		policy = style()->styleHint(QStyle::SH_FormLayoutFieldGrowthPolicy, nullptr, parentWidget());
	}
	return FieldGrowthPolicy(policy);
}

void SanerFormLayout::setFieldGrowthPolicy(FieldGrowthPolicy policy)
{
	if (m_fieldGrowthPolicy != policy) {
		m_fieldGrowthPolicy = policy;
		invalidate();
	}
}

Qt::Alignment SanerFormLayout::formAlignment() const
{
	auto alignment = m_formAlignment;
	if (!m_formAlignment) {
		alignment = Qt::Alignment(style()->styleHint(QStyle::SH_FormLayoutFormAlignment, nullptr, parentWidget()));
	}
	return alignment;
}

void SanerFormLayout::setFormAlignment(Qt::Alignment alignment)
{
	if (m_formAlignment != alignment) {
		m_formAlignment = alignment;
		invalidate();
	}
}

Qt::Alignment SanerFormLayout::labelAlignment() const
{
	auto alignment = m_labelAlignment;
	if (!m_labelAlignment) {
		alignment = Qt::Alignment(style()->styleHint(QStyle::SH_FormLayoutLabelAlignment, nullptr, parentWidget()));
	}
	return alignment;
}

void SanerFormLayout::setLabelAlignment(Qt::Alignment alignment)
{
	if (m_labelAlignment != alignment) {
		m_labelAlignment = alignment;
		invalidate();
	}
}

bool SanerFormLayout::stretchLabel() const
{
	return m_stretchLabel;
}

void SanerFormLayout::setStretchLabel(bool stretch)
{
	if (m_stretchLabel != stretch) {
		m_stretchLabel = stretch;
		invalidate();
	}
}

int SanerFormLayout::addRowSpacing()
{
	const auto row = gridRowCount();
	if (row == 0) {
		return row;
	}

	auto *last = itemAtPosition(row - 1, 1);
	if (!last) {
		// QGridLayout always starts with one row that contains nothing
		// and in that case we shall adopt that row
		if (row == 1) {
			return row - 1;
		}

		qWarning() << "SanerFormLayout::addRow: discontiguous layout at" << row;
		return row;
	}

	// It is not necessary to calculate the spacing for the rows yet since
	// the layout will be invalidated anyway when the geometry is set.
	addItem(new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed), row, 0, 1, 2);

	return row + 1;
}

QStyle *SanerFormLayout::style() const
{
	return parentWidget() ? parentWidget()->style() : QApplication::style();
}

void SanerFormLayout::updateLayout(bool updateSpacers, bool updateFields)
{
	if (updateFields) {
		if (alignment() != formAlignment()) {
			// TODO: This causes invalidation in QGridLayout
			setAlignment(formAlignment());
			invalidate();
		}
		const auto stretch = stretchLabel() && labelAlignment().testFlag(Qt::AlignRight) ? 1 : 0;
		if (columnStretch(0) != stretch) {
			// TODO: This causes invalidation in QGridLayout
			setColumnStretch(0, stretch);
			invalidate();
		}
	}

	const auto growthPolicy = fieldGrowthPolicy();
	const auto growAll = (growthPolicy == QFormLayout::AllNonFixedFieldsGrow);
	const auto growDir = (growthPolicy == QFormLayout::ExpandingFieldsGrow) ? Qt::Horizontal : Qt::Orientations();
	const auto alignment = labelAlignment();
	const auto rows = gridRowCount();
	QLayoutItem *lastField = nullptr;
	for (auto row = 0; row < rows; ++row) {
		auto *firstItem = itemAtPosition(row, 0);

		if (row % 2) {
			if (!updateSpacers) {
				continue;
			}

			if (!firstItem || !firstItem->spacerItem()) {
				qWarning()
					<< "SanerFormLayout::updateLayout: missing spacer at"
					<< row;
				continue;
			}

			// `QWidgetItem::widget` is not const in Qt5 so this cannot be
			// either
			auto *nextField = itemAtPosition(row + 1, 1);

			if (!nextField) {
				qWarning()
					<< "SanerFormLayout::updateLayout: discontiguous layout at"
					<< row;
				continue;
			}

			const auto isHiddenWidget = nextField->isEmpty() && nextField->widget();

			int spacing = 0;
			if (lastField && !isHiddenWidget) {
				spacing = getSpacing(
					lastField->isEmpty(),
					nextField->isEmpty(),
					lastField->controlTypes(),
					nextField->controlTypes(),
					Qt::Vertical,
					parentWidget()
				);
			}

			firstItem->spacerItem()->changeSize(0, spacing, QSizePolicy::Fixed, QSizePolicy::Fixed);

			// Leave this here to make it easier to debug again in the future, unless
			// it has been like a year of everything working perfectly
			// if (nextField && lastField) {
			// 	qDebug() << "row spacing" << spacing
			// 		<< "at" << row
			// 		<< lastField->isEmpty() << nextField->isEmpty()
			// 		<< "for" << nextField->widget()
			// 		<< lastField->controlTypes() << nextField->controlTypes();
			// }
		} else {
			auto *field = itemAtPosition(row, 1);

			// Hidden widgets should not be considered when calculating new
			// spacing, but any other empty things like spacers should
			if (field && field->isEmpty() && field->widget()) {
				continue;
			}

			lastField = field;

			if (!updateFields) {
				continue;
			}

			// Spanning items, like separators and spacers, are not form fields
			if (firstItem == field) {
				continue;
			}

			if (!field || field->isEmpty()) {
				qWarning()
					<< "SanerFormLayout::updateLayout: missing field at"
					<< row;
				continue;
			}

			if (auto *label = firstItem) {
				label->setAlignment(alignment);
			}

			const auto grow = growAll || field->expandingDirections() & growDir;
			const auto shrink = !grow && !(field->expandingDirections() & Qt::Horizontal);

			if (grow || shrink) {
				auto rect = cellRect(row, 1);
				if (itemAtPosition(row, 2) == field) {
					rect = rect.united(cellRect(row, 2));
				}

				if (grow) {
					field->setGeometry(rect);
				} else {
					field->setGeometry({ rect.topLeft(), field->sizeHint() });
				}
			}
		}
	}
}

} // namespace utils
