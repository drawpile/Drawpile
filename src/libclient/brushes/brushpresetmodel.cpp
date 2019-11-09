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
#include "../utils/paths.h"

#include <QVector>
#include <QPixmap>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QUuid>
#include <QSet>
#include <QTimer>
#include <QDebug>

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

	const auto path = utils::paths::writablePath("brushes/", filename);

	QFile f(path);
	if(!f.open(QFile::WriteOnly)) {
		qWarning() << path << f.errorString();
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
		QImage prevImage;
		if(prevImage.load(utils::paths::writablePath("brushes/", name + "_prev.png"))) {
			icon = QPixmap::fromImage(prevImage.scaled(BRUSH_ICON_SIZE, BRUSH_ICON_SIZE));
		} else {
			// If not, generate a preview image
			icon = QPixmap::fromImage(makePreviewIcon(brush));
		}

		return icon;
	}
};

struct PresetFolder {
	QString title;
	QVector<BrushPreset> presets;
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
	QVector<PresetFolder> folders;
	QSet<QString> deleted;
	QTimer *saveTimer;
};

BrushPresetModel::BrushPresetModel(QObject *parent)
	: QAbstractItemModel(parent), d(new Private)
{
	qRegisterMetaType<BrushPreset>();
	qRegisterMetaTypeStreamOperators<BrushPreset>("BrushPreset");

	// A timer is used to delay the saving of changes to disk.
	// This serves two purpose:
	// First: reordering brushes is a three step process:
	//  1. a new empty brush is added
	//  2. its content is set
	//  3. the old brush is deleted
	// We shouldn't write anything until the whole process has completed
	// Second: users often perform multiple operations in succession (especially when renaming a folder.)
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
	const auto basepath = utils::paths::writablePath("brushes/", ".");

	// First, delete all to-be-deleted brushes
	for(const auto &filename : d->deleted) {
		const auto filepath = basepath + filename;
		qDebug() << "Deleting brush" << filepath;
		QFile(filepath).remove();
	}
	d->deleted.clear();

	// Then save all the unsaved brushes
	for(auto &folder : d->folders) {
		for(auto &bp : folder.presets) {
			if(!bp.saved) {
				BrushPresetModel::writeBrush(bp.brush, bp.filename);
				bp.saved = true;
			}
		}
	}

	// Save brush folder metadata
	QFile meta {basepath + "index.txt"};
	if(!meta.open(QFile::WriteOnly)) {
		qWarning("Couldn't open brush metadata file for writing");
		return;
	}
	QTextStream out(&meta);

	for(int i=0;i<d->folders.size();++i) {
		// The first folder is always the implicit "Default" folder
		if(i>0)
			out << '\\' << d->folders.at(i).title << '\n';

		for(const auto &bp : d->folders.at(i).presets) {
			out << bp.filename << '\n';
		}
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
		if(m->d->folders.size()==1 && m->d->folders.first().presets.isEmpty()) {
			if(!migrateQSettingsBrushPresets())
				makeDefaultBrushes();
			m->loadBrushes();
		}
	}
	return m;
}

QModelIndex BrushPresetModel::parent(const QModelIndex &index) const
{
	if(index.internalId() < 1 || index.internalId() > quintptr(d->folders.size()))
		return QModelIndex();
	return createIndex(int(index.internalId() - 1), 0, quintptr(0));
}

QModelIndex BrushPresetModel::index(int row, int column, const QModelIndex &parent) const
{
	if(column != 0 || parent.internalId() != 0)
		return QModelIndex();

	if(parent.isValid()) {
		const int f = parent.row();
		if(f < 0 || f >= d->folders.size() || row < 0 || row >= d->folders.at(f).presets.size())
			return QModelIndex();

		if(row < 0 || row >= d->folders.at(f).presets.size())
			return QModelIndex();

		return createIndex(row, 0, parent.row()+1);
	}

	if(row < 0 || row >= d->folders.size())
		return QModelIndex();

	return createIndex(row, 0, quintptr(0));
}


int BrushPresetModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid() && parent.internalId() == 0)
		return d->folders.at(parent.row()).presets.size();
	else if(!parent.isValid())
		return d->folders.size();
	return 0;
}

int BrushPresetModel::columnCount(const QModelIndex &) const
{
	return 1;
}

QVariant BrushPresetModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid()) {
		if(index.internalId() > 0) {
			auto& folder = d->folders[index.internalId()-1];
			switch(role) {
			case Qt::DecorationRole: return folder.presets[index.row()].getIcon();
			case Qt::SizeHintRole: return QSize(BRUSH_ICON_SIZE + 7, BRUSH_ICON_SIZE + 7);
			//case Qt::ToolTipRole: return d->presets.at(index.row()).value(brushprop::label);
			case BrushPresetRole: return QVariant::fromValue(folder.presets.at(index.row()));
			case BrushRole: return QVariant::fromValue(folder.presets.at(index.row()).brush);
			}

		} else {
			switch(role) {
			case Qt::EditRole:
			case Qt::DisplayRole: return d->folders.at(index.row()).title;
			}
		}
	}
	return QVariant();
}

Qt::ItemFlags BrushPresetModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

	if(index.isValid() && index.internalId() > 0)
		flags |= Qt::ItemIsDragEnabled | Qt::ItemNeverHasChildren;
	else
		flags |= Qt::ItemIsDropEnabled;

	return flags;
}

QMap<int,QVariant> BrushPresetModel::itemData(const QModelIndex &index) const
{
	QMap<int,QVariant> roles;
	if(index.isValid() && index.internalId() > 0) {
		const auto &folder = d->folders.at(index.internalId()-1);
		roles[BrushPresetRole] = QVariant::fromValue(folder.presets.at(index.row()));
	}
	return roles;
}

bool BrushPresetModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if(!index.isValid())
		return false;

	if(index.internalId() > 0) {
		auto &preset = d->folders[index.internalId()-1].presets[index.row()];
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
		d->deleted.remove(preset.filename);
		preset.saved = false;

	} else {
		auto &folder = d->folders[index.row()];
		switch(role) {
			case Qt::EditRole:
			case Qt::DisplayRole: {
				const auto text = value.toString();
				if(text.isEmpty() || text == folder.title)
					return false;
				folder.title = text;
				break;
			}
			default: return false;
		}
	}

	emit dataChanged(index, index);
	d->saveTimer->start();

	return true;
}

bool BrushPresetModel::insertRows(int row, int count, const QModelIndex &parent)
{
	if(!parent.isValid() || parent.internalId() != 0)
		return false;

	auto &folder = d->folders[parent.row()];
	if(row<0 || count<=0 || row > folder.presets.size())
		return false;

	beginInsertRows(parent, row, row+count-1);
	for(int i=0;i<count;++i) {
		const BrushPreset bp {ClassicBrush{}, randomBrushName(), QPixmap(), false };
		folder.presets.insert(row, bp);
	}
	endInsertRows();
	d->saveTimer->start();
	return true;
}

bool BrushPresetModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if(!parent.isValid()) {
		beginRemoveRows(QModelIndex(), row, row+count-1);
		for(int i=row;i<row+count;++i) {
			const auto &folder = d->folders.at(i);
			for(const auto &bp : folder.presets)
				d->deleted << bp.filename;
		}
		d->folders.erase(d->folders.begin()+row, d->folders.begin()+row+count);
		endRemoveRows();

	} else {
		auto &folder = d->folders[parent.row()];
		if(row<0 || count<=0 || row+count > folder.presets.size())
			return false;

		beginRemoveRows(parent, row, row+count-1);
		for(int i=row;i<row+count;++i)
			d->deleted << folder.presets.at(i).filename;
		folder.presets.erase(folder.presets.begin()+row, folder.presets.begin()+row+count);
		endRemoveRows();
	}

	d->saveTimer->start();

	return true;
}

Qt::DropActions BrushPresetModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

void BrushPresetModel::addBrush(int folderIndex, const ClassicBrush &brush)
{
	const auto p = index(folderIndex);
	Q_ASSERT(p.isValid());

	BrushPreset bp { brush, randomBrushName(), QPixmap(), false };
	auto& folder = d->folders[folderIndex];
	beginInsertRows(p, folder.presets.size(), folder.presets.size());
	folder.presets.append(bp);
	endInsertRows();
	d->saveTimer->start();
}

void BrushPresetModel::addFolder(const QString &title)
{
	beginInsertRows(QModelIndex(), d->folders.size(), d->folders.size());
	d->folders << PresetFolder { title, QVector<BrushPreset>() };
	endInsertRows();
}

bool BrushPresetModel::moveBrush(const QModelIndex &brushIndex, int targetFolder)
{
	if(!brushIndex.isValid() || brushIndex.internalId()==0)
		return false;

	auto targetIdx = index(targetFolder);
	if(!targetIdx.isValid())
		return false;

	const auto bp = d->folders[brushIndex.internalId()-1].presets[brushIndex.row()];
	beginRemoveRows(brushIndex.parent(), brushIndex.row(), brushIndex.row());
	d->folders[brushIndex.internalId()-1].presets.remove(brushIndex.row());
	endRemoveRows();
	beginInsertRows(targetIdx, rowCount(targetIdx), rowCount(targetIdx));
	d->folders[targetFolder].presets << bp;
	endInsertRows();

	d->saveTimer->start();

	return true;
}

typedef QVector<QPair<QString,QStringList>> Metadata;

static Metadata loadMetadata(QFile &metadataFile)
{
	QTextStream in(&metadataFile);
	Metadata metadata;
	QString line;

	// The first folder is always the "Default" folder
	metadata << QPair<QString,QStringList> { BrushPresetModel::tr("Default"), QStringList() };

	while(!(line=in.readLine()).isNull()) {
		// # is reserved for comments for now, but might be used for extra metadata fields later
		if(line.isEmpty() || line.at(0) == '#')
			continue;

		// Lines starting with \ start new folders. Nested folders are not supported currently.
		if(line.at(0) == '\\') {
			metadata << QPair<QString,QStringList> { line.mid(1), QStringList() };
			continue;
		}

		metadata.last().second << line;
	}

	return metadata;
}

void BrushPresetModel::loadBrushes()
{
	const auto brushDir = utils::paths::writablePath("brushes/");

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
			qWarning() << "Couldn't open" << filename << "due to:" << f.errorString();
			continue;
		}

		QJsonParseError err;
		QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
		if(doc.isNull()) {
			qWarning() << "Couldn't parse" << filename << "due to:" << err.errorString();
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
	Metadata metadata;

	QFile metadataFile{brushDir + "index.txt"};
	if(metadataFile.open(QFile::ReadOnly)) {
		metadata = loadMetadata(metadataFile);
	}

	// Apply ordering
	QVector<PresetFolder> folders;

	for(const auto &metaFolder : metadata) {
		folders << PresetFolder { metaFolder.first, QVector<BrushPreset>() };
		auto& folder = folders.last();

		for(const auto &filename : metaFolder.second) {
			const auto b = brushes.take(filename);
			if(!b.filename.isEmpty())
				folder.presets << b;
		}
	}

	// Add any remaining unsorted presets to a default folder
	if(folders.isEmpty())
		folders.push_front(PresetFolder { tr("Default"), QVector<BrushPreset>() });

	auto& defaultFolder = folders.first();

	QMapIterator<QString,BrushPreset> i{brushes};
	while(i.hasNext())
		defaultFolder.presets << i.next().value();

	beginResetModel();
	d->folders = folders;
	endResetModel();
}

}
