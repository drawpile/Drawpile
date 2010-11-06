/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2009 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#ifndef LAYERWIDGET_H
#define LAYERWIDGET_H

#include <QFrame>

class QSlider;
class QCheckBox;

namespace dpcore {
	class Layer;
}

namespace widgets {

/**
 * A widget for adjusting the visual attributes of a layer.
 * This pops up when the user clicks on the layer details icon.
 */
class LayerStyleEditor : public QFrame
{
	Q_OBJECT
	public:
		LayerStyleEditor(const dpcore::Layer *layer, QWidget *parent=0);
		~LayerStyleEditor();

	signals:
		void opacityChanged(int id, int newopacity);
		void toggleHidden(int id);

	protected:
		void changeEvent(QEvent*);

	private slots:
		void updateOpacity(int o);
		void toggleHide();

	private:
		QSlider *opacity_;
		QCheckBox *hide_;
		int id_;
};

}

#endif

