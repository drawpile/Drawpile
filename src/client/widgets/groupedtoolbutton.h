/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen, GroupedToolButton based on Gwenview's StatusBarToolbutton by Aurélien Gâteau

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

#ifndef DESIGNER_PLUGIN
namespace widgets {
#define PLUGIN_EXPORT
#else
#include <QtDesigner/QDesignerExportWidget>
#define PLUGIN_EXPORT QDESIGNER_WIDGET_EXPORT
#endif

/**
 * A thin tool button which can be grouped with another and look like one solid
 * bar:
 *
 * ( button1 | button2 )
 */
class PLUGIN_EXPORT GroupedToolButton : public QToolButton
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

	explicit GroupedToolButton(GroupPosition position, QWidget* parent=0);
	explicit GroupedToolButton(QWidget* parent=0);

	GroupPosition groupPosition() const { return mGroupPosition; }
	void setGroupPosition(GroupPosition groupPosition);

protected:
	virtual void paintEvent(QPaintEvent* event) Q_DECL_OVERRIDE;

private:
	GroupPosition mGroupPosition;
};

#ifndef DESIGNER_PLUGIN
}
#endif

#endif
