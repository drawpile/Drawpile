// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLWIDGETS_FILL_H
#define DESKTOP_TOOLWIDGETS_FILL_H
#include "desktop/toolwidgets/toolsettings.h"

class QButtonGroup;
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
	FillSettings(ToolController *ctrl, QObject *parent = nullptr);
	~FillSettings() override;

	QString toolType() const override { return QStringLiteral("fill"); }

	bool affectsCanvas() override { return true; }
	bool affectsLayer() override { return true; }

	void quickAdjust1(qreal adjustment) override;
	void stepAdjust1(bool increase) override;
	void setForeground(const QColor &color) override;

	int getSize() const override;
	bool isSquare() const override { return true; }
	bool requiresOutline() const override { return true; }

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	void setLayerList(canvas::LayerListModel *layerlist);

	static int modeIndexToBlendMode(int mode);

signals:
	void pixelSizeChanged(int size);
	void fillSourceSet(int layerId);

public slots:
	void pushSettings() override;
	void toggleEraserMode() override;
	void setActiveLayer(int layerId);
	void setSourceLayerId(int layerId);
	void setLayers(const QVector<canvas::LayerListItem> &items);

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	class FillLayerModel;
	enum class Source { Merged, MergedWithoutBackground, Layer };
	enum Mode { Normal, Behind, Erase };
	enum Area { Continuous, Similar };

	void updateLayerCombo(int source);

	Ui_FillSettings *m_ui = nullptr;
	QButtonGroup *m_sourceGroup = nullptr;
	QButtonGroup *m_areaGroup = nullptr;
	FillLayerModel *m_fillLayerModel;
	Mode m_previousMode = Normal;
	qreal m_quickAdjust1 = 0.0;
};

}

#endif
