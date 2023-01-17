// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef STATUSBARTOOLBUTTON_H
#define STATUSBARTOOLBUTTON_H

#include <QColor>
#include <QToolButton>

#ifdef DESIGNER_PLUGIN
#include <QtUiPlugin/QDesignerExportWidget>
#else
#define QDESIGNER_WIDGET_EXPORT
#endif

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
class QDESIGNER_WIDGET_EXPORT GroupedToolButton final : public QToolButton
{
	Q_OBJECT
	Q_PROPERTY(GroupPosition groupPosition READ groupPosition WRITE setGroupPosition)
	Q_ENUMS(GroupPosition)
public:
	enum GroupPosition {
		NotGrouped = 0,
		GroupLeft = 1,
		GroupRight = 2,
		GroupCenter = 3
	};

	explicit GroupedToolButton(GroupPosition position, QWidget* parent = nullptr);
	explicit GroupedToolButton(QWidget* parent = nullptr);

	GroupPosition groupPosition() const { return m_groupPosition; }
	void setGroupPosition(GroupPosition groupPosition);

	void setColorSwatch(const QColor &color);
	QColor colorSwatch() const { return m_colorSwatch; }

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	GroupPosition m_groupPosition;
	QColor m_colorSwatch;
};

}

#endif
