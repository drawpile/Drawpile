// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLWIDGETS_SELECTION_H
#define DESKTOP_TOOLWIDGETS_SELECTION_H
#include "desktop/toolwidgets/toolsettings.h"

class QAction;
class QButtonGroup;
class QCheckBox;
class QPushButton;

namespace canvas {
class CanvasModel;
}

namespace tools {

class SelectionSettings final : public ToolSettings {
	Q_OBJECT
public:
	SelectionSettings(ToolController *ctrl, QObject *parent = nullptr);

	QString toolType() const override { return QStringLiteral("selection"); }

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	bool isLocked() override;

	void setAction(QAction *starttransform);

	void setPutImageAllowed(bool putImageAllowed)
	{
		m_putImageAllowed = putImageAllowed;
	}

	QWidget *getHeaderWidget() override { return m_headerWidget; }

public slots:
	void pushSettings() override;

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	void setModel(canvas::CanvasModel *canvas);
	void updateEnabled();
	void updateEnabledFrom(canvas::CanvasModel *canvas);

	QWidget *m_headerWidget = nullptr;
	QButtonGroup *m_headerGroup = nullptr;
	QCheckBox *m_antiAliasCheckBox = nullptr;
	QPushButton *m_startTransformButton = nullptr;
	bool m_putImageAllowed = true;
};

}

#endif
