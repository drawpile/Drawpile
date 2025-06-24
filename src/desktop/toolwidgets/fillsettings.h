// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLWIDGETS_FILL_H
#define DESKTOP_TOOLWIDGETS_FILL_H
#include "desktop/toolwidgets/toolsettings.h"

class BlendModeManager;
class QAction;
class QButtonGroup;
class QLabel;
class QMenu;
class QStackedWidget;
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
	bool isLocked() override;

	void quickAdjust1(qreal adjustment) override;
	void quickAdjust2(qreal adjustment) override;
	void quickAdjust3(qreal adjustment) override;
	void stepAdjust1(bool increase) override;
	void stepAdjust2(bool increase) override;
	void stepAdjust3(bool increase) override;
	void setForeground(const QColor &color) override;

	int getSize() const override;
	bool isSquare() const override { return true; }
	bool requiresOutline() const override { return true; }

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	QWidget *getHeaderWidget() override { return m_headerWidget; }

	void setActions(QAction *automaticAlphaPreserve);
	void setFeatureAccess(bool featureAccess);
	void setCompatibilityMode(bool compatibilityMode) override;

	void pushSettings() override;
	void toggleEraserMode() override;
	void toggleAlphaPreserve() override;
	void toggleBlendMode(int blendMode) override;
	void updateSelection();
	void updateFillSourceLayerId(int layerId);

signals:
	void pixelSizeChanged(int size);

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	void updateTolerance();
	void updateSettings();

	void updateSize();
	static bool isSizeUnlimited(int size);
	int calculatePixelSize(int size) const;

	void setButtonState(bool running, bool pending);
	void setDragState(bool dragging, int tolerance);
	void updateWidgets();

	QWidget *m_headerWidget = nullptr;
	QStackedWidget *m_stack;
	Ui_FillSettings *m_ui = nullptr;
	QMenu *m_menu = nullptr;
	QLabel *m_permissionDeniedLabel = nullptr;
	QAction *m_editableAction = nullptr;
	QAction *m_confirmAction = nullptr;
	QButtonGroup *m_sourceGroup = nullptr;
	QButtonGroup *m_areaGroup = nullptr;
	BlendModeManager *m_blendModeManager = nullptr;
	int m_toleranceBeforeDrag = -1;
	qreal m_quickAdjust1 = 0.0;
	qreal m_quickAdjust2 = 0.0;
	qreal m_quickAdjust3 = 0.0;
	bool m_featureAccess = true;
	bool m_haveSelection = false;
	bool m_updating = false;
};

}

#endif
