// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TOOLSETTINGS_ANNOTATION_H
#define TOOLSETTINGS_ANNOTATION_H
#include "desktop/toolwidgets/toolsettings.h"

class QAction;
class QActionGroup;
class QLabel;
class QStackedWidget;
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
	bool isLocked() override;

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

	void setCanvasView(view::CanvasWrapper *canvasView)
	{
		m_canvasView = canvasView;
	}

	QWidget *getHeaderWidget() override;

	void setAnnotationsShown(bool annotationsShown);

public slots:
	//! Set the currently selected annotation item
	void setSelectionId(uint16_t id);

	//! Focus the content editing box
	void setFocus();

signals:
	void selectionIdChanged(int id);
	void showAnnotationsRequested();

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
	void updateWidgets();

	QWidget *m_headerWidget = nullptr;
	QStackedWidget *m_stack = nullptr;
	Ui_TextSettings *m_ui = nullptr;
	QLabel *m_annotationsHiddenLabel = nullptr;
	QActionGroup *m_editActions = nullptr;
	QAction *m_protectedAction = nullptr;
	uint16_t m_selectionId = 0;
	bool m_annotationsShown = true;
	bool m_noupdate = false;
	QTimer *m_updatetimer = nullptr;
	view::CanvasWrapper *m_canvasView = nullptr;
};

}

#endif
