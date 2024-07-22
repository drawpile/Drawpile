// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TOOLSETTINGS_ANNOTATION_H
#define TOOLSETTINGS_ANNOTATION_H
#include "desktop/toolwidgets/toolsettings.h"

class QAction;
class QActionGroup;
class QTextCharFormat;
class QTimer;
class Ui_TextSettings;

namespace view {
class CanvasWrapper;
}

namespace tools {

/**
 * @brief Settings for the annotation tool
 *
 * The annotation tool is special because it is used to manipulate
 * annotation objects rather than pixel data.
 */
class AnnotationSettings final : public ToolSettings {
	Q_OBJECT
public:
	AnnotationSettings(ToolController *ctrl, QObject *parent = nullptr);
	~AnnotationSettings() override;

	QString toolType() const override { return QStringLiteral("annotation"); }

	bool affectsCanvas() override { return true; }
	bool affectsLayer() override { return false; }

	/**
	 * @brief Get the ID of the currently selected annotation
	 * @return ID or 0 if none selected
	 */
	uint16_t selected() const { return m_selectionId; }

	/**
	 * @brief Focus content editing box and set cursor position
	 * @param cursorPos cursor position
	 */
	void setFocusAt(int cursorPos);

	void setCanvasView(view::CanvasWrapper *canvasView) { m_canvasView = canvasView; }
	QWidget *getHeaderWidget() override;

public slots:
	//! Set the currently selected annotation item
	void setSelectionId(uint16_t id);

	//! Focus the content editing box
	void setFocus();

signals:
	void selectionIdChanged(int id);

private slots:
	void changeAlignment(const QAction *action);
	void toggleBold(bool bold);
	void toggleStrikethrough(bool strike);
	void updateStyleButtons();
	void setEditorBackgroundColor(const QColor &color);

	void applyChanges();
	void saveChanges();
	void removeAnnotation();
	void bake();

	void updateFontIfUniform();

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	void resetContentFormat();
	void resetContentFont(bool resetFamily, bool resetSize, bool resetColor);
	void setFontFamily(QTextCharFormat &fmt);
	void setUiEnabled(bool enabled);

	Ui_TextSettings *m_ui;
	QWidget *m_headerWidget;
	QActionGroup *m_editActions;
	QAction *m_protectedAction;

	uint16_t m_selectionId;

	bool m_noupdate;
	QTimer *m_updatetimer;
	view::CanvasWrapper *m_canvasView;
};

}

#endif
