// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_UTILS_SANERFORMLAYOUT_H
#define DESKTOP_UTILS_SANERFORMLAYOUT_H

#include <QFormLayout>
#include <QGridLayout>
#include <QSize>
#include <QSizePolicy>
#include <Qt>
#include <optional>
#include <utility>

class QButtonGroup;
class QCheckBox;
class QFrame;
class QHBoxLayout;
class QLayout;
class QString;
class QStyle;
class QWidget;

namespace utils {

class EncapsulatedLayout;

int checkBoxLabelSpacing(const QWidget *widget);
QCheckBox *checkable(const QString &accessibleName, EncapsulatedLayout *layout, QWidget *child);
EncapsulatedLayout *encapsulate(const QString &label, QWidget *child);
EncapsulatedLayout *indent(QWidget *child);
EncapsulatedLayout *note(const QString &text, QSizePolicy::ControlType type);
QGridLayout *labelEdges(QWidget *child, const QString &left, const QString &right);
QFrame *makeSeparator();
void setSpacingControlType(EncapsulatedLayout *widget, QSizePolicy::ControlTypes type);
void setSpacingControlType(QWidget *widget, QSizePolicy::ControlType type);

/// @brief Like `QHBoxLayout`, but allows the control type to be overridden to
/// provide more sensible spacing, and on macOS, fixes broken layouts caused by
/// fake extra layout margins.
class EncapsulatedLayout final : public QHBoxLayout {
	Q_OBJECT
public:
	using QHBoxLayout::QHBoxLayout;

	QSizePolicy::ControlTypes controlTypes() const override;
	void setControlTypes(QSizePolicy::ControlTypes controlTypes);

private:
	std::optional<QSizePolicy::ControlTypes> m_controlTypes;

#ifdef Q_OS_MACOS
public:
	int heightForWidth(int width) const override;
	void invalidate() override;
	QSize maximumSize() const override;
	int minimumHeightForWidth(int width) const override;
	QSize minimumSize() const override;
	void setGeometry(const QRect &rect) override;
	QSize sizeHint() const override;

private:
	int adjustHeightForWidth(int width) const;
	QSize adjustSizeHint(QSize size) const;
	void recoverEffectiveMargins() const;

	mutable bool m_dirty = true;
	mutable int m_leftMargin, m_topMargin, m_bottomMargin, m_rightMargin = 0;
#endif
};

class SanerFormLayout final : private QGridLayout {
	Q_OBJECT
public:
	using FieldGrowthPolicy = QFormLayout::FieldGrowthPolicy;

	SanerFormLayout(QWidget *parent);

	void addAside(QLayout *layout, int row, int rowSpan = -1);
	void addAside(QWidget *widget, int row, int rowSpan = -1);
	QButtonGroup *addRadioGroup(const QString &label, bool horizontal, std::initializer_list<std::pair<const QString &, int>> items, int colSpan = 1);
	void addRow(const QString &label, QLayout *layout, int rowSpan = 1, int colSpan = 1, Qt::Alignment alignment = {});
	void addRow(const QString &label, QWidget *widget, int rowSpan = 1, int colSpan = 1, Qt::Alignment alignment = {});
	void addRow(QWidget *label, QLayout *layout, int rowSpan = 1, int colSpan = 1, Qt::Alignment alignment = {});
	void addRow(QWidget *label, QWidget *widget, int rowSpan = 1, int colSpan = 1, Qt::Alignment alignment = {});
	inline void addRow(const QString &label, QLayout *layout, Qt::Alignment alignment) {
		addRow(label, layout, 1, 1, alignment);
	}
	inline void addRow(const QString &label, QWidget *widget, Qt::Alignment alignment) {
		addRow(label, widget, 1, 1, alignment);
	}
	inline void addRow(QWidget *label, QLayout *layout, Qt::Alignment alignment) {
		addRow(label, layout, 1, 1, alignment);
	}
	inline void addRow(QWidget *label, QWidget *widget, Qt::Alignment alignment) {
		addRow(label, widget, 1, 1, alignment);
	}
	QFrame *addSeparator();
	void addSpacer(QSizePolicy::Policy vPolicy = QSizePolicy::Fixed, int colSpan = 2);
	void addSpanningRow(QLayout *layout, Qt::Alignment = {});
	void addSpanningRow(QWidget *widget, Qt::Alignment = {});
	Qt::Orientations expandingDirections() const override;
	void invalidate() override;
	QSize maximumSize() const override;
	QSize minimumSize() const override;
	int rowCount() const { return (gridRowCount() + 1) / 2; }
	void setGeometry(const QRect &rect) override;
	QSize sizeHint() const override;

	FieldGrowthPolicy fieldGrowthPolicy() const;
	void setFieldGrowthPolicy(FieldGrowthPolicy policy);

	Qt::Alignment formAlignment() const;
	void setFormAlignment(Qt::Alignment alignment);

	Qt::Alignment labelAlignment() const;
	void setLabelAlignment(Qt::Alignment alignment);

	bool stretchLabel() const;
	void setStretchLabel(bool stretch);

	// Using private inheritance to avoid accidental use of the grid methods
	// that would break the layout, but all these methods are fine
	using QGridLayout::columnStretch;
	using QGridLayout::contentsMargins;
	using QGridLayout::expandingDirections;
	using QGridLayout::hasHeightForWidth;
	using QGridLayout::heightForWidth;
	using QGridLayout::itemAt;
	using QGridLayout::itemAtPosition;
	using QGridLayout::maximumSize;
	using QGridLayout::minimumHeightForWidth;
	using QGridLayout::minimumSize;
	using QGridLayout::setColumnStretch;
	using QGridLayout::setContentsMargins;
	using QGridLayout::sizeHint;
	using QGridLayout::takeAt;

private:
	int gridRowCount() const { return QGridLayout::rowCount(); }

	int addRowSpacing();
	QStyle *style() const;
	void updateLayout(bool updateSpacers, bool updateFields);

	bool m_dirtyLayout = true;
	mutable bool m_dirtySpacers = true;
	// QFormLayout uses a magic number hack so we will too
	int m_fieldGrowthPolicy = -1;
	Qt::Alignment m_formAlignment = {};
	Qt::Alignment m_labelAlignment = {};
	bool m_stretchLabel = true;
};

} // namespace utils

#endif
