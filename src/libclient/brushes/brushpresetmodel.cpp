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
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QUuid>
#include <QSet>
#include <QTimer>

namespace brushes {

static constexpr int BRUSH_ICON_SIZE = 48;

static QString randomBrushName()
{
	const auto uuid = QUuid::createUuid().toString();
	return uuid.mid(1, uuid.length()-2) + ".dpbrush";
}

bool BrushPresetModel::writeBrush(const ClassicBrush &brush, const QString &filename)
{
	Q_ASSERT(!filename.isEmpty());

	QDir dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/brushes/");
	dir.mkpath(".");

	const auto path = dir.absoluteFilePath(filename);

	QFile f(path);
	if(!f.open(QFile::WriteOnly)) {
		qWarning("%s: %s", qPrintable(path), qPrintable(f.errorString()));
		return false;
	}

	const auto json = QJsonDocument(brush.toJson()).toJson(QJsonDocument::Indented);
	return f.write(json) == json.length();
}

static QImage makePreviewIcon(const ClassicBrush &brush)
{
	paintcore::BrushMask mask;
	switch(brush.shape()) {
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
		// Center the icon
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

	return icon;
}

struct BrushPreset {
	ClassicBrush brush;
	QString filename;
	QPixmap icon;
	bool saved;

	QPixmap getIcon() {
		if(!icon.isNull())
			return icon;

		// See if there is a (MyPaint style) preview image
		const QString name = filename.mid(0, filename.lastIndexOf('.'));
		const QDir dir();
		QImage prevImage;
		if(prevImage.load(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/brushes/" + name + "_prev.png")) {
			icon = QPixmap::fromImage(prevImage.scaled(BRUSH_ICON_SIZE, BRUSH_ICON_SIZE));
		} else {
			// If not, generate a preview image
			icon = QPixmap::fromImage(makePreviewIcon(brush));
		}

		return icon;
	}
};

QDataStream &operator<<(QDataStream &out, const BrushPreset &bp)
{
	return out << bp.brush << bp.filename << bp.icon << bp.saved;
}

QDataStream &operator>>(QDataStream &in, BrushPreset &bp)
{
	return in >> bp.brush >> bp.filename >> bp.icon >> bp.saved;
}

}

Q_DECLARE_METATYPE(brushes::BrushPreset)

namespace brushes {

struct BrushPresetModel::Private {
	QVector<BrushPreset> presets;
	QSet<QString> deleted;
	QTimer *saveTimer;
};

BrushPresetModel::BrushPresetModel(QObject *parent)
	: QAbstractListModel(parent), d(new Private)
{
	qRegisterMetaType<BrushPreset>();
	qRegisterMetaTypeStreamOperators<BrushPreset>("BrushPreset");

	// A timer is used to delay the saving of changes to disk.
	// This serves two purpose:
	// First: reordering brushes is a three step process:
	//  1. a new empty brush is added
	//  2. it's content set
	//  3. the old brush deleted
	// We shouldn't write anything until the whole process has completed
	// Second: users often perform multiple operations in succession.
	// We can avoid some churn by not writing out the changes immediately.
	d->saveTimer = new QTimer(this);
	d->saveTimer->setInterval(1000);
	d->saveTimer->setSingleShot(true);
	connect(d->saveTimer, &QTimer::timeout, this, &BrushPresetModel::saveBrushes);
}

BrushPresetModel::~BrushPresetModel()
{
	delete d;
}

void BrushPresetModel::saveBrushes()
{
	const auto basepath = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/brushes/";

	// First, delete all to-be-deleted brushes
	for(const auto &filename : d->deleted) {
		qDebug("Deleting %s", qPrintable(basepath + filename));
		QFile(basepath).remove();
	}
	d->deleted.clear();

	// Then save all the unsaved brushes
	for(auto &bp : d->presets) {
		if(!bp.saved) {
			qDebug("Saving %s", qPrintable(basepath + bp.filename));
			BrushPresetModel::writeBrush(bp.brush, basepath + bp.filename);
			bp.saved = true;
		}
	}

	// Save brush folder metadata
	QFile meta {basepath + "index.txt"};
	if(!meta.open(QFile::WriteOnly)) {
		qWarning("Couldn't open brush metadata file for writing");
		return;
	}
	QTextStream out(&meta);

	for(const auto &bp : d->presets) {
		out << bp.filename << '\n';
	}
}

static void makeDefaultBrushes()
{
	{
		ClassicBrush b;
		b.setShape(ClassicBrush::ROUND_PIXEL);
		b.setSize(16);
		b.setOpacity(1.0);
		b.setSpacing(0.15);
		b.setSizePressure(true);
		BrushPresetModel::writeBrush(b, "default-1.dpbrush");
	}
	{
		ClassicBrush b;
		b.setShape(ClassicBrush::ROUND_SOFT);
		b.setSize(10);
		b.setOpacity(1.0);
		b.setHardness(0.8);
		b.setSpacing(0.15);
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
		b.setSpacing(0.18);
		BrushPresetModel::writeBrush(b, "default-3.dpbrush");
	}
	{
		ClassicBrush b;
		b.setShape(ClassicBrush::ROUND_PIXEL);
		b.setIncremental(false);
		b.setSize(32);
		b.setOpacity(0.65);
		b.setSpacing(0.15);
		BrushPresetModel::writeBrush(b, "default-4.dpbrush");
	}
	{
		ClassicBrush b;
		b.setShape(ClassicBrush::ROUND_PIXEL);
		b.setIncremental(false);
		b.setSize(70);
		b.setOpacity(0.42);
		b.setSpacing(0.15);
		b.setOpacityPressure(true);
		BrushPresetModel::writeBrush(b, "default-5.dpbrush");
	}
	{
		ClassicBrush b;
		b.setShape(ClassicBrush::ROUND_SOFT);
		b.setSize(113);
		b.setOpacity(0.6);
		b.setHardness(1.0);
		b.setSpacing(0.19);
		b.setOpacityPressure(true);
		BrushPresetModel::writeBrush(b, "default-6.dpbrush");
	}
	{
		ClassicBrush b;
		b.setShape(ClassicBrush::ROUND_SOFT);
		b.setSize(43);
		b.setOpacity(0.3);
		b.setHardness(1.0);
		b.setSpacing(0.25);
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
		case Qt::DecorationRole: return d->presets[index.row()].getIcon();
		case Qt::SizeHintRole: return QSize(BRUSH_ICON_SIZE + 7, BRUSH_ICON_SIZE + 7);
		//case Qt::ToolTipRole: return d->presets.at(index.row()).value(brushprop::label);
		case BrushPresetRole: return QVariant::fromValue(d->presets.at(index.row()));
		case BrushRole: return QVariant::fromValue(d->presets.at(index.row()).brush);
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
		const auto &preset = d->presets[index.row()];
		roles[BrushPresetRole] = QVariant::fromValue(preset);
	}
	return roles;
}

bool BrushPresetModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if(!index.isValid() || index.row()<0 || index.row()>=d->presets.size())
		return false;

	auto &preset = d->presets[index.row()];
	switch(role) {
		case BrushPresetRole:
			preset = value.value<BrushPreset>();
			break;
		case BrushRole:
			preset.brush = value.value<ClassicBrush>();
			preset.icon = QPixmap();
			break;
		default: return false;
	}
	preset.saved = false;
	emit dataChanged(index, index);
	d->saveTimer->start();
	return true;
}

bool BrushPresetModel::insertRows(int row, int count, const QModelIndex &parent)
{
	if(parent.isValid())
		return false;
	if(row<0 || count<=0 || row > d->presets.size())
		return false;
	beginInsertRows(QModelIndex(), row, row+count-1);
	for(int i=0;i<count;++i) {
		const BrushPreset bp {ClassicBrush{}, randomBrushName(), QPixmap(), false };
		d->presets.insert(row, bp);
	}
	endInsertRows();
	d->saveTimer->start();
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
		d->deleted << d->presets.at(i).filename;
	d->presets.erase(d->presets.begin()+row, d->presets.begin()+row+count);
	endRemoveRows();
	d->saveTimer->start();

	return true;
}

Qt::DropActions BrushPresetModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

void BrushPresetModel::addBrush(const ClassicBrush &brush)
{
	BrushPreset bp { brush, randomBrushName(), QPixmap(), false };
	beginInsertRows(QModelIndex(), d->presets.size(), d->presets.size());
	d->presets.append(bp);
	endInsertRows();
	d->saveTimer->start();
}

static QStringList loadMetadata(QFile &metadataFile)
{
	QTextStream in(&metadataFile);
	QStringList order;
	QString line;
	while(!(line=in.readLine()).isNull()) {
		// Forward compatibility:
		// In the future, we'll use the \ prefix to indicate groups
		// # is reserved for comments for now, but might be used for extra metadata fields later
		if(line.isEmpty() || line.at(0) == '#' || line.at(0) == '\\')
			continue;
		order << line;
	}

	return order;
}

void BrushPresetModel::loadBrushes()
{
	const auto brushDir = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/brushes/";

	QMap<QString, BrushPreset> brushes;

	QDirIterator entries{
		brushDir,
		QStringList() << "*.dpbrush",
		QDir::Files | QDir::Readable,
		QDirIterator::Subdirectories
	};

	while(entries.hasNext()) {
		QFile f{entries.next()};
		const auto filename = entries.filePath().mid(brushDir.length());

		if(!f.open(QFile::ReadOnly)) {
			qWarning("Couldn't open %s: %s", qPrintable(filename), qPrintable(f.errorString()));
			continue;
		}

		QJsonParseError err;
		QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
		if(doc.isNull()) {
			qWarning("Couldn't parse %s: %s", qPrintable(filename), qPrintable(err.errorString()));
			continue;
		}

		brushes[filename] = BrushPreset {
			ClassicBrush::fromJson(doc.object()),
			filename,
			QPixmap(),
			true
		};
	}

	// Load metadata file (if present)
	QStringList order;

	QFile metadataFile{brushDir + "index.txt"};
	if(metadataFile.open(QFile::ReadOnly)) {
		order = loadMetadata(metadataFile);
	}

	// Apply ordering
	QVector<BrushPreset> orderedBrushes;

	for(const auto &filename : order) {
		const auto b = brushes.take(filename);
		if(!b.filename.isEmpty())
			orderedBrushes << b;
	}

	QMapIterator<QString,BrushPreset> i{brushes};
	while(i.hasNext())
		orderedBrushes << i.next().value();

	beginResetModel();
	d->presets = orderedBrushes;
	endResetModel();
}

}
