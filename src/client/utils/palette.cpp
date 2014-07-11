/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2014 Calle Laakkonen

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

#include "palette.h"

#include <QApplication>
#include <QDebug>
#include <QVariant>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>

Palette::Palette() : _columns(16), _modified(false) { }

Palette::Palette(const QString& name, const QString& filename)
	: _name(name), _filename(filename), _columns(16), _modified(false)
{
	if(_filename.isEmpty())
		_filename = QString("%1.gpl").arg(name);
}

/**
 * Load a palette from a GIMP palette file.
 *
 * The file format is:
 *
 *     GIMP Palette
 *     *HEADER FIELDS*
 *     # one or more comment
 *     r g b	name
 *     ...
 *
 * @param filename palette file name
 */
Palette Palette::fromFile(const QFileInfo& file)
{
	QFile palfile(file.absoluteFilePath());
	if (!palfile.open(QIODevice::ReadOnly | QIODevice::Text))
		return Palette();

	QTextStream in(&palfile);
	if(in.readLine() != "GIMP Palette")
		return Palette();

	Palette pal(file.baseName(), file.fileName());

	const QRegularExpression colorRe("^(\\d+)\\s+(\\d+)\\s+(\\d+)\\s*(.+)?$");

	do {
		QString line = in.readLine().trimmed();
		if(line.isEmpty() || line.at(0) == '#') {
			// ignore comments and empty lines

		} else if(line.startsWith("Name:")) {
			pal._name = line.mid(5).trimmed();

		} else if(line.startsWith("Columns:")) {
			bool ok;
			int cols = line.mid(9).trimmed().toInt(&ok);
			if(ok && cols>0)
				pal._columns = cols;

		} else {
			QRegularExpressionMatch m = colorRe.match(line);
			if(m.hasMatch()) {
				pal.appendColor(
					QColor(
						m.captured(1).toInt(),
						m.captured(2).toInt(),
						m.captured(3).toInt()
					),
					m.captured(4)
				);

			} else {
				qWarning() << "unhandled line" << line << "in" << file.fileName();
			}
		}
	} while(!in.atEnd());

	return pal;
}

/**
 * @param filename palette file name
 */
bool Palette::save(const QString& filename)
{
	QFile data(filename);
	if (data.open(QFile::WriteOnly | QFile::Truncate)) {
		QTextStream out(&data);
		out << "GIMP Palette\n";
		out << "Name: " << _name << "\n";
		out << "Columns: " << _columns << "\n";
		out << "#\n";
		for(const PaletteColor c : _colors) {
			out << c.color.red() << ' ' << c.color.green() << ' ' << c.color.blue() << '\t' << c.name << '\n';
		}
		_modified = false;
		return true;
	}
	return false;
}


/**
 * Generates a palette with some predefined colors.
 * @return a new palette
 */
Palette Palette::makeDefaultPalette()
{
	Palette pal(QApplication::tr("Default"));

	for(int hue=0;hue<352;hue+=16) {
		for(int value=255;value>=15;value-=16) {
			pal.appendColor(QColor::fromHsv(hue,255,value));
		}
	}
	return pal;
}

/**
 * Change the palette name.
 * The filename is set as the name + extension ".gpl"
 * @param name new palette name
 */
void Palette::setName(const QString& name)
{
	_name = name;
	_filename = QString("%1.gpl").arg(name);
}

void Palette::setColumns(int columns)
{
	if(_columns != columns) {
		_columns = columns;
		_modified = true;
	}
}

/**
 * Size of the palette is increased by one. The new color is
 * inserted before the index. If index == count(), the color is
 * added to the end of the palette.
 * @param index color index
 * @param color color
 * @pre 0 <= index <= count()
*/
void Palette::setColor(int index, const PaletteColor& color)
{
	_colors[index] = color;
	_modified = true;
}

void Palette::insertColor(int index, const QColor& color, const QString &name)
{
	_colors.insert(index, PaletteColor(color, name));
	_modified = true;
}

void Palette::appendColor(const QColor &color, const QString &name)
{
	_colors.append(PaletteColor(color, name));
	_modified = true;
}

/**
 * Size of the palette is decreased by one.
 * @param index color index
 * @pre 0 <= index < count()
*/
void Palette::removeColor(int index)
{
	_colors.removeAt(index);
	_modified = true;
}

