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

namespace dpcore {
	class Layer;
}

namespace widgets {

class LayerEditor : public QFrame
{
	Q_OBJECT
	public:
		LayerEditor(const dpcore::Layer *layer, QWidget *parent=0);
		~LayerEditor();

	signals:
		void opacityChanged(int id, int newopacity);

	protected:
		void changeEvent(QEvent*);

	private slots:
		void updateOpacity(int o);

	private:
		QSlider *opacity_;
		int id_;
};

}

#endif

