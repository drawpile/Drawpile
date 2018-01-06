/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2015 Calle Laakkonen

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
#ifndef LOCALPALETTE_H
#define LOCALPALETTE_H

#include <QList>
#include <QColor>
#include <QObject>

class QFileInfo;

struct PaletteColor {
	QColor color;
	QString name;

	PaletteColor(const QColor &c=QColor(), const QString &n=QString()) : color(c), name(n) { }
};

Q_DECLARE_TYPEINFO(PaletteColor, Q_MOVABLE_TYPE);

class Palette : public QObject {
	Q_OBJECT
public:
	//! Construct a blank palette
	explicit Palette(QObject *parent=0);
	explicit Palette(const QString &name, QObject *parent=0);
	Palette(const QString& name, const QString& filename, bool readonly, QObject *parent=0);

	//! Load a palette from a file
	static Palette *fromFile(const QFileInfo& file, bool readonly=false, QObject *parent=0);

	static Palette *copy(const Palette *pal, const QString &newname, QObject *parent=0);

	//! Set the name of the palette
	void setName(const QString& name);

	//! Get the name of the palette
	const QString& name() const { return _name; }

	//! Get the filename of the palette
	const QString& filename() const { return _filename; }

	//! Get the optimum number of columns to display the colors in
	int columns() const { return _columns; }

	//! Set the optimum number of columns
	void setColumns(int columns);

	//! Has the palette been modified since it was last saved/loaded?
	bool isModified() const { return _modified; }

	//! Is the palette write-protected?
	bool isWriteProtected() const { return _writeprotect | _readonly; }

	//! Set read-only mode
	void setWriteProtected(bool wp) { _writeprotect = wp; }

	//! Is the palette file read-only?
	bool isReadonly() const { return _readonly; }

	//! Save palette to its original file (filename must be set)
	bool save();

	//! Save the palette to an external file
	bool exportPalette(const QString &filename, QString *errorString=nullptr);

	//! Delete the palette file (if it exists)
	bool deleteFile();

	//! Get the number of colors
	int count() const { return _colors.size(); }

	//! Get the color at index
	PaletteColor color(int index) const { return _colors.at(index); }

	//! Set a color at the specified index
	void setColor(int index, const PaletteColor &color);
	void setColor(int index, const QColor& color, const QString &name=QString()) { setColor(index, PaletteColor(color, name)); }

	//! Insert a new color
	void insertColor(int index, const QColor& color, const QString &name=QString());

	//! Append anew color to the end of the palette
	void appendColor(const QColor &color, const QString &name=QString());

	//! Remove a color
	void removeColor(int index);

signals:
	void nameChanged();
	void columnsChanged();
	void colorsChanged();

private:
	bool save(const QString& filename);

	QList<PaletteColor> _colors;
	QString _name;
	QString _oldname;
	QString _filename;

	int _columns;
	bool _modified;
	bool _readonly;
	bool _writeprotect;
};

#endif

