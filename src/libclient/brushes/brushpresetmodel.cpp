// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/brushes/brushpresetmodel.h"
#include "libclient/brushes/brush.h"
#include "libshared/util/database.h"
#include "libshared/util/paths.h"
#include "libshared/util/qtcompat.h"
#include "libclient/drawdance/ziparchive.h"

#include <QBuffer>
#include <QDebug>
#include <QDirIterator>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
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
		: m_db{utils::db::sqlite(
			QStringLiteral("drawpile_brush_preset_connection"),
			QStringLiteral("brush presets"), QStringLiteral("brushes.db"),
			QStringLiteral("initialbrushpresets.db"))}
	{
		initDb();
	}

	int createTag(const QString &name)
	{
		QMutexLocker locker{&m_mutex};
		QSqlQuery query(m_db);
		if(exec(query, "insert into tag (name) values (?)", {name})) {
			return query.lastInsertId().toInt();
		} else {
			return 0;
		}
	}

	int readTagCount()
	{
		QMutexLocker locker{&m_mutex};
		return readInt("select count(*) from tag");
	}

	int readTagCountByName(const QString &name)
	{
		QMutexLocker locker{&m_mutex};
		return readInt("select count(*) from tag where name = ?", {name});
	}

	int readTagIdAtIndex(int index)
	{
		QMutexLocker locker{&m_mutex};
		return readInt("select id from tag order by LOWER(name) limit 1 offset ?", {index});
	}

	Tag readTagAtIndex(int index)
	{
		QMutexLocker locker{&m_mutex};
		QSqlQuery query(m_db);
		QString sql = QStringLiteral(
			"select id, name from tag order by LOWER(name) limit 1 offset ?");
		if(exec(query, sql, {index}) && query.next()) {
			return Tag{query.value(0).toInt(), query.value(1).toString(), true};
		} else {
			return Tag{0, "", false};
		}
	}

	QString readTagNameById(int id)
	{
		QMutexLocker locker{&m_mutex};
		return readString("select name from tag where id = ?", {id});
	}

	int readTagIndexById(int id)
	{
		QMutexLocker locker{&m_mutex};
		QString sql = QStringLiteral(
			"select n - 1 from (\n"
			"	select row_number() over(order by LOWER(name)) as n, id from tag)\n"
			"where id = ?");
		return readInt(sql, {id}, -1);
	}

	bool updateTagName(int id, const QString &name)
	{
		QMutexLocker locker{&m_mutex};
		QSqlQuery query(m_db);
		if(exec(query, "update tag set name = ? where id = ?", {name, id})) {
			return query.numRowsAffected() == 1;
		} else {
			return false;
		}
	}

	bool deleteTagById(int id)
	{
		QMutexLocker locker{&m_mutex};
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
		QMutexLocker locker{&m_mutex};
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
		QMutexLocker locker{&m_mutex};
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
		QMutexLocker locker{&m_mutex};
		return readInt("select count(*) from preset");
	}

	int readPresetCountByUntagged()
	{
		QMutexLocker locker{&m_mutex};
		QString sql = QStringLiteral(
			"select count(*) from preset p where not exists(\n"
			"	select 1 from preset_tag pt where pt.preset_id = p.id)");
		return readInt(sql);
	}

	int readPresetCountByTagId(int tagId)
	{
		QMutexLocker locker{&m_mutex};
		QString sql = QStringLiteral(
			"select count(*) from preset p\n"
			"	join preset_tag pt on pt.preset_id = p.id\n"
			"	where pt.tag_id = ?");
		return readInt(sql, {tagId});
	}

	int readPresetIdAtIndexAll(int index)
	{
		QMutexLocker locker{&m_mutex};
		return readInt("select id from preset order by LOWER(name) limit 1 offset ?", {index});
	}

	int readPresetIdAtIndexByUntagged(int index)
	{
		QMutexLocker locker{&m_mutex};
		QString sql = QStringLiteral(
			"select p.id from preset p\n"
			"	where not exists(select 1 from preset_tag pt where pt.preset_id = p.id)\n"
			"	order by LOWER(p.name) limit 1 offset ?");
		return readInt(sql, {index});
	}

	int readPresetIdAtIndexByTagId(int index, int tagId)
	{
		QMutexLocker locker{&m_mutex};
		QString sql = QStringLiteral(
			"select p.id from preset p\n"
			"	join preset_tag pt on pt.preset_id = p.id\n"
			"	where pt.tag_id = ?"
			"	order by LOWER(p.name) limit 1 offset ?");
		return readInt(sql, {tagId, index});
	}

	QString readPresetNameById(int id)
	{
		QMutexLocker locker{&m_mutex};
		return readString("select name from preset where id = ?", {id});
	}

	QByteArray readPresetThumbnailById(int id)
	{
		QMutexLocker locker{&m_mutex};
		return readByteArray("select thumbnail from preset where id = ?", {id});
	}

	QByteArray readPresetDataById(int id)
	{
		QMutexLocker locker{&m_mutex};
		return readByteArray("select data from preset where id = ?", {id});
	}

	PresetMetadata readPresetMetadataById(int id)
	{
		QMutexLocker locker{&m_mutex};
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
		QMutexLocker locker{&m_mutex};
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
		QMutexLocker locker{&m_mutex};
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
		QMutexLocker locker{&m_mutex};
		QSqlQuery query(m_db);
		if(exec(query, "delete from preset where id = ?", {id})) {
			return query.numRowsAffected() == 1;
		} else {
			return false;
		}
	}

	QList<TagAssignment> readTagAssignmentsByPresetId(int presetId)
	{
		QMutexLocker locker{&m_mutex};
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
		QMutexLocker locker{&m_mutex};
		QSqlQuery query(m_db);
		QString sql = QStringLiteral(
			"insert into preset_tag (preset_id, tag_id) values (?, ?)");
		return exec(query, sql, {presetId, tagId});
	}

	bool createPresetTagsFromPresetId(int targetPresetId, int sourcePresetId)
	{
		QMutexLocker locker{&m_mutex};
		QSqlQuery query(m_db);
		QString sql = QStringLiteral(
			"insert into preset_tag (preset_id, tag_id)\n"
			"	select ?, tag_id from preset_tag where preset_id = ?");
		return exec(query, sql, {targetPresetId, sourcePresetId});
	}

	bool deletePresetTag(int presetId, int tagId)
	{
		QMutexLocker locker{&m_mutex};
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
		QMutexLocker locker{&m_mutex};
		QSqlQuery query(m_db);
		QString sql = QStringLiteral(
			"insert into state (key, value) values (?, ?)\n"
			"	on conflict (key) do update set value = ?");
		return exec(query, sql, {key, value, value});
	}

	QVariant readState(const QString &key)
	{
		QMutexLocker locker{&m_mutex};
		QSqlQuery query(m_db);
		if(exec(query, "select value from state where key = ?", {key}) && query.next()) {
			return query.value(0);
		} else {
			return QVariant();
		}
	}

private:
	QMutex m_mutex;
	QSqlDatabase m_db;

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
		return utils::db::exec(query, sql, params);
	}

	void initDb()
	{
		QSqlQuery query(m_db);
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

int BrushPresetTagModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : d->readTagCount() + TAG_OFFSET;
}

int BrushPresetTagModel::columnCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : 1;
}

QModelIndex BrushPresetTagModel::parent(const QModelIndex &) const
{
	return QModelIndex();
}

QModelIndex BrushPresetTagModel::index(int row, int column, const QModelIndex &parent) const
{
	if(parent.isValid()) {
		return QModelIndex();
	} else if(isBuiltInTag(row)) {
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
			return QIcon::fromTheme("folder");
		case UNTAGGED_ROW:
			return QIcon::fromTheme("folder-new");
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

bool BrushPresetTagModel::isExportableRow(int row)
{
	return row != ALL_ROW;
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

BrushImportResult BrushPresetTagModel::importBrushPack(const QString &file)
{
	BrushImportResult result = {{}, {}, 0};

	drawdance::ZipReader zr{file};
	if(zr.isNull()) {
		qWarning("Error opening '%s': %s", qUtf8Printable(file), DP_error());
		result.errors.append(tr("Can't open '%1'.").arg(qUtf8Printable(file)));
		return result;
	}

	beginResetModel();
	m_presetModel->beginResetModel();

	QVector<ImportBrushGroup> groups = readOrderConf(result, file, zr);
	for(const ImportBrushGroup &group : groups) {
		if(!group.brushes.isEmpty()) {
			readImportBrushes(result, zr, group.name, group.brushes);
		}
	}

	m_presetModel->endResetModel();
	endResetModel();
	return result;
}

QVector<BrushPresetTagModel::ImportBrushGroup>
BrushPresetTagModel::readOrderConf(
	BrushImportResult &result, const QString &file, const drawdance::ZipReader &zr)
{
	static QRegularExpression newlineRe{"[\r\n]+"};
	static QRegularExpression groupRe{"^Group:\\s*(.+)$"};

	drawdance::ZipReader::File orderFile =
		zr.readFile(QStringLiteral("order.conf"));
	if(orderFile.isNull()) {
		qWarning("Error reading order.conf from zip: %s", DP_error());
		result.errors.append(tr("Invalid brush pack: order.conf not found inside"));
		return {};
	}

	QFileInfo info{file};
	QVector<BrushPresetTagModel::ImportBrushGroup> groups;
	groups.append({tr("Uncategorized %1").arg(info.baseName()), {}});

	int current = 0;
	bool haveBrush = false;
	QStringList order =
		orderFile.readUtf8().split(newlineRe, compat::SkipEmptyParts);
	for(const QString &line : order) {
		QString trimmed = line.trimmed();
		if(!trimmed.isEmpty() && !trimmed.startsWith("#")) {
			QRegularExpressionMatch match = groupRe.match(trimmed);
			if(match.hasMatch()) {
				current = addOrderConfGroup(groups, match.captured(1));
			} else {
				haveBrush = true;
				addOrderConfBrush(groups[current].brushes, trimmed);
			}
		}
	}

	if(haveBrush) {
		return groups;
	} else {
		result.errors.append(tr("Invalid brush pack: order.conf contains no brushes"));
		return {};
	}
}

int BrushPresetTagModel::addOrderConfGroup(
	QVector<ImportBrushGroup> &groups, const QString &name)
{
	int count = groups.size();
	for(int i = 0; i < count; ++i) {
		if(groups[i].name == name) {
			return i;
		}
	}
	groups.append({name, {}});
	return count;
}

void BrushPresetTagModel::addOrderConfBrush(
	QStringList &brushes, const QString &brush)
{
	if(!brushes.contains(brush)) {
		brushes.append(brush);
	}
}

void BrushPresetTagModel::readImportBrushes(
	BrushImportResult &result, const drawdance::ZipReader &zr,
	const QString &groupName, const QStringList &brushes)
{
	QString tagName = groupName;
	if(d->readTagCountByName(groupName) != 0) {
		for(int i = 1; i < 100; ++i) {
			QString candidate =
				QStringLiteral("%1 (%2)").arg(groupName).arg(i);
			if(d->readTagCountByName(candidate) == 0) {
				tagName = candidate;
				break;
			}
		}
	}

	int tagId = -1;
	int count = brushes.size();
	for(int i = 0; i < count; ++i) {
		const QString &prefix = brushes[i];
		ActiveBrush brush;
		QString description;
		QPixmap thumbnail;
		if(readImportBrush(
			   result, zr, prefix, brush, description, thumbnail)) {
			if(tagId == -1) {
				tagId = d->createTag(tagName);
				if(tagId < 1) {
					result.errors.append(
						tr("Could not create tag '%1'.").arg(tagName));
					break;
				}
				result.importedTags.append({tagId, tagName, true});
			}

			int slashIndex = prefix.indexOf("/");
			QString name =
				QStringLiteral("%1/%2-%3")
					.arg(tagName)
					.arg(i * 100, 4, 10, QLatin1Char('0'))
					.arg(slashIndex < 0 ? prefix : prefix.mid(slashIndex + 1));

			int presetId = d->createPreset(
				brush.presetType(), name, description,
				BrushPresetModel::toPng(thumbnail), brush.presetData());
			if(presetId < 1) {
				result.errors.append(
					tr("Could not create brush preset '%1'.").arg(name));
			}

			++result.importedBrushCount;

			if(!d->createPresetTag(presetId, tagId)) {
				result.errors.append(
					tr("Could not assign brush '%1' to tag '%2'.").arg(name).arg(tagName));
			}
		}
	}
}

bool BrushPresetTagModel::readImportBrush(
	BrushImportResult &result, const drawdance::ZipReader &zr,
	const QString &prefix, ActiveBrush &outBrush, QString &outDescription,
	QPixmap &outThumbnail)
{
	QString mybPath = QStringLiteral("%1.myb").arg(prefix);
	drawdance::ZipReader::File mybFile = zr.readFile(mybPath);
	if(mybFile.isNull()) {
		qWarning(
			"Error reading '%s' from zip: %s", qUtf8Printable(mybPath),
			DP_error());
		result.errors.append(
			tr("Can't read brush file '%1'").arg(qUtf8Printable(mybPath)));
		return false;
	}

	QJsonParseError error;
	QJsonDocument json = QJsonDocument::fromJson(mybFile.readBytes(), &error);
	if(error.error != QJsonParseError::NoError) {
		result.errors.append(tr("Brush file '%1' does not contain valid JSON: %1")
							 .arg(qUtf8Printable(mybPath))
							 .arg(qUtf8Printable(error.errorString())));
		return false;
	}

	QJsonObject object = json.object();
	if(!outBrush.fromExportJson(object)) {
		result.errors.append(tr("Can't load brush from brush file '%1'")
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


BrushExportResult BrushPresetTagModel::exportBrushPack(
	const QString &file, const QVector<BrushExportTag> &exportTags) const
{
	BrushExportResult result = {false, {}, 0};

	drawdance::ZipWriter zw{file};
	if(zw.isNull()) {
		qWarning("Error opening '%s': %s", qUtf8Printable(file), DP_error());
		result.errors.append(tr("Can't open '%1'.").arg(qUtf8Printable(file)));
		return result;
	}

	QStringList order;
	for(const BrushExportTag &tag : exportTags) {
		exportTag(result, order, zw, tag);
	}

	if(result.exportedBrushCount == 0) {
		result.errors.append(tr("No brushes exported"));
		return result;
	}

	if(!zw.addFile(QStringLiteral("order.conf"), order.join("\n").toUtf8())) {
		qWarning("Error exporting order.conf: %s", DP_error());
		result.errors.append(tr("Can't export order.conf"));
		return result;
	}

	if(zw.finish()) {
		result.ok = true;
	} else {
		result.errors.append(
			tr("Error writing '%1': %2").arg(file).arg(DP_error()));
	}
	return result;
}

void BrushPresetTagModel::exportTag(
	BrushExportResult &result, QStringList &order, drawdance::ZipWriter &zw,
	const BrushExportTag &tag) const
{
	QString tagPath = makeExportSafe(tag.name);
	if(!zw.addDir(tagPath)) {
		qWarning("Error creating tag directory '%s': %s", qUtf8Printable(tagPath), DP_error());
		result.errors.append(tr("Can't export tag '%1'").arg(tag.name));
		return;
	}

	order.append(QStringLiteral("Group: %1").arg(tag.name));
	for(int presetId : tag.presetIds) {
		exportPreset(result, order, zw, tagPath, presetId);
	}
	order.append(QString{});
}

void BrushPresetTagModel::exportPreset(
	BrushExportResult &result, QStringList &order, drawdance::ZipWriter &zw,
	const QString &tagPath, int presetId) const
{
	QByteArray data = d->readPresetDataById(presetId);
	PresetMetadata preset = d->readPresetMetadataById(presetId);
	if(data.isEmpty() || preset.id != presetId) {
		qWarning("Preset %d not found", presetId);
		result.errors.append(tr("Missing preset %1").arg(presetId));
		return;
	}

	QJsonDocument doc = QJsonDocument::fromJson(data);
	if(!doc.isObject()) {
		qWarning("Invalid preset data %d", presetId);
		result.errors.append(tr("Invalid preset %1").arg(presetId));
		return;
	}

	ActiveBrush brush = ActiveBrush::fromJson(doc.object());
	QByteArray exportData = brush.toExportJson(preset.description);

	QString presetName = getExportName(presetId, preset.name);
	QString presetPath = QStringLiteral("%1/%2").arg(tagPath, presetName);

	QString dataPath = QStringLiteral("%1.myb").arg(presetPath);
	if(!zw.addFile(dataPath, exportData)) {
		qWarning("Error exporting preset '%s': %s", qUtf8Printable(dataPath), DP_error());
		result.errors.append(tr("Can't export preset '%1'").arg(preset.name));
		return;
	}

	QString thumbnailPath = QStringLiteral("%1_prev.png").arg(presetPath);
	if(!zw.addFile(thumbnailPath, preset.thumbnail)) {
		qWarning("Error exporting preset thumbnail '%s': %s", qUtf8Printable(thumbnailPath), DP_error());
		result.errors.append(tr("Can't export preset thumbnail '%1'").arg(preset.name));
		return;
	}

	order.append(presetPath);
	++result.exportedBrushCount;
}

QString BrushPresetTagModel::getExportName(
	int presetId, const QString presetName)
{
	static QRegularExpression baseNameRe{"(?:[0-9]+-)?([^/\\\\]+)$"};
	QRegularExpressionMatch match = baseNameRe.match(presetName);
	if(match.hasMatch()) {
		return makeExportSafe(match.captured(1));
	} else {
		return QStringLiteral("Brush%1").arg(presetId);
	}
}

QString BrushPresetTagModel::makeExportSafe(const QString &s)
{
	static QRegularExpression safeRe{"[^a-zA-Z0-9_ -]"};
	QString t = s.trimmed();
	t.replace(safeRe, QStringLiteral("_"));
	return t;
}


BrushPresetModel::BrushPresetModel(BrushPresetTagModel *tagModel)
	: QAbstractItemModel(tagModel)
	, d(tagModel->d)
	, m_tagIdToFilter(-1)
{}

BrushPresetModel::~BrushPresetModel() {}

int BrushPresetModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid()) {
		return 0;
	} else {
		return rowCountForTagId(m_tagIdToFilter);
	}
}

int BrushPresetModel::rowCountForTagId(int tagId) const
{
	switch(tagId) {
	case ALL_ID:
		return d->readPresetCountAll();
	case UNTAGGED_ID:
		return d->readPresetCountByUntagged();
	default:
		return d->readPresetCountByTagId(tagId);
	}
}

int BrushPresetModel::columnCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : 1;
}

QModelIndex BrushPresetModel::parent(const QModelIndex &) const
{
	return QModelIndex();
}

QModelIndex BrushPresetModel::index(int row, int column, const QModelIndex &parent) const
{
	if(parent.isValid() || column != 0) {
		return QModelIndex();
	} else {
		return indexForTagId(m_tagIdToFilter, row);
	}
}

QModelIndex BrushPresetModel::indexForTagId(int tagId, int row) const
{
	int presetId;
	switch(tagId) {
	case ALL_ID:
		presetId = d->readPresetIdAtIndexAll(row);
		break;
	case UNTAGGED_ID:
		presetId = d->readPresetIdAtIndexByUntagged(row);
		break;
	default:
		presetId = d->readPresetIdAtIndexByTagId(row, tagId);
		break;
	}
	if(presetId > 0) {
		return createIndex(row, 0, quintptr(presetId));
	} else {
		return QModelIndex();
	}
}

QVariant BrushPresetModel::data(const QModelIndex &index, int role) const
{
	switch(role) {
	case Qt::ToolTipRole:
	case FilterRole:
	case TitleRole:
		return d->readPresetNameById(index.internalId());
	case ThumbnailRole: {
		QPixmap pixmap;
		if(pixmap.loadFromData(d->readPresetThumbnailById(index.internalId()))) {
			return pixmap.scaled(iconSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		} else {
			return QPixmap();
		}
	}
	case Qt::SizeHintRole:
		return iconSize();
	case BrushRole: {
		QByteArray data = d->readPresetDataById(index.internalId());
		ActiveBrush brush = ActiveBrush::fromJson(QJsonDocument::fromJson(data).object());
		return QVariant::fromValue(brush);
	}
	case IdRole:
		return int(index.internalId());
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
