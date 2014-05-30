/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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
#include "scene/annotationitem.h"
#include "utils/palette.h"

class Ui_PenSettings;
class Ui_BrushSettings;
class Ui_EraserSettings;
class Ui_SimpleSettings;
class Ui_TextSettings;
class Ui_SelectionSettings;
class Ui_LaserSettings;
class QSettings;
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
}

namespace tools {

/**
 * @brief Abstract base class for tool settings
 *
 * The tool settings class provides a user interface widget that is
 * displayed in a dock window and a uniform way of getting a brush
 * configured by the user.
 */
class ToolSettings {
public:
	ToolSettings(QString name,QString title)
		: _name(name), _title(title), _widget(0) {}
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
	virtual const paintcore::Brush& getBrush(bool swapcolors) const = 0;

	/**
	 * @brief Get the current brush size
	 * @return size of the current brush
	 */
	virtual int getSize() const = 0;

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

	/**
	 * @brief Save the settings of this tool
	 */
	void saveSettings();

	/**
	 * @brief Load settings for this tool
	 */
	void restoreSettings();

protected:
	virtual QWidget *createUiWidget(QWidget *parent) = 0;
	virtual void saveToolSettings(QSettings &) {}
	virtual void restoreToolSettings(QSettings &) {}

private:
	QString _name;
	QString _title;
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
	const paintcore::Brush& getBrush(bool swapcolors) const;

	int getSize() const;

protected:
	virtual QWidget *createUiWidget(QWidget *parent);
	virtual void saveToolSettings(QSettings &cfg);
	virtual void restoreToolSettings(QSettings &cfg);

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
	const paintcore::Brush& getBrush(bool swapcolors) const;

	int getSize() const;

protected:
	virtual QWidget *createUiWidget(QWidget *parent);
	virtual void saveToolSettings(QSettings &cfg);
	virtual void restoreToolSettings(QSettings &cfg);

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
	const paintcore::Brush& getBrush(bool swapcolors) const;

	int getSize() const;

protected:
	virtual QWidget *createUiWidget(QWidget *parent);
	virtual void saveToolSettings(QSettings &cfg);
	virtual void restoreToolSettings(QSettings &cfg);

private:
	Ui_BrushSettings *_ui;
};

/**
 * @brief Settings for tools without pressure sensitivity
 */
class SimpleSettings : public ToolSettings {
public:
	enum Type {Line, Rectangle, Ellipse};

	SimpleSettings(QString name, QString title, Type type, bool sp);
	~SimpleSettings();

	void setForeground(const QColor& color);
	void setBackground(const QColor& color);
	void quickAdjust1(float adjustment);
	const paintcore::Brush& getBrush(bool swapcolors) const;

	int getSize() const;

protected:
	virtual QWidget *createUiWidget(QWidget *parent);
	virtual void saveToolSettings(QSettings &cfg);
	virtual void restoreToolSettings(QSettings &cfg);

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
	BrushlessSettings(QString name,QString title) : ToolSettings(name, title) {}

	const paintcore::Brush& getBrush(bool swapcolors) const;
	void setForeground(const QColor& color);
	void setBackground(const QColor& color);
	void quickAdjust1(float adjustment) { Q_UNUSED(adjustment); }

	int getSize() const { return 0; }

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

	//! Set the client to use for edit commands
	void setClient(net::Client *client) { _client = client; }

	//! Set the layer selection widget (needed for baking)
	void setLayerSelector(docks::LayerList *layerlist) { _layerlist = layerlist; }

	/**
	 * @brief Get the ID of the currently selected annotation
	 * @return ID or 0 if none selected
	 */
	int selected() const;

public slots:
	//! Set the currently selected annotation item
	void setSelection(drawingboard::AnnotationItem *item);

	//! Unselect this item if currently selected
	void unselect(int id);

private slots:
	void changeAlignment();
	void toggleBold(bool bold);
	void toggleStrikethrough(bool strike);
	void updateStyleButtons();

	void applyChanges();
	void saveChanges();
	void removeAnnotation();
	void bake();

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:

	Ui_TextSettings *_ui;
	QWidget *_uiwidget;

	QPointer<drawingboard::AnnotationItem> _selection;

	bool _noupdate;
	net::Client *_client;
	docks::LayerList *_layerlist;
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

public slots:
	void addColor(const QColor &color);

signals:
	void colorSelected(const QColor &color);

protected:
	virtual QWidget *createUiWidget(QWidget *parent);
	virtual void saveToolSettings(QSettings &cfg);
	virtual void restoreToolSettings(QSettings &cfg);

private:
	Palette _palette;
	widgets::PaletteWidget *_palettewidget;
	QCheckBox *_layerpick;
};

class SelectionSettings : public BrushlessSettings {
public:
	SelectionSettings(const QString &name, const QString &title);
	~SelectionSettings();

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Ui_SelectionSettings * _ui;
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
	const paintcore::Brush& getBrush(bool swapcolors) const;

signals:
	void pointerTrackingToggled(bool);

protected:
	virtual QWidget *createUiWidget(QWidget *parent);
	virtual void saveToolSettings(QSettings &cfg);
	virtual void restoreToolSettings(QSettings &cfg);

private:
	Ui_LaserSettings * _ui;
};

}

#endif

