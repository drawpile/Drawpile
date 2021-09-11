/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2021 Calle Laakkonen

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

#include <qobjectdefs.h>
#include <qdialog.h>

class Ui_LayerProperties;

namespace canvas {
    class CanvasModel;
}

namespace rustpile {
	enum class Blendmode : uint8_t;
}

namespace dialogs {

class LayerProperties : public QDialog
{
Q_OBJECT
public:
    struct LayerData {
        int id;
        QString title;
        float opacity;
		rustpile::Blendmode blend;
        bool hidden;
        bool fixed;
        bool defaultLayer;
    };

    enum ChangeFlag {
        CHANGE_NOTHING = 0,
        CHANGE_TITLE = 1 << 0,
        CHANGE_OPACITY = 1 << 1,
        CHANGE_BLEND = 1 << 2,
        CHANGE_HIDDEN = 1 << 3,
        CHANGE_FIXED = 1 << 4,
        CHANGE_DEFAULT = 1 << 5,
    };

    struct ChangedLayerData : public LayerData {
        unsigned int changes;
    };

	explicit LayerProperties(QWidget *parent = nullptr);

    void setCanvas(canvas::CanvasModel *canvas);
    void setLayerData(const LayerData &data);

signals:
    void propertiesChanged(const ChangedLayerData &c);

protected:
    virtual void showEvent(QShowEvent *event) override;

private slots:
    void emitChanges();

private:
    void applyLayerDataToUi();
    int layerDataOpacity();
	int searchBlendModeIndex(rustpile::Blendmode mode);

    Ui_LayerProperties *m_ui;
    canvas::CanvasModel *m_canvas;
    LayerData m_layerData;
};

}

#endif
