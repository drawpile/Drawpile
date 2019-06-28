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

#include "palette.h"

#include <QDebug>
#include <QVariant>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QStandardPaths>

Palette::Palette(QObject *parent) : Palette(QString(), QString(), false, parent) { }
Palette::Palette(const QString &name, QObject *parent) : Palette(name, QString(), false, parent) { }

Palette::Palette(const QString& name, const QString& filename, bool readonly, QObject *parent)
	: QObject(parent), _name(name), _oldname(name), _filename(filename), _columns(8), _modified(false), _readonly(readonly), _writeprotect(false)
{
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
 * @param writeprotected is the source file read only
 */
Palette *Palette::fromFile(const QFileInfo& file, bool readonly, QObject *parent)
{
	QFile palfile(file.absoluteFilePath());
	if (!palfile.open(QIODevice::ReadOnly | QIODevice::Text))
		return nullptr;

	QTextStream in(&palfile);
	if(in.readLine() != "GIMP Palette")
		return nullptr;

	Palette *pal = new Palette(file.baseName(), file.absoluteFilePath(), !file.isWritable() | readonly, parent);

	const QRegularExpression colorRe("^(\\d+)\\s+(\\d+)\\s+(\\d+)\\s*(.+)?$");

	do {
		QString line = in.readLine().trimmed();
		if(line.isEmpty() || line.at(0) == '#') {
			// ignore comments and empty lines

		} else if(line.startsWith("Name:")) {
			pal->_name = line.mid(5).trimmed();

		} else if(line.startsWith("Columns:")) {
			bool ok;
			int cols = line.mid(9).trimmed().toInt(&ok);
			if(ok && cols>0)
				pal->_columns = cols;

		} else {
			QRegularExpressionMatch m = colorRe.match(line);
			if(m.hasMatch()) {
				pal->_colors.append(PaletteColor(
					QColor(
						m.captured(1).toInt(),
						m.captured(2).toInt(),
						m.captured(3).toInt()
					),
					m.captured(4)
					)
				);

			} else {
				qWarning() << "unhandled line" << line << "in" << file.fileName();
			}
		}
	} while(!in.atEnd());

	// Palettes loaded from file are write-protected by default
	pal->_writeprotect = true;

	return pal;
}

Palette *Palette::copy(const Palette *pal, const QString &newname, QObject *parent)
{
	Q_ASSERT(pal);
	Palette *p = new Palette(newname, parent);
	p->_columns = pal->_columns;
	p->_colors = pal->_colors;
	p->_modified = true;
	return p;
}

/**
 * @param filename palette file name
 */
bool Palette::save(const QString& filename)
{
	QDir dir = QFileInfo(filename).absoluteDir();
	if(!dir.exists()) {
		if(!dir.mkpath(".")) {
			qWarning() << "Couldn't create missing directory:" << dir;
			return false;
		}
	}

	return exportPalette(filename);
}

bool Palette::exportPalette(const QString &filename, QString *errorString)
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
		return true;

	} else {
		if(errorString)
			*errorString = data.errorString();
		qWarning() << filename << data.errorString();
		return false;
	}
}

bool Palette::save()
{
	if(_readonly)
		return false;

	QString oldpath;
	if(_name != _oldname) {
		// Name has changed: we need to delete the old palette
		oldpath = _filename;
		_filename = QString();
	}

	if(_filename.isEmpty()) {
		// No filename set? Create it from the palette name
		_filename = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/palettes/" + _name + ".gpl";
	}

	bool ok = save(_filename);
	if(ok) {
		_oldname = _name;
		_modified = false;

		if(!oldpath.isEmpty() && oldpath != _filename)
			QFile(oldpath).remove();
	}

	return ok;
}

bool Palette::deleteFile()
{
	if(_filename.isEmpty() || _readonly)
		return false;
	return QFile(_filename).remove();
}

/**
 * Change the palette name.
 * The filename is set as the name + extension ".gpl"
 * @param name new palette name
 */
void Palette::setName(const QString& name)
{
	if(_name != name) {
		_name = name;
		_modified = true;
		emit nameChanged();
	}
}

void Palette::setColumns(int columns)
{
	if(_columns != columns) {
		_columns = columns;
		_modified = true;
		emit columnsChanged();
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
	if(_readonly)
		return;

	_colors[index] = color;
	_modified = true;
	emit colorsChanged();
}

void Palette::insertColor(int index, const QColor& color, const QString &name)
{
	if(_readonly)
		return;

	_colors.insert(index, PaletteColor(color, name));
	_modified = true;
	emit colorsChanged();
}

void Palette::appendColor(const QColor &color, const QString &name)
{
	if(_readonly)
		return;

	_colors.append(PaletteColor(color, name));
	_modified = true;
	emit colorsChanged();
}

/**
 * Size of the palette is decreased by one.
 * @param index color index
 * @pre 0 <= index < count()
*/
void Palette::removeColor(int index)
{
	if(_readonly)
		return;

	_colors.removeAt(index);
	_modified = true;
	emit colorsChanged();
}

