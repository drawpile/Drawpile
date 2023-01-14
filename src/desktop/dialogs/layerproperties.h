/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2021 Calle Laakkonen

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

#ifndef LAYERPROPERTIES_H
#define LAYERPROPERTIES_H

#include "canvas/layerlist.h"

#include <QDialog>

class Ui_LayerProperties;

namespace drawdance {
    class Message;
}

namespace dialogs {

class LayerProperties : public QDialog
{
Q_OBJECT
public:
	explicit LayerProperties(uint8_t localUser, QWidget *parent = nullptr);
	~LayerProperties();

	void setLayerItem(const canvas::LayerListItem &data, const QString &creator, bool isDefault);
	void setControlsEnabled(bool enabled);
	void setOpControlsEnabled(bool enabled);

	int layerId() const { return m_item.id; }

signals:
	void layerCommands(int count, const drawdance::Message *msgs);
	void visibilityChanged(int layerId, bool visible);

protected:
    virtual void showEvent(QShowEvent *event) override;

private slots:
	void emitChanges();

private:
	int searchBlendModeIndex(DP_BlendMode mode);

    Ui_LayerProperties *m_ui;
	canvas::LayerListItem m_item;
	bool m_wasDefault;
	uint8_t m_user;
};

}

#endif
