// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_GROUPEDTOOLBUTTON_H
#define DESKTOP_WIDGETS_GROUPEDTOOLBUTTON_H
#include <QColor>
#include <QToolButton>

class QEvent;
class QPaintEvent;
class QWidget;

namespace widgets {

/**
 * A thin tool button which can be grouped with another and look like one solid
 * bar:
 *
 * ( button1 | button2 )
 *
 * New Drawpile specific features:
 *  - Dropdown arrow if a menu (and text) is attached
 *  - Color swatch mode
 */
class GroupedToolButton final : public QToolButton {
	Q_OBJECT
public:
	enum GroupPosition {
		NotGrouped = 0,
		GroupLeft = 1,
		GroupRight = 2,
		GroupCenter = 3
	};

	explicit GroupedToolButton(
		GroupPosition position, QWidget *parent = nullptr);
	explicit GroupedToolButton(QWidget *parent = nullptr);

	GroupPosition groupPosition() const { return m_groupPosition; }
	void setGroupPosition(GroupPosition groupPosition);

	void setColorSwatch(const QColor &colorSwatch);
	QColor colorSwatch() const { return m_colorSwatch; }

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	GroupPosition m_groupPosition;
	QColor m_colorSwatch;
};

}

#endif
