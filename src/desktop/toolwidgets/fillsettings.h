// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLSETTINGS_FILL_H
#define TOOLSETTINGS_FILL_H

#include "desktop/toolwidgets/toolsettings.h"

class Ui_FillSettings;

namespace canvas {
struct LayerListItem;
class LayerListModel;
}

namespace tools {

/**
 * @brief Settings for the flood fill tool
 */
class FillSettings final : public ToolSettings {
	Q_OBJECT
public:
	FillSettings(ToolController *ctrl, QObject *parent=nullptr);
	~FillSettings() override;

	QString toolType() const override { return QStringLiteral("fill"); }

	void quickAdjust1(qreal adjustment) override;
	void stepAdjust1(bool increase) override;
	void setForeground(const QColor &color) override;

	int getSize() const override;
	bool getSubpixelMode() const override { return false; }
	bool isSquare() const override { return true; }

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	void setLayerList(canvas::LayerListModel *layerlist);

	static int modeIndexToBlendMode(int mode);

signals:
	void pixelSizeChanged(int size);

public slots:
	void pushSettings() override;
	void toggleEraserMode() override;
	void setActiveLayer(int layerId);
	void setSourceLayerId(int layerId);
	void setLayers(const QVector<canvas::LayerListItem> &items);

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	class FillSourceModel;
	enum Mode {
		Normal,
		Behind,
		Erase,
	};

	Ui_FillSettings *m_ui;
	FillSourceModel *m_fillSourceModel;
	Mode m_previousMode = Normal;
	qreal m_quickAdjust1 = 0.0;
};

}

#endif

