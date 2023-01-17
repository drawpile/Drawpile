/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019-2022 Calle Laakkonen, askmeaboutloom

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

#include "libclient/brushes/brushpresetmodel.h"
#include "libclient/brushes/brush.h"

#include "libclient/utils/icon.h"
#include "libshared/util/paths.h"
#include "libshared/util/qtcompat.h"
#include "libclient/drawdance/ziparchive.h"

#include <QBuffer>
#include <QDebug>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVector>

namespace brushes {

static constexpr int THUMBNAIL_SIZE = 64;
static constexpr int ALL_ROW = 0;
static constexpr int UNTAGGED_ROW = 1;
static constexpr int TAG_OFFSET = 2;
static constexpr int ALL_ID = -1;
static constexpr int UNTAGGED_ID = -2;

typedef QVector<QPair<QString, QStringList>> OldMetadata;

struct OldBrushPreset {
	ClassicBrush brush;
	QString filename;
};

struct OldPresetFolder {
	QString title;
	QVector<OldBrushPreset> presets;
};

class BrushPresetTagModel::Private {
public:
	Private()
		: m_db(getDbInstance())
	{
	}

	int createTag(const QString &name)
	{
		QSqlQuery query(m_db);
		if(exec(query, "insert into tag (name) values (?)", {name})) {
			return query.lastInsertId().toInt();
		} else {
			return 0;
		}
	}

	int readTagCount()
	{
		return readInt("select count(*) from tag");
	}

	int readTagCountByName(const QString &name)
	{
		return readInt("select count(*) from tag where name = ?", {name});
	}

	int readTagIdAtIndex(int index)
	{
		return readInt("select id from tag order by LOWER(name) limit 1 offset ?", {index});
	}

	Tag readTagAtIndex(int index)
	{
		QSqlQuery query(m_db);
		QString sql = QStringLiteral(
			"select id, name from tag order by id limit 1 offset ?");
		if(exec(query, sql, {index}) && query.next()) {
			return Tag{query.value(0).toInt(), query.value(1).toString(), true};
		} else {
			return Tag{0, "", false};
		}
	}

	QString readTagNameById(int id)
	{
		return readString("select name from tag where id = ?", {id});
	}

	int readTagIndexById(int id)
	{
		QString sql = QStringLiteral(
			"select n - 1 from (\n"
			"	select row_number() over(order by id) as n, id from tag)\n"
			"where id = ?");
		return readInt(sql, {id}, -1);
	}

	bool updateTagName(int id, const QString &name)
	{
		QSqlQuery query(m_db);
		if(exec(query, "update tag set name = ? where id = ?", {name, id})) {
			return query.numRowsAffected() == 1;
		} else {
			return false;
		}
	}

	bool deleteTagById(int id)
	{
		QSqlQuery query(m_db);
		if(exec(query, "delete from tag where id = ?", {id})) {
			return query.numRowsAffected() == 1;
		} else {
			return false;
		}
	}

	int createPreset(const QString &type, const QString &name, const QString &description,
		const QByteArray &thumbnail, const QByteArray &data)
	{
		QSqlQuery query(m_db);
		QString sql = QStringLiteral(
			"insert into preset (type, name, description, thumbnail, data)\n"
			"	values (?, ?, ?, ?, ?)");
		if(exec(query, sql, {type, name, description, thumbnail, data})) {
			return query.lastInsertId().toInt();
		} else {
			return 0;
		}
	}

	int createPresetFromId(int id)
	{
		QSqlQuery query(m_db);
		QString sql = QStringLiteral(
			"insert into preset (type, name, description, thumbnail, data)\n"
			"	select type, name, description, thumbnail, data from preset where id = ?");
		if(exec(query, sql, {id})) {
			return query.lastInsertId().toInt();
		} else {
			return 0;
		}
	}

	int readPresetCountAll()
	{
		return readInt("select count(*) from preset");
	}

	int readPresetCountByUntagged()
	{
		QString sql = QStringLiteral(
			"select count(*) from preset p where not exists(\n"
			"	select 1 from preset_tag pt where pt.preset_id = p.id)");
		return readInt(sql);
	}

	int readPresetCountByTagId(int tagId)
	{
		QString sql = QStringLiteral(
			"select count(*) from preset p\n"
			"	join preset_tag pt on pt.preset_id = p.id\n"
			"	where pt.tag_id = ?");
		return readInt(sql, {tagId});
	}

	int readPresetIdAtIndexAll(int index)
	{
		return readInt("select id from preset order by LOWER(name) limit 1 offset ?", {index});
	}

	int readPresetIdAtIndexByUntagged(int index)
	{
		QString sql = QStringLiteral(
			"select p.id from preset p\n"
			"	where not exists(select 1 from preset_tag pt where pt.preset_id = p.id)\n"
			"	order by LOWER(p.name) limit 1 offset ?");
		return readInt(sql, {index});
	}

	int readPresetIdAtIndexByTagId(int index, int tagId)
	{
		QString sql = QStringLiteral(
			"select p.id from preset p\n"
			"	join preset_tag pt on pt.preset_id = p.id\n"
			"	where pt.tag_id = ?"
			"	order by LOWER(p.name) limit 1 offset ?");
		return readInt(sql, {tagId, index});
	}

	QString readPresetNameById(int id)
	{
		return readString("select name from preset where id = ?", {id});
	}

	QByteArray readPresetThumbnailById(int id)
	{
		return readByteArray("select thumbnail from preset where id = ?", {id});
	}

	QByteArray readPresetDataById(int id)
	{
		return readByteArray("select data from preset where id = ?", {id});
	}

	PresetMetadata readPresetMetadataById(int id)
	{
		QSqlQuery query(m_db);
		QString sql = QStringLiteral(
			"select id, name, description, thumbnail from preset where id = ?");
		if(exec(query, sql, {id}) && query.next()) {
			return PresetMetadata{query.value(0).toInt(), query.value(1).toString(),
				query.value(2).toString(), query.value(3).toByteArray()};
		} else {
			return PresetMetadata{0, QString(), QString(), QByteArray()};
		}
	}

	bool updatePresetData(int id, const QString &type, const QByteArray &data)
	{
		QSqlQuery query(m_db);
		if(exec(query, "update preset set type = ?, data = ? where id = ?", {type, data, id})) {
			return query.numRowsAffected() == 1;
		} else {
			return false;
		}
	}

	bool updatePresetMetadata(int id, const QString &name, const QString &description,
		const QByteArray &thumbnail)
	{
		QSqlQuery query(m_db);
		QString sql = QStringLiteral(
			"update preset set name = ?, description = ?, thumbnail = ? where id = ?");
		if(exec(query, sql, {name, description, thumbnail, id})) {
			return query.numRowsAffected() == 1;
		} else {
			return false;
		}
	}

	bool deletePresetById(int id)
	{
		QSqlQuery query(m_db);
		if(exec(query, "delete from preset where id = ?", {id})) {
			return query.numRowsAffected() == 1;
		} else {
			return false;
		}
	}

	QList<TagAssignment> readTagAssignmentsByPresetId(int presetId)
	{
		QList<TagAssignment> tagAssignments;
		QSqlQuery query(m_db);
		QString sql = QStringLiteral(
			"select t.id, t.name, pt.preset_id is not null from tag t\n"
			"	left outer join preset_tag pt\n"
			"		on pt.tag_id = t.id and pt.preset_id = ?\n"
			"	order by t.id");
		if(exec(query, sql, {presetId})) {
			while(query.next()) {
				tagAssignments.append(TagAssignment{query.value(0).toInt(),
					query.value(1).toString(), query.value(2).toBool()});
			}
		}
		return tagAssignments;
	}

	bool createPresetTag(int presetId, int tagId)
	{
		QSqlQuery query(m_db);
		QString sql = QStringLiteral(
			"insert into preset_tag (preset_id, tag_id) values (?, ?)");
		return exec(query, sql, {presetId, tagId});
	}

	bool createPresetTagsFromPresetId(int targetPresetId, int sourcePresetId)
	{
		QSqlQuery query(m_db);
		QString sql = QStringLiteral(
			"insert into preset_tag (preset_id, tag_id)\n"
			"	select ?, tag_id from preset_tag where preset_id = ?");
		return exec(query, sql, {targetPresetId, sourcePresetId});
	}

	bool deletePresetTag(int presetId, int tagId)
	{
		QSqlQuery query(m_db);
		QString sql = QStringLiteral(
			"delete from preset_tag where preset_id = ? and tag_id = ?");
		if(exec(query, sql, {presetId, tagId})) {
			return query.numRowsAffected() == 1;
		} else {
			return false;
		}
	}

	bool createOrUpdateState(const QString &key, const QVariant &value)
	{
		QSqlQuery query(m_db);
		QString sql = QStringLiteral(
			"insert into state (key, value) values (?, ?)\n"
			"	on conflict (key) do update set value = ?");
		return exec(query, sql, {key, value, value});
	}

	QVariant readState(const QString &key)
	{
		QSqlQuery query(m_db);
		if(exec(query, "select value from state where key = ?", {key}) && query.next()) {
			return query.value(0);
		} else {
			return QVariant();
		}
	}

private:
	QSqlDatabase &m_db;

	int readInt(const QString &sql,const QList<QVariant> &params = {}, int defaultValue = 0)
	{
		QSqlQuery query(m_db);
		if(exec(query, sql, params) && query.next()) {
			return query.value(0).toInt();
		} else {
			return defaultValue;
		}
	}

	QString readString(const QString &sql, const QList<QVariant> &params = {})
	{
		QSqlQuery query(m_db);
		if(exec(query, sql, params) && query.next()) {
			return query.value(0).toString();
		} else {
			return QString();
		}
	}

	QByteArray readByteArray(const QByteArray &sql, const QList<QVariant> &params = {})
	{
		QSqlQuery query(m_db);
		if(exec(query, sql, params) && query.next()) {
			return query.value(0).toByteArray();
		} else {
			return QByteArray();
		}
	}

	static bool exec(QSqlQuery &query, const QString &sql, const QList<QVariant> &params = {})
	{
		if(!query.prepare(sql)) {
			qWarning("Error preparing statement '%s': %s", qPrintable(sql),
				qPrintable(query.lastError().text()));
			return false;
		}

		for (const QVariant &param : params) {
			query.addBindValue(param);
		}

		if(!query.exec()) {
			qWarning("Error executing statement '%s': %s", qPrintable(sql),
				qPrintable(query.lastError().text()));
			return false;
		}
		return true;
	}

	static QString createDb()
	{
		QString databasePath = utils::paths::writablePath("brushpresets.db");
		QFileInfo fileInfo(databasePath);

		QString dirName = fileInfo.canonicalPath();
		if(!QDir().mkpath(dirName)) {
			qWarning("Failed to create brush preset database parent directory '%s'",
				qPrintable(dirName));
		}

		if(!fileInfo.exists()) {
			QString initialPath = utils::paths::locateDataFile("initialbrushpresets.db");
			if(initialPath.isEmpty()) {
				qWarning("No initial brush preset database found");
			} else if(QFile::copy(initialPath, databasePath)) {
				qDebug("Created brush database '%s' from '%s'",
					qPrintable(databasePath), qPrintable(initialPath));
				if(QFile::setPermissions(databasePath, QFileDevice::ReadUser | QFileDevice::WriteUser)) {
					qDebug("Set permissions of '%s' to read-write", qPrintable(databasePath));
				} else {
					qWarning("Could not set permissions of '%s' to read-write", qPrintable(databasePath));
				}
			} else {
				qWarning("Could not create brush database '%s' from '%s'",
					qPrintable(databasePath), qPrintable(initialPath));
			}
		}
		return databasePath;
	}

	static void initDb(QSqlQuery &query)
	{
		exec(query, "pragma foreign_keys = on");
		exec(query,
			"create table if not exists state (\n"
			"	key text primary key not null,\n"
			"	value)");
		exec(query,
			"create table if not exists preset (\n"
			"	id integer primary key not null,\n"
			"	type text not null,\n"
			"	name text not null,\n"
			"	description text not null,\n"
			"	thumbnail blob,\n"
			"	data blob not null)");
		exec(query,
			"create table if not exists tag (\n"
			"	id integer primary key not null,\n"
			"	name text not null)\n");
		exec(query,
			"create table if not exists preset_tag (\n"
			"	preset_id integer not null\n"
			"		references preset (id) on delete cascade,\n"
			"	tag_id integer not null\n"
			"		references tag (id) on delete cascade,\n"
			"	primary key (preset_id, tag_id))");
		exec(query,
			"create index if not exists preset_name_idx\n"
			"	ON preset(LOWER(name))");
		exec(query,
			"create index if not exists tag_name_idx\n"
			"	ON tag(LOWER(name))");
	}

	static QSqlDatabase &getDbInstance()
	{
		static QSqlDatabase db;
		if(!db.isValid()) {
			QString databasePath = createDb();
			db = QSqlDatabase::addDatabase(
				"QSQLITE", QStringLiteral("drawpile_brush_preset_connection"));
			db.setDatabaseName(databasePath);
			if(db.open()) {
				QSqlQuery query(db);
				initDb(query);
			} else {
				qWarning("Can't open brush preset model database at '%s'",
					qPrintable(db.databaseName()));
			}
		}
		return db;
	}
};

BrushPresetTagModel::BrushPresetTagModel(QObject *parent)
	: QAbstractItemModel(parent)
	, d(new Private)
	, m_presetModel(new BrushPresetModel(this))
{
	maybeConvertOldPresets();
	connect(this, &QAbstractItemModel::modelAboutToBeReset,
		m_presetModel, &BrushPresetModel::tagsAboutToBeReset);
	connect(this, &QAbstractItemModel::modelReset,
		m_presetModel, &BrushPresetModel::tagsReset);
}

BrushPresetTagModel::~BrushPresetTagModel()
{
	delete d;
}

int BrushPresetTagModel::rowCount(const QModelIndex &) const
{
	return d->readTagCount() + TAG_OFFSET;
}

int BrushPresetTagModel::columnCount(const QModelIndex &) const
{
	return 1;
}

QModelIndex BrushPresetTagModel::parent(const QModelIndex &) const
{
	return QModelIndex();
}

QModelIndex BrushPresetTagModel::index(int row, int column, const QModelIndex &) const
{
	if(isBuiltInTag(row)) {
		return createIndex(row, column);
	} else {
		int tagId = d->readTagIdAtIndex(row - TAG_OFFSET);
		if(tagId > 0) {
			return createIndex(row, column, quintptr(tagId));
		} else {
			return QModelIndex();
		}
	}
}

QVariant BrushPresetTagModel::data(const QModelIndex &index, int role) const
{
	switch(role) {
	case Qt::DecorationRole:
		switch(index.row()) {
		case ALL_ROW:
			return icon::fromTheme("folder");
		case UNTAGGED_ROW:
			return icon::fromTheme("folder-new");
		default:
			return QVariant();
		}
	case Qt::DisplayRole:
	case Qt::EditRole:
		switch(index.row()) {
		case ALL_ROW:
			return tr("All");
		case UNTAGGED_ROW:
			return tr("Untagged");
		default:
			return d->readTagNameById(index.internalId());
		}
	case Qt::ToolTipRole:
		switch(index.row()) {
		case ALL_ROW:
			return tr("Show all brushes, regardless of tagging.");
		case UNTAGGED_ROW:
			return tr("Show brushes not assigned to any tag.");
		default:
			return QVariant();
		}
	default:
		return QVariant();
	}
}

Tag BrushPresetTagModel::getTagAt(int row) const
{
	switch(row) {
	case ALL_ROW:
		return Tag{ALL_ID, data(createIndex(row, 0)).toString(), false};
	case UNTAGGED_ROW:
		return Tag{UNTAGGED_ID, data(createIndex(row, 0)).toString(), false};
	default:
		return d->readTagAtIndex(row - TAG_OFFSET);
	}
}

int BrushPresetTagModel::getTagRowById(int tagId) const
{
	switch(tagId) {
	case ALL_ID:
		return ALL_ROW;
	case UNTAGGED_ID:
		return UNTAGGED_ROW;
	default:
		int index = d->readTagIndexById(tagId);
		return index < 0 ? -1 : index + TAG_OFFSET;
	}
}

void BrushPresetTagModel::setState(const QString &key, const QVariant &value)
{
	d->createOrUpdateState(key, value);
}

QVariant BrushPresetTagModel::getState(const QString &key) const
{
	return d->readState(key);
}

int BrushPresetTagModel::newTag(const QString& name)
{
	beginResetModel();
	int tagId = d->createTag(name);
	endResetModel();
	return tagId;
}

int BrushPresetTagModel::editTag(int tagId, const QString &name)
{
	beginResetModel();
	d->updateTagName(tagId, name);
	endResetModel();
	return getTagRowById(tagId);
}

void BrushPresetTagModel::deleteTag(int tagId)
{
	beginResetModel();
	d->deleteTagById(tagId);
	endResetModel();
}

bool BrushPresetTagModel::isBuiltInTag(int row)
{
	return row == ALL_ROW || row == UNTAGGED_ROW;
}

void BrushPresetTagModel::maybeConvertOldPresets()
{
	if(!d->readState("classic_presets_loaded").toBool() &&
			d->createOrUpdateState("classic_presets_loaded", true)) {
		convertOldPresets();
	}
}

static OldMetadata loadOldMetadata(QFile &metadataFile)
{
	QTextStream in(&metadataFile);
	OldMetadata metadata;
	QString line;

	// The first folder is always the "Default" folder
	metadata << QPair<QString, QStringList> { BrushPresetModel::tr("Default"), QStringList() };

	while(!(line=in.readLine()).isNull()) {
		// # is reserved for comments for now, but might be used for extra metadata fields later
		if(line.isEmpty() || line.at(0) == '#')
			continue;

		// Lines starting with \ start new folders. Nested folders are not supported currently.
		if(line.at(0) == '\\') {
			metadata << QPair<QString, QStringList> { line.mid(1), QStringList() };
			continue;
		}

		metadata.last().second << line;
	}

	return metadata;
}

void BrushPresetTagModel::convertOldPresets()
{
	QString brushDir = utils::paths::writablePath("brushes/");

	QMap<QString, OldBrushPreset> brushes;

	QDirIterator entries{
		brushDir,
		QStringList() << "*.dpbrush",
		QDir::Files | QDir::Readable,
		QDirIterator::Subdirectories
	};

	while(entries.hasNext()) {
		QFile f{entries.next()};
		QString filename = entries.filePath().mid(brushDir.length());

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

		brushes[filename] = OldBrushPreset {
			ClassicBrush::fromJson(doc.object()),
			filename,
		};
	}

	// Load metadata file (if present)
	OldMetadata metadata;

	QFile metadataFile{brushDir + "index.txt"};
	if(metadataFile.open(QFile::ReadOnly)) {
		metadata = loadOldMetadata(metadataFile);
	}

	// Apply ordering
	QVector<OldPresetFolder> folders;

	for(const QPair<QString, QStringList>  &metaFolder : metadata) {
		folders << OldPresetFolder { metaFolder.first, QVector<OldBrushPreset>() };
		OldPresetFolder& folder = folders.last();

		for(const QString &filename : metaFolder.second) {
			const OldBrushPreset b = brushes.take(filename);
			if(!b.filename.isEmpty()) {
				folder.presets << b;
			}
		}
	}

	// Add any remaining unsorted presets to a default folder
	if(folders.isEmpty()) {
		folders.push_front(OldPresetFolder { tr("Default"), QVector<OldBrushPreset>() });
	}

	OldPresetFolder& defaultFolder = folders.first();

	QMapIterator<QString, OldBrushPreset> i{brushes};
	while(i.hasNext()) {
		defaultFolder.presets << i.next().value();
	}

	if(folders.size() > 1 || !folders.first().presets.isEmpty()) {
		ActiveBrush brush(ActiveBrush::CLASSIC);
		int brushCount = 0;

		for (const OldPresetFolder &folder : folders) {
			int tagId = newTag(folder.title);
			for(const OldBrushPreset &preset : folder.presets) {
				++brushCount;
				QString name = tr("Classic Brush %1").arg(brushCount);
				QString description = tr("Converted from %1.").arg(preset.filename);
				brush.setClassic(preset.brush);
				int presetId = m_presetModel->newPreset(brush.presetType(), name,
					description, brush.presetThumbnail(), brush.presetData());
				if(tagId > 0 && presetId > 0) {
					m_presetModel->changeTagAssignment(presetId, tagId, true);
				}
			}
		}
	}
}

bool BrushPresetTagModel::importMyPaintBrushPack(
		const QString &file, int &outTagId, QString &outTagName,
		QStringList &outErrors)
{
	drawdance::ZipReader zr{file};
	if(zr.isNull()) {
		qWarning("Error opening '%s': %s", qUtf8Printable(file), DP_error());
		outErrors.append(tr("Can't open '%1'.").arg(qUtf8Printable(file)));
		return false;
	}

	drawdance::ZipReader::File orderFile = zr.readFile(QStringLiteral("order.conf"));
	if(orderFile.isNull()) {
		qWarning("Error reading order.conf from zip: %s", DP_error());
		outErrors.append(tr("Invalid brush pack: order.conf not found inside"));
		return false;
	}

	QStringList order = orderFile.readUtf8()
		.split(QRegularExpression{"\\s*\n\\s*"}, compat::SkipEmptyParts);
	int orderCount = order.size();
	if(orderCount < 2) {
		outErrors.append(tr("Invalid brush pack: order.conf contains no brushes"));
		return false;
	}

	QRegularExpressionMatch match =
		QRegularExpression{"^Group:\\s*(\\S.+?)\\s*$"}.match(order.first());
	if(!match.hasMatch()) {
		outErrors.append(tr("Invalid brush pack: order.conf does not start with 'Group: ...'"));
		return false;
	}

	outTagName = match.captured(1);
	if(d->readTagCountByName(outTagName) != 0) {
		for(int i = 2; i < 100; ++i) {
			QString candidate = QStringLiteral("%1 (%2)").arg(outTagName).arg(i);
			if(d->readTagCountByName(candidate) == 0) {
				outTagName = candidate;
				break;
			}
		}
	}

	outTagId = -1;
	for(int i = 1; i < orderCount; ++i) {
		QString prefix = order[i];
		ActiveBrush brush;
		QString description;
		QPixmap thumbnail;
		if(readMyPaintBrush(zr, prefix, outErrors, brush, description, thumbnail)) {
			if(outTagId == -1) {
				beginResetModel();
				outTagId = d->createTag(outTagName);
				if(outTagId < 1) {
					outErrors.append(tr("Could not create tag '%1'.").arg(outTagName));
					endResetModel();
					return false;
				}
				m_presetModel->beginResetModel();
			}

			int slashIndex = prefix.indexOf("/");
			QString name = QStringLiteral("%1/%2-%3")
				.arg(outTagName)
				.arg(i * 100, 4, 10, QLatin1Char('0'))
				.arg(slashIndex < 0 ? prefix : prefix.mid(slashIndex + 1));

			int presetId = d->createPreset(
				brush.presetType(), name, description,
				BrushPresetModel::toPng(thumbnail), brush.presetData());
			d->createPresetTag(presetId, outTagId);
		}
	}

	if(outTagId == -1) {
		outErrors.append(tr("Could not read any brushes from brush pack."));
		return false;
	}

	m_presetModel->endResetModel();
	endResetModel();
	return true;
}

bool BrushPresetTagModel::readMyPaintBrush(
	const drawdance::ZipReader &zr, const QString &prefix, QStringList &outErrors,
	ActiveBrush &outBrush, QString &outDescription, QPixmap &outThumbnail)
{
	QString mybPath = QStringLiteral("%1.myb").arg(prefix);
	drawdance::ZipReader::File mybFile = zr.readFile(mybPath);
	if(mybFile.isNull()) {
		qWarning("Error reading '%s' from zip: %s", qUtf8Printable(mybPath), DP_error());
		outErrors.append(tr("Can't read brush file '%1'").arg(qUtf8Printable(mybPath)));
		return false;
	}

	QJsonParseError error;
	QJsonDocument json = QJsonDocument::fromJson(mybFile.readBytes(), &error);
	if(error.error != QJsonParseError::NoError) {
		outErrors.append(tr("Brush file '%1' does not contain valid JSON: %1")
			.arg(qUtf8Printable(mybPath)).arg(qUtf8Printable(error.errorString())));
		return false;
	}

	outBrush = ActiveBrush{ActiveBrush::MYPAINT};
	QJsonObject object = json.object();
	if(!outBrush.myPaint().loadMyPaintJson(object)) {
		outErrors.append(tr("Can't load brush from brush file '%1'")
			.arg(qUtf8Printable(mybPath)));
		return false;
	}

	outDescription = object["notes"].toString();
	if (outDescription.isNull()) {
		outDescription = object["description"].toString();
		if (outDescription.isNull()) {
			outDescription = "";
		}
	}

	QString prevPath = QStringLiteral("%1_prev.png").arg(prefix);
	drawdance::ZipReader::File prevFile = zr.readFile(prevPath);
	if(!prevFile.isNull()) {
		outThumbnail.loadFromData(prevFile.readBytes());
	}

	if(outThumbnail.isNull()) {
		outThumbnail = outBrush.presetThumbnail();
	}

	if(outThumbnail.width() != THUMBNAIL_SIZE || outThumbnail.height() != THUMBNAIL_SIZE) {
		outThumbnail = outThumbnail.scaled(THUMBNAIL_SIZE, THUMBNAIL_SIZE,
			Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}

	return true;
}


BrushPresetModel::BrushPresetModel(BrushPresetTagModel *tagModel)
	: QAbstractItemModel(tagModel)
	, d(tagModel->d)
	, m_tagIdToFilter(-1)
{}

BrushPresetModel::~BrushPresetModel() {}

int BrushPresetModel::rowCount(const QModelIndex &) const
{
	switch(m_tagIdToFilter) {
	case ALL_ID:
		return d->readPresetCountAll();
	case UNTAGGED_ID:
		return d->readPresetCountByUntagged();
	default:
		return d->readPresetCountByTagId(m_tagIdToFilter);
	}
}

int BrushPresetModel::columnCount(const QModelIndex &) const
{
	return 1;
}

QModelIndex BrushPresetModel::parent(const QModelIndex &) const
{
	return QModelIndex();
}

QModelIndex BrushPresetModel::index(int row, int column, const QModelIndex &) const
{
	int presetId;
	switch(m_tagIdToFilter) {
	case ALL_ID:
		presetId = d->readPresetIdAtIndexAll(row);
		break;
	case UNTAGGED_ID:
		presetId = d->readPresetIdAtIndexByUntagged(row);
		break;
	default:
		presetId = d->readPresetIdAtIndexByTagId(row, m_tagIdToFilter);
		break;
	}
	if(presetId > 0) {
		return createIndex(row, column, quintptr(presetId));
	} else {
		return QModelIndex();
	}
}

QVariant BrushPresetModel::data(const QModelIndex &index, int role) const
{
	switch(role) {
	case Qt::ToolTipRole:
	case FilterRole:
		return d->readPresetNameById(index.internalId());
	case Qt::DecorationRole: {
		QPixmap pixmap;
		if(pixmap.loadFromData(d->readPresetThumbnailById(index.internalId()))) {
			return pixmap.scaled(iconSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		} else {
			return QPixmap();
		}
	}
	case Qt::SizeHintRole: {
		return iconSize() + QSize(7, 7);
	}
	case BrushRole: {
		QByteArray data = d->readPresetDataById(index.internalId());
		ActiveBrush brush = ActiveBrush::fromJson(QJsonDocument::fromJson(data).object());
		return QVariant::fromValue(brush);
	}
	default:
		break;
	}
	return QVariant();
}

void BrushPresetModel::setTagIdToFilter(int tagId)
{
	beginResetModel();
	m_tagIdToFilter = tagId;
	endResetModel();
}

QList<TagAssignment> BrushPresetModel::getTagAssignments(int presetId)
{
	return d->readTagAssignmentsByPresetId(presetId);
}

PresetMetadata BrushPresetModel::getPresetMetadata(int presetId)
{
	return d->readPresetMetadataById(presetId);
}

bool BrushPresetModel::changeTagAssignment(int presetId, int tagId, bool assigned)
{
	beginResetModel();
	bool ok;
	if(assigned) {
		ok = d->createPresetTag(presetId, tagId);
	} else {
		ok =  d->deletePresetTag(presetId, tagId);
	}
	endResetModel();
	return ok;
}

int BrushPresetModel::newPreset(const QString &type, const QString &name, const QString description,
	const QPixmap &thumbnail, const QByteArray &data)
{
	beginResetModel();
	int presetId = d->createPreset(type, name, description, toPng(thumbnail), data);
	endResetModel();
	return presetId;
}

int BrushPresetModel::duplicatePreset(int presetId)
{
	beginResetModel();
	int newPresetId = d->createPresetFromId(presetId);
	if(newPresetId > 0) {
		d->createPresetTagsFromPresetId(newPresetId, presetId);
	}
	endResetModel();
	return presetId;
}

bool BrushPresetModel::updatePresetData(int presetId, const QString &type, const QByteArray &data)
{
	return d->updatePresetData(presetId, type, data);
}

bool BrushPresetModel::updatePresetMetadata(int presetId, const QString &name,
	const QString &description, const QPixmap &thumbnail)
{
	beginResetModel();
	bool ok = d->updatePresetMetadata(presetId, name, description, toPng(thumbnail));
	endResetModel();
	return ok;
}

bool BrushPresetModel::deletePreset(int presetId)
{
	beginResetModel();
	bool ok = d->deletePresetById(presetId);
	endResetModel();
	return ok;
}

QSize BrushPresetModel::iconSize() const
{
	int dimension = iconDimension();
	return QSize(dimension, dimension);
}

int BrushPresetModel::iconDimension() const
{
	int dimension = d->readState(QStringLiteral("preset_icon_size")).toInt();
	return dimension <= 0 ? THUMBNAIL_SIZE : dimension;
}

void BrushPresetModel::setIconDimension(int dimension)
{
	beginResetModel();
	d->createOrUpdateState(QStringLiteral("preset_icon_size"), dimension);
	endResetModel();
}

void BrushPresetModel::tagsAboutToBeReset()
{
	beginResetModel();
}

void BrushPresetModel::tagsReset()
{
	endResetModel();
}

QPixmap BrushPresetModel::loadBrushPreview(const QFileInfo &fileInfo)
{
	QString file = fileInfo.path() + QDir::separator() + fileInfo.completeBaseName() + "_prev.png";
	QPixmap pixmap;
	if(pixmap.load(file)) {
		return pixmap;
	} else {
		return QPixmap();
	}
}

QByteArray BrushPresetModel::toPng(const QPixmap &pixmap)
{
	QByteArray bytes;
	QBuffer buffer(&bytes);
	buffer.open(QIODevice::WriteOnly);
	pixmap.save(&buffer, "PNG");
	buffer.close();
	return bytes;
}

}
