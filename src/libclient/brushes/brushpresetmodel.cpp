/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#include "brushpresetmodel.h"
#include "pixelbrushpainter.h"
#include "classicbrushpainter.h"
#include "brush.h"

#include "brushpresetmigration.h"

#include "../core/brushmask.h"
#include "../utils/icon.h"

#include <QVector>
#include <QPixmap>
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace brushes {

static constexpr int BRUSH_ICON_SIZE = 42;

static QString randomBrushName()
{
	auto uuid = QUuid::createUuid().toString();
	return uuid.mid(1, uuid.length()-2) + ".dpbrush";
}

bool BrushPresetModel::writeBrush(const ClassicBrush &brush, const QString &filename)
{
	QDir dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/brushes/");
	dir.mkpath(".");

	const QString path = dir.absoluteFilePath(filename.isEmpty() ? randomBrushName() : filename);

	QFile f(path);
	if(!f.open(QFile::WriteOnly)) {
		qWarning("%s: %s", qPrintable(path), qPrintable(f.errorString()));
		return false;
	}

	const QByteArray json = QJsonDocument(brush.toJson()).toJson(QJsonDocument::Indented);
	return f.write(json) == json.length();
}

struct BrushPreset {
	ClassicBrush brush;
	QString filename;
	QPixmap icon;

	bool save()
	{
		if(filename.isEmpty()) {
			qWarning("Cannot save brush: filename not set!");
			return false;
		}
		return BrushPresetModel::writeBrush(brush);
	}

	bool deleteFile()
	{
		if(filename.isEmpty()) {
			qWarning("Cannot delete brush: filename not set!");
			return false;
		}

		return QFile(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/brushes/" + filename).remove();
	}
};

}

Q_DECLARE_METATYPE(brushes::BrushPreset)

namespace brushes {

struct BrushPresetModel::Private {
	QVector<BrushPreset> presets;

	QPixmap getIcon(int idx) {
		Q_ASSERT(idx >=0 && idx < presets.size());

		if(presets.at(idx).icon.isNull()) {
			auto &preset = presets[idx];
			const auto &brush = preset.brush;

			paintcore::BrushMask mask;
			switch(preset.brush.shape()) {
			case ClassicBrush::ROUND_PIXEL:
				mask = brushes::makeRoundPixelBrushMask(brush.size1(), brush.opacity1()*255);
				break;
			case ClassicBrush::SQUARE_PIXEL:
				mask = brushes::makeSquarePixelBrushMask(brush.size1(), brush.opacity1()*255);
				break;
			case ClassicBrush::ROUND_SOFT:
				mask = brushes::makeGimpStyleBrushStamp(QPointF(), brush.size1(), brush.hardness1(), brush.opacity1()).mask;
				break;
			}

			const int maskdia = mask.diameter();
			QImage icon(BRUSH_ICON_SIZE, BRUSH_ICON_SIZE, QImage::Format_ARGB32_Premultiplied);

			const QRgb color = (brush.smudge1()>0) ? 0x001d99f3 : (icon::isDarkThemeSelected() ? 0x00ffffff : 0);

			if(maskdia > BRUSH_ICON_SIZE) {
				// Clip to fit
				const int clip = (maskdia - BRUSH_ICON_SIZE);
				const uchar *m = mask.data() + (clip/2*maskdia) + clip/2;
				for(int y=0;y<BRUSH_ICON_SIZE;++y) {
					quint32 *scanline = reinterpret_cast<quint32*>(icon.scanLine(y));
					for(int x=0;x<BRUSH_ICON_SIZE;++x,++m) {
						*(scanline++) = qPremultiply((*m << 24) | color);
					}
					m += clip;
				}

			} else {
				// Center in the icon
				icon.fill(Qt::transparent);
				const uchar *m = mask.data();
				const int offset = (BRUSH_ICON_SIZE - maskdia)/2;
				for(int y=0;y<maskdia;++y) {
					quint32 *scanline = reinterpret_cast<quint32*>(icon.scanLine(y+offset)) + offset;
					for(int x=0;x<maskdia;++x,++m) {
						*(scanline++) = qPremultiply((*m << 24) | color);
					}
				}
			}

			preset.icon = QPixmap::fromImage(icon);
		}
		return presets.at(idx).icon;
	}
};

BrushPresetModel::BrushPresetModel(QObject *parent)
	: QAbstractListModel(parent), d(new Private)
{
}

BrushPresetModel::~BrushPresetModel()
{
	delete d;
}

static void makeDefaultBrushes()
{
	{
		ClassicBrush b;
		b.setShape(ClassicBrush::ROUND_PIXEL);
		b.setSize(16);
		b.setOpacity(1.0);
		b.setSpacing(15);
		b.setSizePressure(true);
		BrushPresetModel::writeBrush(b, "default-1.dpbrush");
	}
	{
		ClassicBrush b;
		b.setShape(ClassicBrush::ROUND_SOFT);
		b.setSize(10);
		b.setOpacity(1.0);
		b.setHardness(0.8);
		b.setSpacing(15);
		b.setSizePressure(true);
		b.setOpacityPressure(true);
		BrushPresetModel::writeBrush(b, "default-2.dpbrush");
	}
	{
		ClassicBrush b;
		b.setShape(ClassicBrush::ROUND_SOFT);
		b.setSize(30);
		b.setOpacity(0.34);
		b.setHardness(1.0);
		b.setSpacing(18);
		BrushPresetModel::writeBrush(b, "default-3.dpbrush");
	}
	{
		ClassicBrush b;
		b.setShape(ClassicBrush::ROUND_PIXEL);
		b.setIncremental(false);
		b.setSize(32);
		b.setOpacity(0.65);
		b.setSpacing(15);
		BrushPresetModel::writeBrush(b, "default-4.dpbrush");
	}
	{
		ClassicBrush b;
		b.setShape(ClassicBrush::ROUND_PIXEL);
		b.setIncremental(false);
		b.setSize(70);
		b.setOpacity(0.42);
		b.setSpacing(15);
		b.setOpacityPressure(true);
		BrushPresetModel::writeBrush(b, "default-5.dpbrush");
	}
	{
		ClassicBrush b;
		b.setShape(ClassicBrush::ROUND_SOFT);
		b.setSize(113);
		b.setOpacity(0.6);
		b.setHardness(1.0);
		b.setSpacing(19);
		b.setOpacityPressure(true);
		BrushPresetModel::writeBrush(b, "default-6.dpbrush");
	}
	{
		ClassicBrush b;
		b.setShape(ClassicBrush::ROUND_SOFT);
		b.setSize(43);
		b.setOpacity(0.3);
		b.setHardness(1.0);
		b.setSpacing(25);
		b.setSmudge(1.0);
		b.setResmudge(1);
		b.setOpacityPressure(true);
		BrushPresetModel::writeBrush(b, "default-7.dpbrush");
	}
}

BrushPresetModel *BrushPresetModel::getSharedInstance()
{
	static BrushPresetModel *m;
	if(!m) {
		m = new BrushPresetModel;
		m->loadBrushes();
		if(m->d->presets.isEmpty()) {
			if(!migrateQSettingsBrushPresets())
				makeDefaultBrushes();
			m->loadBrushes();
		}
	}
	return m;
}

int BrushPresetModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return d->presets.size();
}

QVariant BrushPresetModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && index.row() >= 0 && index.row() < d->presets.size()) {
		switch(role) {
		case Qt::DecorationRole: return d->getIcon(index.row());
		case Qt::SizeHintRole: return QSize(BRUSH_ICON_SIZE, BRUSH_ICON_SIZE);
		//case Qt::ToolTipRole: return d->presets.at(index.row()).value(brushprop::label);
		case BrushPresetRole: return QVariant::fromValue(d->presets.at(index.row()).brush);
		}
	}
	return QVariant();
}

Qt::ItemFlags BrushPresetModel::flags(const QModelIndex &index) const
{
	if(index.isValid() && index.row() >= 0 && index.row() < d->presets.size()) {
		return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;
	}
	return Qt::ItemIsSelectable | Qt::ItemIsEnabled |  Qt::ItemIsDropEnabled;
}

QMap<int,QVariant> BrushPresetModel::itemData(const QModelIndex &index) const
{
	QMap<int,QVariant> roles;
	if(index.isValid() && index.row()>=0 && index.row()<d->presets.size()) {
		roles[BrushPresetRole] = QVariant::fromValue(d->presets[index.row()]);
	}
	return roles;
}

bool BrushPresetModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if(!index.isValid() || index.row()<0 || index.row()>=d->presets.size())
		return false;
	switch(role) {
		case BrushPresetRole: {
			auto &preset = d->presets[index.row()];
			preset.brush = value.value<ClassicBrush>();
			preset.icon = QPixmap();
			emit dataChanged(index, index);
			preset.save();
			return true;
		}
	}
	return false;
}

bool BrushPresetModel::insertRows(int row, int count, const QModelIndex &parent)
{
	if(parent.isValid())
		return false;
	if(row<0 || count<=0 || row > d->presets.size())
		return false;
	beginInsertRows(QModelIndex(), row, row+count-1);
	for(int i=0;i<count;++i) {
		BrushPreset bp {ClassicBrush{}, randomBrushName(), QPixmap() };
		if(bp.save())
			d->presets.insert(row, bp);
	}
	endInsertRows();
	return true;
}

bool BrushPresetModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if(parent.isValid())
		return false;
	if(row<0 || count<=0 || row+count > d->presets.size())
		return false;
	beginRemoveRows(QModelIndex(), row, row+count-1);
	for(int i=row;i<row+count;++i)
		d->presets[i].deleteFile();
	d->presets.erase(d->presets.begin()+row, d->presets.begin()+row+count);

	endRemoveRows();
	return true;
}

Qt::DropActions BrushPresetModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

void BrushPresetModel::addBrush(const ClassicBrush &brush)
{
	BrushPreset bp { brush, randomBrushName(), QPixmap() };
	if(bp.save()) {
		beginInsertRows(QModelIndex(), d->presets.size(), d->presets.size());
		d->presets.append(bp);
		endInsertRows();
	}
}

void BrushPresetModel::loadBrushes()
{
	QDir brushDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/brushes/");

	QVector<BrushPreset> brushes;

	const auto entries = brushDir.entryList(
		QStringList() << "*.dpbrush",
		QDir::Files | QDir::Readable
	);

	for(const auto &brushfile : entries) {
		QFile f(brushDir.absoluteFilePath(brushfile));
		if(!f.open(QFile::ReadOnly)) {
			qWarning("Couldn't open %s: %s", qPrintable(brushfile), qPrintable(f.errorString()));
			continue;
		}

		QJsonParseError err;
		QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
		if(doc.isNull()) {
			qWarning("Couldn't parse %s: %s", qPrintable(brushfile), qPrintable(err.errorString()));
			continue;
		}

		brushes << BrushPreset { ClassicBrush::fromJson(doc.object()), brushfile, QPixmap() };
	}

	beginResetModel();
	d->presets = brushes;
	endResetModel();
}

}
