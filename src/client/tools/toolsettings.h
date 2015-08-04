/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2015 Calle Laakkonen

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
#ifndef TOOLSETTINGS_H
#define TOOLSETTINGS_H

#include <QPointer>

#include "core/brush.h"
#include "utils/palette.h"
#include "utils/icon.h"

class Ui_PenSettings;
class Ui_BrushSettings;
class Ui_SmudgeSettings;
class Ui_EraserSettings;
class Ui_SimpleSettings;
class Ui_TextSettings;
class Ui_SelectionSettings;
class Ui_LaserSettings;
class Ui_FillSettings;
class QTimer;
class QCheckBox;

namespace net {
	class Client;
}
namespace docks {
	class LayerList;
}
namespace widgets {
	class PaletteWidget;
	class CanvasView;
}
namespace  canvas {
	class CanvasModel;
}

namespace tools {

class ToolProperties;
class ToolController;

/**
 * @brief Abstract base class for tool settings
 *
 * The tool settings class provides a user interface widget that is
 * displayed in a dock window and a uniform way of getting a brush
 * configured by the user.
 */
class ToolSettings {
public:
	ToolSettings(const QString &name, const QString &title, const QString &icon)
		: _name(name), _title(title), _icon(icon), _widget(0) {}
	virtual ~ToolSettings() = default;

	/**
	 * @brief Create the UI widget
	 *
	 * If the tool has a size changing signal, it will be connected to the
	 * parent's sizeChanged(int) signal.
	 *
	 * @param parent parent widget
	 * @return UI widget
	 */
	QWidget *createUi(QWidget *parent);

	//! Get the UI widget
	QWidget *getUi() { return _widget; }

	//! Set the foreground color
	virtual void setForeground(const QColor& color) = 0;

	//! Set the background color
	virtual void setBackground(const QColor& color) = 0;

	/**
	 * @brief Quick adjust a tool parameter
	 *
	 * This is a shortcut for adjusting a tool parameter.
	 * For brush based tools, this adjust the size.
	 * @param adjustment how much to adjust by (-1/1 is the normal rate)
	 */
	virtual void quickAdjust1(float adjustment) = 0;

	/**
	 * @brief Get a brush based on the settings in the UI
	 * An UI widget must have been created before this can be called.
	 *
	 * @param swapcolors if true, foreground and background colors are swapped
	 * @return brush with values from the UI widget
	 */
	virtual paintcore::Brush getBrush() const = 0;

	/**
	 * @brief Get the current brush size
	 * @return size of the current brush
	 */
	virtual int getSize() const = 0;

	/**
	 * @brief Is this tool in subpixel precision mode
	 */
	virtual bool getSubpixelMode() const = 0;

	/**
	 * @brief Get the internal name of this tool
	 * The internal name is used when settings are stored to a
	 * configuration file
	 * @return internal tool name
	 */
	const QString& getName() const { return _name; }

	/**
	 * @brief Get the user facing name of this tool
	 * @return visible tool name
	 */
	const QString& getTitle() const { return _title; }

	//! Get the icon for this tool type
	const QIcon getIcon(icon::Theme variant=icon::CURRENT) const { return icon::fromTheme(_icon, variant); }

	/**
	 * @brief Save the settings of this tool
	 * @return saved tool settings
	 */
	virtual ToolProperties saveToolSettings();

	/**
	 * @brief Load settings for this tool
	 * @param props
	 */
	virtual void restoreToolSettings(const ToolProperties &);

protected:
	virtual QWidget *createUiWidget(QWidget *parent) = 0;	

private:
	QString _name;
	QString _title;
	QString _icon;
	QWidget *_widget;
};

/**
 * @brief Pen settings
 *
 * This is much like BrushSettings, except the pen always has 100% hardness
 * and no antialiasing.
 */
class PenSettings : public ToolSettings {
public:
	PenSettings(QString name, QString title);
	~PenSettings();

	void setForeground(const QColor& color);
	void setBackground(const QColor& color);
	void quickAdjust1(float adjustment);
	paintcore::Brush getBrush() const;

	int getSize() const;
	bool getSubpixelMode() const { return false; }

	virtual ToolProperties saveToolSettings() override;
	virtual void restoreToolSettings(const ToolProperties &cfg) override;

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Ui_PenSettings *_ui;
};

/**
 * @brief Eraser settings
 *
 * This is a settings class for brushes that erase.
 * Erasers don't actually use the colors assigned to them, but will
 * always simple erase the alpha channel.
 */
class EraserSettings : public ToolSettings {
public:
	EraserSettings(QString name, QString title);
	~EraserSettings();

	void setForeground(const QColor& color);
	void setBackground(const QColor& color);
	void quickAdjust1(float adjustment);
	paintcore::Brush getBrush() const;

	int getSize() const;
	bool getSubpixelMode() const;

	virtual ToolProperties saveToolSettings() override;
	virtual void restoreToolSettings(const ToolProperties &cfg) override;

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Ui_EraserSettings *_ui;
};

/**
 * @brief Brush settings
 *
 * This is a settings class for the brush tool.
 */
class BrushSettings : public ToolSettings {
public:
	BrushSettings(QString name, QString title);
	~BrushSettings();

	void setForeground(const QColor& color);
	void setBackground(const QColor& color);
	void quickAdjust1(float adjustment);
	paintcore::Brush getBrush() const;

	int getSize() const;
	bool getSubpixelMode() const { return true; }

	virtual ToolProperties saveToolSettings() override;
	virtual void restoreToolSettings(const ToolProperties &cfg) override;

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Ui_BrushSettings *_ui;
};

/**
 * @brief Smudge brush settings
 *
 * This is a settings class for the color smudging brush tool.
 */
class SmudgeSettings : public ToolSettings {
public:
	SmudgeSettings(QString name, QString title);
	~SmudgeSettings();

	void setForeground(const QColor& color);
	void setBackground(const QColor& color);
	void quickAdjust1(float adjustment);
	paintcore::Brush getBrush() const;

	int getSize() const;
	bool getSubpixelMode() const { return true; }

	virtual ToolProperties saveToolSettings() override;
	virtual void restoreToolSettings(const ToolProperties &cfg) override;

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Ui_SmudgeSettings *_ui;
};

/**
 * @brief Settings for tools without pressure sensitivity
 */
class SimpleSettings : public ToolSettings {
public:
	enum Type {Line, Rectangle, Ellipse};

	SimpleSettings(const QString &name, const QString &title, const QString &icon, Type type, bool sp);
	~SimpleSettings();

	void setForeground(const QColor& color);
	void setBackground(const QColor& color);
	void quickAdjust1(float adjustment);
	paintcore::Brush getBrush() const;

	int getSize() const;
	bool getSubpixelMode() const;

	virtual ToolProperties saveToolSettings() override;
	virtual void restoreToolSettings(const ToolProperties &cfg) override;

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Ui_SimpleSettings *_ui;
	Type _type;
	bool _subpixel;
};

/**
 * @brief Settings base class for non-brush based tools
 */
class BrushlessSettings : public ToolSettings {
public:
	BrushlessSettings(const QString &name, const QString &title, const QString &icon) : ToolSettings(name, title, icon) {}

	paintcore::Brush getBrush() const;
	void setForeground(const QColor& color);
	void setBackground(const QColor& color);
	void quickAdjust1(float adjustment) { Q_UNUSED(adjustment); }

	int getSize() const { return 0; }
	bool getSubpixelMode() const { return false; }

protected:
	paintcore::Brush _dummybrush;
};

/**
 * @brief Settings for the annotation tool
 *
 * The annotation tool is special because it is used to manipulate
 * annotation objects rather than pixel data.
 */
class AnnotationSettings : public QObject, public BrushlessSettings {
Q_OBJECT
public:
	AnnotationSettings(QString name, QString title);
	~AnnotationSettings();

	void setController(ToolController *ctrl) { m_ctrl = ctrl; }

	/**
	 * @brief Get the ID of the currently selected annotation
	 * @return ID or 0 if none selected
	 */
	int selected() const { return m_selectionId; }

	/**
	 * @brief Focus content editing box and set cursor position
	 * @param cursorPos cursor position
	 */
	void setFocusAt(int cursorPos);

public slots:
	//! Set the currently selected annotation item
	void setSelectionId(int id);

	//! Focus the content editing box
	void setFocus();

private slots:
	void changeAlignment();
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
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	void resetContentFont(bool resetFamily, bool resetSize, bool resetColor);
	void setUiEnabled(bool enabled);

	Ui_TextSettings *_ui;
	QWidget *_uiwidget;

	int m_selectionId;

	bool _noupdate;
	ToolController *m_ctrl;
	QTimer *_updatetimer;
};

/**
 * @brief Color picker history
 */
class ColorPickerSettings : public QObject, public BrushlessSettings {
Q_OBJECT
public:
	ColorPickerSettings(const QString &name, const QString &title);
	~ColorPickerSettings();

	//! Pick color from current layer only?
	bool pickFromLayer() const;

	virtual ToolProperties saveToolSettings() override;
	virtual void restoreToolSettings(const ToolProperties &cfg) override;

public slots:
	void addColor(const QColor &color);

signals:
	void colorSelected(const QColor &color);

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Palette _palette;
	widgets::PaletteWidget *_palettewidget;
	QCheckBox *_layerpick;
};

class SelectionSettings : public QObject, public BrushlessSettings {
	Q_OBJECT
public:
	SelectionSettings(const QString &name, const QString &title, bool freeform);
	~SelectionSettings();

	void setController(tools::ToolController *ctrl) { m_ctrl = ctrl; }
	void setView(widgets::CanvasView *view) { _view = view; }

private slots:
	void flipSelection();
	void mirrorSelection();
	void fitToScreen();
	void resetSize();

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Ui_SelectionSettings * _ui;
	tools::ToolController *m_ctrl;
	widgets::CanvasView *_view;
};

class LaserPointerSettings : public QObject, public BrushlessSettings {
	Q_OBJECT
public:
	LaserPointerSettings(const QString &name, const QString &title);
	~LaserPointerSettings();

	bool pointerTracking() const;
	int trailPersistence() const;

	void setForeground(const QColor& color);
	void quickAdjust1(float adjustment);
	paintcore::Brush getBrush() const;

	virtual ToolProperties saveToolSettings() override;
	virtual void restoreToolSettings(const ToolProperties &cfg) override;

signals:
	void pointerTrackingToggled(bool);

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Ui_LaserSettings * _ui;
};

class FillSettings : public BrushlessSettings {
public:
	FillSettings(const QString &name, const QString &title);
	~FillSettings();

	int fillTolerance() const;
	int fillExpansion() const;
	bool sampleMerged() const;
	bool underFill() const;

	void quickAdjust1(float adjustment) override;
	void setForeground(const QColor &color) override;
	void setBackground(const QColor &color) override;

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Ui_FillSettings * _ui;
};

}

#endif

