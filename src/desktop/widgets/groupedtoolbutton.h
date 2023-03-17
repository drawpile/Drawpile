/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2017 Calle Laakkonen, GroupedToolButton based on Gwenview's StatusBarToolbutton by Aurélien Gâteau

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef STATUSBARTOOLBUTTON_H
#define STATUSBARTOOLBUTTON_H

#include <QToolButton>

#ifdef DESIGNER_PLUGIN
#include <QtUiPlugin/QDesignerExportWidget>
#else
#define QDESIGNER_WIDGET_EXPORT
#endif

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

	GroupPosition groupPosition() const { return mGroupPosition; }
	void setGroupPosition(GroupPosition groupPosition);

	void setColorSwatch(const QColor &color);
	QColor colorSwatch() const { return m_colorSwatch; }

protected:
	virtual void paintEvent(QPaintEvent* event) Q_DECL_OVERRIDE;

private:
	GroupPosition mGroupPosition;
	QColor m_colorSwatch;
};

}

#endif
