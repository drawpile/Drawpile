/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2015 Calle Laakkonen

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

#include "palettelistmodel.h"
#include "palette.h"
#include "icon.h"
#include "utils/settings.h"

#include <QDebug>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>

PaletteListModel::PaletteListModel(QObject *parent) :
	QAbstractListModel(parent),
	_readonlyEmblem(icon::fromTheme("object-locked"))
{
}

PaletteListModel *PaletteListModel::getSharedInstance()
{
	static PaletteListModel *instance;
	if(!instance) {
		instance = new PaletteListModel;
		instance->loadPalettes();
	}
	return instance;
}

void PaletteListModel::loadPalettes()
{
	QList<Palette*> palettes;

	QStringList datapaths = utils::settings::dataPaths();
	QString writablepath = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
	QSet<QString> palettefiles;

	for(const QString datapath : datapaths) {
		QFileInfoList pfiles = QDir(datapath + "/palettes").entryInfoList(
				QStringList("*.gpl"),
				QDir::Files|QDir::Readable
				);

		for(const QFileInfo &pfile : pfiles) {
			if(!palettefiles.contains(pfile.fileName())) {
				palettefiles.insert(pfile.fileName());

				// QFile::isWritable doesn't seem to work reliably on Windows.
				// As a workaround, mark all palettes outside our own writable directory
				// as read-only

				Palette *pal = Palette::fromFile(pfile, datapath != writablepath, this);
				if(!pal)
					qWarning() << "Invalid palette:" << pfile.absoluteFilePath();

				else
					palettes.append(pal);
			}
		}
	}

	beginResetModel();
	_palettes = palettes;
	endResetModel();
}

void PaletteListModel::saveChanged()
{
	for(Palette *pal : _palettes) {
		if(pal->isModified())
			pal->save();
	}
}

int PaletteListModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;

	return _palettes.size();
}

QVariant PaletteListModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && index.row() >= 0 && index.row() < _palettes.size()) {
		switch(role) {
		case Qt::DisplayRole:
		case Qt::EditRole:
			return _palettes.at(index.row())->name();
		case Qt::DecorationRole:
			if(_palettes.at(index.row())->isReadonly())
				return _readonlyEmblem;
			break;
		}
	}

	return QVariant();
}

bool PaletteListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if(role==Qt::EditRole && index.isValid() && index.row() >= 0 && index.row() < _palettes.size()) {
		QString newname = value.toString().trimmed();

		if(_palettes[index.row()]->isReadonly())
			return false;

		// Name must be non-empty and unique
		if(newname.isEmpty() || !isUniqueName(newname, index.row()))
			return false;

		_palettes[index.row()]->setName(value.toString());
		emit dataChanged(index, index);
		return true;
	}
	return false;
}

Qt::ItemFlags PaletteListModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags f = Qt::NoItemFlags;
	if(index.isValid() && index.row() >= 0 && index.row() < _palettes.size()) {
		f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

		if(!_palettes.at(index.row())->isReadonly())
			f |= Qt::ItemIsEditable;
	}

	return f;
}

Palette *PaletteListModel::getPalette(int index)
{
	Q_ASSERT(index>=0 && index<_palettes.size());
	if(index>=0 && index<_palettes.size())
		return _palettes[index];

	return nullptr;
}

int PaletteListModel::findPalette(const QString &name) const
{
	for(int i=0;i<_palettes.size();++i)
		if(_palettes.at(i)->name() == name)
			return i;
	return -1;
}

void PaletteListModel::addNewPalette()
{
	QString name;
	bool nameOk = false;

	// Autogenerate name
	for(int tries=0;tries<99;++tries) {
		name = tr("New palette");
		if(tries>0)
			name = name + " " + QString::number(tries);

		if(isUniqueName(name)) {
			// name found
			nameOk = true;
			break;
		}
	}

	if(!nameOk)
		return;

	int pos = _palettes.size();
	beginInsertRows(QModelIndex(), pos, pos);
	_palettes.append(new Palette(name, this));
	endInsertRows();
}

bool PaletteListModel::copyPalette(int index)
{
	if(index<0 || index >= _palettes.size())
		return false;

	const Palette *pal = _palettes.at(index);

	QString basename = pal->name();
	while(basename.size()>0 && (basename.at(basename.size()-1).isDigit() || basename.at(basename.size()-1).isSpace()))
		basename.chop(1);
	basename.append(' ');

	QString name;
	for(int tries=2;tries<99;++tries) {
		QString newname = basename + QString::number(tries);

		if(isUniqueName(newname)) {
			name = newname;
			break;
		}
	}

	if(name.isNull())
		return false;

	Palette *copy = Palette::copy(pal, name, this);
	beginInsertRows(QModelIndex(), index, index);
	_palettes.insert(index, copy);
	endInsertRows();

	return true;
}

bool PaletteListModel::isUniqueName(const QString &name, int exclude) const
{
	for(int i=0;i<_palettes.size();++i)
		if(i != exclude && _palettes.at(i)->name().compare(name, Qt::CaseInsensitive) == 0)
			return false;
	return true;
}

bool PaletteListModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if(parent.isValid() || row < 0 || row+count > _palettes.size())
		return false;

	beginRemoveRows(parent, row, row+count-1);

	while(count-->0) {
		Palette *pal = _palettes.takeAt(row);
		pal->deleteFile();
		delete pal;
	}

	endRemoveRows();
	return true;
}
