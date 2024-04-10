// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef WIDGETUTILS_H
#define WIDGETUTILS_H
#include <QHBoxLayout>
#include <QIcon>
#include <QObject>
#include <QScroller>
#include <QSizePolicy>
#include <optional>

class QAbstractScrollArea;
class QBoxLayout;
class QButtonGroup;
class QCheckBox;
class QCursor;
class QFormLayout;
class QFrame;
class QHeaderView;
class QLabel;
class QMessageBox;
class QWidget;

namespace utils {

class ScopedOverrideCursor {
public:
	ScopedOverrideCursor(); // Uses Qt::WaitCursor.
	ScopedOverrideCursor(const QCursor &cursor);
	~ScopedOverrideCursor();
};

class ScopedUpdateDisabler {
public:
	ScopedUpdateDisabler(QWidget *widget);
	~ScopedUpdateDisabler();

private:
	QWidget *m_widget;
	bool m_wasEnabled;
};


/// @brief Like `QHBoxLayout`, but allows the control type to be overridden to
/// provide more sensible spacing, and on macOS, fixes broken layouts caused by
/// fake extra layout margins.
class EncapsulatedLayout final : public QHBoxLayout {
	Q_OBJECT
public:
	EncapsulatedLayout();

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


// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-3.0-or-later
// SDPX—SnippetName: Based on the kinetic scroll event filter from Krita
class KineticScroller : public QObject {
	Q_OBJECT
public:
	KineticScroller(
		QAbstractScrollArea *parent,
		Qt::ScrollBarPolicy horizontalScrollBarPolicy,
		Qt::ScrollBarPolicy verticalScrollBarPolicy, int kineticScrollGesture,
		int kineticScrollThreshold, int kineticScrollHideBars);

	void
	setVerticalScrollBarPolicy(Qt::ScrollBarPolicy verticalScrollBarPolicy);

	void setKineticScrollGesture(int kineticScrollGesture);
	void setKineticScrollThreshold(int kineticScrollThreshold);
	void setKineticScrollHideBars(bool kineticScrollHideBars);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void reset(
		int kineticScrollGesture, int kineticScrollSensitivity,
		bool kineticScrollHideBars);

	void enableKineticScrolling(
		QScroller::ScrollerGestureType gestureType,
		int kineticScrollSensitivity, bool kineticScrollHideBars);

	void disableKineticScrolling();

	void setScrollPerPixel(bool scrollPerPixel);
	void setEventFilter(QObject *target, bool install);

	static QScrollerProperties
	getScrollerPropertiesForSensitivity(int kineticScrollSensitivity);

	static int toSensitivity(int kineticScrollThreshold);
	static bool isKineticScrollingEnabled(
		int kineticScrollGesture,
		QScroller::ScrollerGestureType *outGestureType = nullptr);

	QAbstractScrollArea *const m_scrollArea;
	Qt::ScrollBarPolicy m_horizontalScrollBarPolicy;
	Qt::ScrollBarPolicy m_verticalScrollBarPolicy;
	int m_kineticScrollGesture;
	int m_kineticScrollSensitivity;
	bool m_kineticScrollHideBars;
};
// SPDX-SnippetEnd

void showWindow(
	QWidget *widget, bool maximized = false, bool isMainWindow = false);

void setWidgetRetainSizeWhenHidden(QWidget *widget, bool retainSize);

bool setGeometryIfOnScreen(QWidget *widget, const QRect &geometry);

// Sets header to sort by no column (as opposed to the first one) and enables
// clearing the sort indicator if the Qt version is >= 6.1.0.
void initSortingHeader(
	QHeaderView *header, int sortColumn = -1,
	Qt::SortOrder order = Qt::AscendingOrder);

KineticScroller *bindKineticScrolling(QAbstractScrollArea *scrollArea);
KineticScroller *bindKineticScrollingWith(
	QAbstractScrollArea *scrollArea,
	Qt::ScrollBarPolicy horizontalScrollBarPolicy,
	Qt::ScrollBarPolicy verticalScrollBarPolicy);

QFormLayout *addFormSection(QBoxLayout *layout);

void addFormSpacer(
	QLayout *layout, QSizePolicy::Policy vPolicy = QSizePolicy::Fixed);

QFrame *makeSeparator();

void addFormSeparator(QBoxLayout *layout);

EncapsulatedLayout *encapsulate(const QString &label, QWidget *child);

EncapsulatedLayout *indent(QWidget *child);

QWidget *formNote(
	const QString &text, QSizePolicy::ControlType type = QSizePolicy::Label,
	const QIcon &icon = QIcon());

void setSpacingControlType(
	EncapsulatedLayout *widget, QSizePolicy::ControlTypes type);

void setSpacingControlType(QWidget *widget, QSizePolicy::ControlType type);

QButtonGroup *addRadioGroup(
	QFormLayout *form, const QString &label, bool horizontal,
	std::initializer_list<std::pair<const QString &, int>> items);

QCheckBox *addCheckable(
	const QString &accessibleName, EncapsulatedLayout *layout, QWidget *child);

QLabel *makeIconLabel(const QIcon &icon, QWidget *parent = nullptr);

QMessageBox *makeQuestion(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText = QString());

QMessageBox *makeInformation(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText = QString());

QMessageBox *makeInformationWithSaveButton(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText = QString());

QMessageBox *makeWarning(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText = QString());

QMessageBox *makeCritical(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText = QString());

QMessageBox *showQuestion(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText = QString());

QMessageBox *showInformation(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText = QString());

QMessageBox *showWarning(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText = QString());

QMessageBox *showCritical(
	QWidget *parent, const QString &title, const QString &text,
	const QString &informativeText = QString());

}

#endif
