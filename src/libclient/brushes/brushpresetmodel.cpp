// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/brushes/brushpresetmodel.h"
#include "libclient/drawdance/ziparchive.h"
#include "libclient/utils/wasmpersistence.h"
#include "libshared/util/database.h"
#include "libshared/util/paths.h"
#include "libshared/util/qtcompat.h"
#include <QBuffer>
#include <QDirIterator>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMutexLocker>
#include <QRecursiveMutex>
#include <QRegularExpression>
#include <QTimer>
#include <QVector>
#include <algorithm>
#include <cctype>
#include <dpcommon/platform_qt.h>

namespace brushes {

static constexpr int ALL_ROW = 0;
static constexpr int UNTAGGED_ROW = 1;
static constexpr int TAG_OFFSET = 2;
static constexpr int ALL_ID = -1;
static constexpr int UNTAGGED_ID = -2;

namespace {
typedef QVector<QPair<QString, QStringList>> OldMetadata;

struct OldBrushPreset {
	ClassicBrush brush;
	QString filename;
};

struct OldPresetFolder {
	QString title;
	QVector<OldBrushPreset> presets;
};

struct CachedTag {
	int id;
	QString name;
};

struct CachedPreset : Preset {
	QSet<int> tagIds;
};

struct PresetChange {
	std::optional<QString> name;
	std::optional<QString> description;
	std::optional<QPixmap> thumbnail;
	std::optional<ActiveBrush> brush;
};

struct PresetShortcutEntry {
	int id;
	QString name;

	bool operator==(const PresetShortcutEntry &other) const
	{
		return id == other.id && name == other.name;
	}

	bool operator<(const PresetShortcutEntry &other) const
	{
		return name.compare(other.name, Qt::CaseInsensitive) < 0;
	}
};

struct PresetShortcut {
	QVector<PresetShortcutEntry> entries;

	bool containsId(int id) const
	{
		for(const PresetShortcutEntry &entry : entries) {
			if(entry.id == id) {
				return true;
			}
		}
		return false;
	}

	QVector<int> ids() const
	{
		QVector<int> v;
		v.reserve(entries.size());
		for(const PresetShortcutEntry &entry : entries) {
			v.append(entry.id);
		}
		return v;
	}

	QStringList names() const
	{
		QStringList v;
		v.reserve(entries.size());
		for(const PresetShortcutEntry &entry : entries) {
			v.append(entry.name);
		}
		return v;
	}

	QString text(const QLocale &locale) const
	{
		return locale.createSeparatedList(names());
	}
};

using PresetShortcutMap = QHash<QKeySequence, PresetShortcut>;
}

class BrushPresetTagModel::Private {
public:
	Private()
	{
		initDb();
		m_presetChangeTimer.setTimerType(Qt::VeryCoarseTimer);
		m_presetChangeTimer.setSingleShot(true);
		m_presetChangeTimer.setInterval(4000);
		connect(
			&m_presetChangeTimer, &QTimer::timeout,
			std::bind(&Private::writePresetChanges, this));
	}

	void setPresetModel(BrushPresetModel *presetModel)
	{
		Q_ASSERT(!m_presetModel);
		m_presetModel = presetModel;
	}

	BrushPresetModel *presetModel() const { return m_presetModel; }

	int createTag(const QString &name)
	{
		DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
		drawdance::Query query = db.query();
		if(query.exec("insert into tag (name) values (?)", {name})) {
			return query.lastInsertId();
		} else {
			return 0;
		}
	}

	int readTagCount() { return db.readInt("select count(*) from tag"); }

	int readTagCountByName(const QString &name)
	{
		return db.readInt("select count(*) from tag where name = ?", 0, {name});
	}

	int readTagIdAtIndex(int index)
	{
		return db.readInt(
			"select id from tag order by LOWER(name) limit 1 offset ?", 0,
			{index});
	}

	Tag readTagAtIndex(int index)
	{
		drawdance::Query query = db.query();
		const char *sql =
			"select id, name from tag order by LOWER(name) limit 1 offset ?";
		if(query.exec(sql, {index}) && query.next()) {
			return Tag{query.columnInt(0), query.columnText16(1)};
		} else {
			return Tag{0, QString()};
		}
	}

	QString readTagNameById(int id)
	{
		return db.readText16("select name from tag where id = ?", {id});
	}

	int readTagIndexById(int id)
	{
		const char *sql =
			"select n - 1 from (select row_number() over("
			"order by LOWER(name)) as n, id from tag) where id = ?";
		return db.readInt(sql, -1, {id});
	}

	bool updateTagName(int id, const QString &name)
	{
		DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
		drawdance::Query query = db.query();
		if(query.exec("update tag set name = ? where id = ?", {name, id})) {
			return query.numRowsAffected() == 1;
		} else {
			return false;
		}
	}

	bool deleteTagById(int id)
	{
		DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
		drawdance::Query query = db.query();
		if(query.exec("delete from tag where id = ?", {id})) {
			return query.numRowsAffected() == 1;
		} else {
			return false;
		}
	}

	int createPreset(
		const QString &name, const QString &description,
		const QByteArray &thumbnail, const QString &type,
		const QByteArray &data)
	{
		DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
		drawdance::Query query = db.query();
		const char *sql =
			"insert into preset (name, description, thumbnail, type, data) "
			"values (?, ?, ?, ?, ?)";
		if(query.exec(sql, {name, description, thumbnail, type, data})) {
			return query.lastInsertId();
		} else {
			return 0;
		}
	}

	int readPresetCountAll()
	{
		return db.readInt("select count(*) from preset");
	}

	int readPresetCountByUntagged()
	{
		return db.readInt(
			"select count(*) from preset p where not exists("
			"select 1 from preset_tag pt where pt.preset_id = p.id)");
	}

	int readPresetCountByTagId(int tagId)
	{
		return db.readInt(
			"select count(*) from preset p "
			"join preset_tag pt on pt.preset_id = p.id "
			"where pt.tag_id = ?",
			0, {tagId});
	}

	int readPresetCountByName(const QString &name)
	{
		return db.readInt(
			"select count(*) from preset where name = ?", 0, {name});
	}

	int readPresetIdAtIndexAll(int index)
	{
		return db.readInt(
			"select id from preset order by LOWER(name) limit 1 offset ?", 0,
			{index});
	}

	int readPresetIdAtIndexByUntagged(int index)
	{
		return db.readInt(
			"select p.id from preset p where not exists("
			"select 1 from preset_tag pt where pt.preset_id = p.id) "
			"order by LOWER(p.name) limit 1 offset ?",
			0, {index});
	}

	int readPresetIdAtIndexByTagId(int index, int tagId)
	{
		return db.readInt(
			"select p.id from preset p "
			"join preset_tag pt on pt.preset_id = p.id "
			"where pt.tag_id = ? order by LOWER(p.name) limit 1 offset ?",
			0, {tagId, index});
	}

	QString readPresetEffectiveNameById(int id)
	{
		return db.readText16(
			"select coalesce(changed_name, name) from preset where id = ?",
			{id});
	}

	QString readPresetEffectiveDescriptionById(int id)
	{
		return db.readText16(
			"select coalesce(changed_description, description) "
			"from preset where id = ?",
			{id});
	}

	QByteArray readPresetEffectiveThumbnailById(int id)
	{
		return db.readBlob(
			"select coalesce(changed_thumbnail, thumbnail) from "
			"preset where id = ?",
			{id});
	}

	QByteArray readPresetDataById(int id)
	{
		return db.readBlob("select data from preset where id = ?", {id});
	}

	bool readPresetHasChangesById(int id)
	{
		return db.readInt(
			"select changed_name is not null or changed_description is not "
			"null or changed_thumbnail is not null or changed_data is not "
			"null as has_changes from preset where id = ?",
			0, {id});
	}

	std::optional<Preset> readPresetById(int id)
	{
		drawdance::Query query = db.query();
		const char *sql =
			"select id, name, description, thumbnail, data, changed_name, "
			"changed_description, changed_thumbnail, changed_data from preset "
			"where id = ?";
		if(query.exec(sql, {id}) && query.next()) {
			Preset preset;
			readPreset(preset, query);
			return preset;
		} else {
			return {};
		}
	}

	bool readPresetBrushDataById(
		int id, QString &outName, QPixmap &outThumbnail, ActiveBrush &outBrush)
	{
		drawdance::Query query = db.query();
		const char *sql =
			"select name, thumbnail, data from preset where id = ?";
		if(query.exec(sql, {id}) && query.next()) {
			outName = query.columnText16(0);
			outThumbnail.loadFromData(query.columnBlob(1));
			outBrush = loadBrush(id, query.columnBlob(2));
			return true;
		} else {
			return false;
		}
	}

	bool updatePreset(
		int id, const QString &name, const QString &description,
		const QByteArray &thumbnail, const QString &type,
		const QByteArray &data)
	{
		DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
		drawdance::Query query = db.query();
		if(query.exec(
			   "update preset set name = ?, description = ?, thumbnail = ?, "
			   "type = ?, data = ?, changed_name = null, "
			   "changed_description = null, changed_thumbnail = null, "
			   "changed_type = null, changed_data = null where id = ?",
			   {name, description, thumbnail, type, data, id})) {
			return query.numRowsAffected() == 1;
		} else {
			return false;
		}
	}

	bool updatePresetBrush(int id, const QString &type, const QByteArray &data)
	{
		DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
		drawdance::Query query = db.query();
		if(query.exec(
			   "update preset set type = ?, data = ?, changed_type = null, "
			   "changed_data = null where id = ?",
			   {type, data, id})) {
			return query.numRowsAffected() == 1;
		} else {
			return false;
		}
	}

	bool updatePresetShortcut(int id, const QString &shortcut)
	{
		DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
		drawdance::Query query = db.query();
		if(query.exec(
			   "update preset set shortcut = ? where id = ?", {shortcut, id})) {
			return query.numRowsAffected() == 1;
		} else {
			return false;
		}
	}

	bool resetAllPresetChanges()
	{
		DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
		drawdance::Query query = db.query();
		return query.exec("update preset set changed_description = null, "
						  "changed_thumbnail = null, changed_type = null, "
						  "changed_data = null");
	}

	bool deletePresetById(int id)
	{
		drawdance::Query query = db.query();
		if(query.exec("delete from preset where id = ?", {id})) {
			return query.numRowsAffected() == 1;
		} else {
			return false;
		}
	}

	QList<TagAssignment> readTagAssignmentsByPresetId(int presetId)
	{
		QList<TagAssignment> tagAssignments;
		drawdance::Query query = db.query();
		const char *sql =
			"select t.id, t.name, pt.preset_id is not null "
			"from tag t left outer join preset_tag pt "
			"on pt.tag_id = t.id and pt.preset_id = ? order by t.id";
		if(query.exec(sql, {presetId})) {
			while(query.next()) {
				tagAssignments.append(TagAssignment{
					query.columnInt(0), query.columnText16(1),
					query.columnBool(2)});
			}
		}
		return tagAssignments;
	}

	bool createPresetTag(int presetId, int tagId)
	{
		DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
		drawdance::Query query = db.query();
		const char *sql =
			"insert into preset_tag (preset_id, tag_id) values (?, ?)";
		return query.exec(sql, {presetId, tagId});
	}

	bool deletePresetTag(int presetId, int tagId)
	{
		DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
		drawdance::Query query = db.query();
		const char *sql =
			"delete from preset_tag where preset_id = ? and tag_id = ?";
		if(query.exec(sql, {presetId, tagId})) {
			return query.numRowsAffected() == 1;
		} else {
			return false;
		}
	}

	bool createOrUpdateState(
		const QString &key, const drawdance::Query::Param &value)
	{
		DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
		drawdance::Query query = db.query();
		return createOrUpdateStateInternal(query, key, value);
	}

	bool readStateBool(const QString &key, bool defaultValue)
	{
		drawdance::Query query = db.query();
		return readStateBoolInternal(query, key, defaultValue);
	}

	int readStateInt(const QString &key, int defaultValue)
	{
		drawdance::Query query = db.query();
		return readStateIntInternal(query, key, defaultValue);
	}


	void refreshTagCache()
	{
		drawdance::Query query = db.query();
		bool tagIdToFilterFound = refreshTagCacheInternal(query);
		if(!tagIdToFilterFound) {
			m_tagIdToFilter = ALL_ID;
			refreshPresetCacheInternal(query);
		}
	}

	int tagCacheSize() const { return m_tagCache.size(); }

	const CachedTag getCachedTag(int index) const { return m_tagCache[index]; }


	int presetIconSize() const { return m_presetIconSize; }

	void setPresetIconSize(int presetIconSize)
	{
		createOrUpdateState(QStringLiteral("preset_icon_size"), presetIconSize);
		refreshPresetCache();
	}


	int tagIdToFilter() const { return m_tagIdToFilter; }

	void setTagIdToFilter(int tagIdToFilter)
	{
		if(tagIdToFilter != m_tagIdToFilter) {
			m_tagIdToFilter = tagIdToFilter;
			refreshPresetCache();
		}
	}

	void refreshPresetCache()
	{
		drawdance::Query query = db.query();
		refreshPresetCacheInternal(query);
	}

	void resetAllPresetsInCache()
	{
		for(CachedPreset &cp : m_presetCache) {
			cp.changedName = {};
			cp.changedDescription = {};
			cp.changedThumbnail = {};
			cp.changedBrush = {};
		}
	}

	int presetCacheSize() const { return m_presetCache.size(); }

	CachedPreset &getCachedPreset(int index) { return m_presetCache[index]; }

	void removeCachedPreset(int index) { m_presetCache.removeAt(index); }

	int getCachedPresetIndexById(int presetId)
	{
		int count = m_presetCache.size();
		for(int i = 0; i < count; ++i) {
			if(m_presetCache[i].id == presetId) {
				return i;
			}
		}
		return -1;
	}

	static ActiveBrush loadBrush(int presetId, const QByteArray &data)
	{
		QJsonParseError err;
		QJsonDocument doc = QJsonDocument::fromJson(data, &err);
		if(err.error != QJsonParseError::NoError) {
			qWarning(
				"Error reading data for preset %d: %s", presetId,
				qUtf8Printable(err.errorString()));
			return ActiveBrush();
		} else if(!doc.isObject()) {
			qWarning("Data for preset %d is not a JSON object", presetId);
			return ActiveBrush();
		} else {
			return ActiveBrush::fromJson(doc.object());
		}
	}

	void enqueuePresetChange(int presetId, const PresetChange &presetChange)
	{
		m_presetChanges.insert(presetId, presetChange);
		m_presetChangeTimer.start();
	}

	void removePresetChange(int presetId) { m_presetChanges.remove(presetId); }

	void writePresetChanges()
	{
		m_presetChangeTimer.stop();
		if(!m_presetChanges.isEmpty()) {
			DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
			drawdance::Query query = db.query();
			const char *sql =
				"update preset set changed_name = ?, changed_description = ?, "
				"changed_thumbnail = ?, changed_type = ?, changed_data = ? "
				"where id = ?";
			if(query.prepare(sql)) {
				QVector<drawdance::Query::Param> params;
				params.reserve(6);
				for(QHash<int, PresetChange>::const_iterator
						it = m_presetChanges.constBegin(),
						end = m_presetChanges.constEnd();
					it != end; ++it) {
					params.clear();
					params.append(
						drawdance::Query::Param::fromOptional(it->name));
					params.append(
						drawdance::Query::Param::fromOptional(it->description));
					if(it->thumbnail.has_value()) {
						params.append(
							BrushPresetModel::toPng(it->thumbnail.value()));
					} else {
						params.append(std::nullopt);
					}
					if(it->brush.has_value()) {
						const ActiveBrush &brush = it->brush.value();
						params.append(brush.presetType());
						params.append(brush.presetData());
					} else {
						params.append(std::nullopt);
						params.append(std::nullopt);
					}
					params.append(it.key());
					if(query.bindAll(params)) {
						query.execPrepared();
					}
				}
			}
			m_presetChanges.clear();
			refreshShortcutsInternal(query);
		}
	}

	void discardPresetChanges()
	{
		m_presetChangeTimer.stop();
		m_presetChanges.clear();
	}

	void initShortcuts()
	{
		drawdance::Query query = db.query();
		refreshShortcutsInternal(query);
	}

	void refreshShortcuts()
	{
		drawdance::Query query = db.query();
		refreshShortcutsInternal(query);
	}

	void getShortcutActions(
		const std::function<void(
			const QString &, const QString &, const QKeySequence &)> &fn) const
	{
		QLocale locale;
		for(PresetShortcutMap::const_iterator
				it = m_presetShortcuts.constBegin(),
				end = m_presetShortcuts.constEnd();
			it != end; ++it) {
			const QKeySequence &shortcut = it.key();
			fn(getActionName(shortcut), it->text(locale), shortcut);
		}
	}

	QKeySequence getShortcutForPresetId(int presetId) const
	{
		for(PresetShortcutMap::const_iterator
				it = m_presetShortcuts.constBegin(),
				end = m_presetShortcuts.constEnd();
			it != end; ++it) {
			if(it->containsId(presetId)) {
				return it.key();
			}
		}
		return QKeySequence();
	}

	QVector<int> getPresetIdsForShortcut(const QKeySequence &shortcut) const
	{
		PresetShortcutMap::const_iterator found =
			m_presetShortcuts.constFind(shortcut);
		if(found == m_presetShortcuts.constEnd()) {
			qWarning(
				"Brush shortcut for %s not found",
				qUtf8Printable(shortcut.toString(QKeySequence::NativeText)));
			return {};
		} else {
			return found->ids();
		}
	}

	bool haveShortcutForPresetId(int presetId)
	{
		for(const PresetShortcut &ps : m_presetShortcuts) {
			for(const PresetShortcutEntry &entry : ps.entries) {
				if(entry.id == presetId) {
					return true;
				}
			}
		}
		return false;
	}

	QVector<ShortcutPreset> getShortcutPresets()
	{
		drawdance::Query query = db.query();
		const char *sql =
			"select p.id, coalesce(p.changed_name, p.name), "
			"coalesce(p.changed_thumbnail, p.thumbnail), "
			"p.shortcut, group_concat(pt.tag_id) from preset p "
			"left join preset_tag pt on pt.preset_id = p.id "
			"group by p.id, coalesce(p.changed_name, p.name), "
			"coalesce(p.changed_thumbnail, p.thumbnail), p.shortcut "
			"order by lower(coalesce(p.changed_name, p.name))";
		QVector<ShortcutPreset> results;
		if(query.exec(sql)) {
			while(query.next()) {
				ShortcutPreset sp;
				sp.id = query.columnInt(0);
				sp.name = query.columnText16(1);
				sp.thumbnail.loadFromData(query.columnBlob(2));
				sp.shortcut = QKeySequence::fromString(
					query.columnText16(3), QKeySequence::PortableText);
				parseGroupedTagIds(query.columnText16(4), sp.tagIds);
				results.append(sp);
			}
		}
		return results;
	}

private:
	static void parseGroupedTagIds(const QString &input, QSet<int> &tagIds)
	{
		QStringList tagIdStrings =
			input.split(QChar(','), compat::SkipEmptyParts);
		tagIds.reserve(tagIdStrings.size());
		for(const QString &tagIdString : tagIdStrings) {
			bool ok;
			int tagId = tagIdString.toInt(&ok);
			if(ok && tagId > 0) {
				tagIds.insert(tagId);
			}
		}
	}

	bool createOrUpdateStateInternal(
		drawdance::Query &query, const QString &key,
		const drawdance::Query::Param &value)
	{
		const char *sql = "insert into state (key, value) values (?, ?) "
						  "on conflict (key) do update set value = ?";
		return query.exec(sql, {key, value, value});
	}

	bool readStateBoolInternal(
		drawdance::Query &query, const QString &key, bool defaultValue)
	{
		if(queryState(query, key)) {
			return query.columnBool(0);
		} else {
			return defaultValue;
		}
	}

	int readStateIntInternal(
		drawdance::Query &query, const QString &key, int defaultValue)
	{
		if(queryState(query, key)) {
			return query.columnInt(0);
		} else {
			return defaultValue;
		}
	}

	bool queryState(drawdance::Query &query, const QString &key)
	{
		return query.exec("select value from state where key = ?", {key}) &&
			   query.next();
	}

	bool refreshTagCacheInternal(drawdance::Query &query)
	{
		m_tagCache.clear();
		QString sql =
			QStringLiteral("select id, name from tag order by lower(name)");
		bool tagIdToFilterFound = m_tagIdToFilter < 0;
		if(query.exec(sql)) {
			while(query.next()) {
				int id = query.columnInt(0);
				m_tagCache.append({id, query.columnText16(1)});
				if(id == m_tagIdToFilter) {
					tagIdToFilterFound = true;
				}
			}
		}
		return tagIdToFilterFound;
	}

	void refreshPresetCacheInternal(drawdance::Query &query)
	{
		m_presetIconSize =
			readStateIntInternal(query, QStringLiteral("preset_icon_size"), 0);
		m_presetCache.clear();

		QString sql = QStringLiteral(
			"select p.id, p.name, p.description, p.thumbnail, p.data, "
			"p.changed_name, p.changed_description, p.changed_thumbnail, "
			"p.changed_data, group_concat(t.tag_id) tags from preset p left "
			"join preset_tag t on t.preset_id = p.id");
		QVector<drawdance::Query::Param> params;
		switch(m_tagIdToFilter) {
		case ALL_ID:
			break;
		case UNTAGGED_ID:
			sql += QStringLiteral(" where not exists(select 1 from preset_tag "
								  "pt where pt.preset_id = p.id)");
			break;
		default:
			sql += QStringLiteral(" join preset_tag pt on pt.preset_id = p.id "
								  "where pt.tag_id = ?");
			params.append(m_tagIdToFilter);
			break;
		}
		sql += QStringLiteral(" group by p.id, p.name, p.description, "
							  "p.thumbnail, p.data order by lower(p.name)");

		if(query.exec(sql, params)) {
			while(query.next()) {
				CachedPreset cp;
				readPreset(cp, query);
				parseGroupedTagIds(query.columnText16(9), cp.tagIds);
				m_presetCache.append(cp);
			}
		}
	}

	void readPreset(Preset &preset, drawdance::Query &query)
	{
		preset.id = query.columnInt(0);
		preset.originalName = query.columnText16(1);
		preset.originalDescription = query.columnText16(2);

		QPixmap pixmap;
		if(pixmap.loadFromData(query.columnBlob(3))) {
			preset.originalThumbnail = pixmap;
		} else {
			qWarning("Error loading thumbnail for preset %d", preset.id);
		}

		preset.originalBrush = loadBrush(preset.id, query.columnBlob(4));

		if(!query.columnNull(5)) {
			preset.changedName = query.columnText16(5);
		}

		if(!query.columnNull(6)) {
			preset.changedDescription = query.columnText16(6);
		}

		if(!query.columnNull(7)) {
			if(pixmap.loadFromData(query.columnBlob(7))) {
				preset.changedThumbnail = pixmap;
			} else {
				qWarning(
					"Error loading changed thumbnail for preset %d", preset.id);
			}
		}

		if(!query.columnNull(8)) {
			preset.changedBrush = loadBrush(preset.id, query.columnBlob(8));
		}
	}

	static QString getActionName(const QKeySequence &shortcut)
	{
		return QStringLiteral("__brushshortcut_%1")
			.arg(shortcut.toString(QKeySequence::PortableText));
	}

	void refreshShortcutsInternal(drawdance::Query &query)
	{
		Q_ASSERT(m_presetModel);
		PresetShortcutMap newPresetShortcuts;

		const char *sql = "select shortcut, id, coalesce(changed_name, name) "
						  "from preset where coalesce(shortcut, '') <> ''";
		if(query.exec(sql)) {
			while(query.next()) {
				QKeySequence shortcut = QKeySequence::fromString(
					query.columnText16(0), QKeySequence::PortableText);
				if(!shortcut.isEmpty()) {
					newPresetShortcuts[shortcut].entries.append(
						{query.columnInt(1), query.columnText16(2)});
				}
			}
		}

		QLocale locale;
		for(PresetShortcutMap::iterator it = newPresetShortcuts.begin(),
										end = newPresetShortcuts.end();
			it != end; ++it) {

			std::sort(it->entries.begin(), it->entries.end());
			QString text = locale.createSeparatedList(it->names());
			const QKeySequence &shortcut = it.key();

			PresetShortcutMap::iterator found =
				m_presetShortcuts.find(shortcut);
			if(found == m_presetShortcuts.end()) {
				emit m_presetModel->shortcutActionAdded(
					getActionName(shortcut), text, shortcut);
			} else {
				if(found->entries != it->entries) {
					emit m_presetModel->shortcutActionChanged(
						getActionName(shortcut), text);
				}
				m_presetShortcuts.erase(found);
			}
		}

		for(QKeySequence &shortcut : m_presetShortcuts.keys()) {
			emit m_presetModel->shortcutActionRemoved(getActionName(shortcut));
		}

		m_presetShortcuts.swap(newPresetShortcuts);
	}

	void initDb()
	{
		drawdance::DatabaseLocker locker(db);
		if(!db.isOpen()) {
			if(utils::db::open(
				   db, QStringLiteral("prush presets"),
				   QStringLiteral("brushes.db"),
				   QStringLiteral("initialbrushpresets.db"))) {
				drawdance::Query query = db.queryWithoutLock();
				query.enableWalMode();
				query.setForeignKeysEnabled(false);
				query.tx([this, &query] {
					return createStateTable(query) && executeMigrations(query);
				});
				cleanupDb(query);
				query.setForeignKeysEnabled(true);
			}
		}
		drawdance::Query query = db.queryWithoutLock();
		refreshTagCacheInternal(query);
		refreshPresetCacheInternal(query);
	}

	void cleanupDb(drawdance::Query &query)
	{
		// Purge any orphaned tag assignments.
		query.exec("delete from preset_tag "
				   "where not exists(select 1 from tag t "
				   "where t.id = preset_tag.tag_id) "
				   "or not exists(select 1 from preset p "
				   "where p.id = preset_tag.preset_id)");
		query.exec("vacuum");
	}

	bool createStateTable(drawdance::Query &query)
	{
		return query.exec("create table if not exists state ("
						  "key text primary key not null, value)");
	}

	bool executeMigrations(drawdance::Query &query)
	{
		QString key = QStringLiteral("migration_version");
		QVariant migrationVersionVariant = readStateIntInternal(query, key, 0);
		if(!migrationVersionVariant.isValid()) {
			return false;
		}

		QVector<std::function<bool(Private *, drawdance::Query &)>> migrations =
			{
				&Private::migrateInitial,
				&Private::migrateChangedPreset,
				&Private::migratePresetShortcuts,
			};

		int originalMigrationVersion = migrationVersionVariant.toInt();
		int migrationVersion = originalMigrationVersion;
		int migrationCount = migrations.size();

		while(migrationVersion < migrationCount) {
			if(migrations[migrationVersion](this, query)) {
				++migrationVersion;
			} else {
				return false;
			}
		}

		return migrationVersion == originalMigrationVersion ||
			   createOrUpdateStateInternal(query, key, migrationVersion);
	}

	bool migrateInitial(drawdance::Query &query)
	{
		return query.exec("create table if not exists preset ("
						  "id integer primary key not null, "
						  "type text not null, "
						  "name text not null, "
						  "description text not null, "
						  "thumbnail blob, "
						  "data blob not null)") &&
			   query.exec("create table if not exists tag ("
						  "id integer primary key not null, "
						  "name text not null)") &&
			   query.exec("create table if not exists preset_tag ("
						  "preset_id integer not null "
						  "references preset (id) on delete cascade,"
						  "tag_id integer not null "
						  "references tag (id) on delete cascade, "
						  "primary key (preset_id, tag_id))") &&
			   query.exec("create index if not exists preset_name_idx "
						  "on preset(lower(name))") &&
			   query.exec("create index if not exists tag_name_idx "
						  "on tag(lower(name))");
	}

	bool migrateChangedPreset(drawdance::Query &query)
	{
		return query.exec("alter table preset add column changed_type text") &&
			   query.exec("alter table preset add column changed_name text") &&
			   query.exec(
				   "alter table preset add column changed_description text") &&
			   query.exec(
				   "alter table preset add column changed_thumbnail blob") &&
			   query.exec("alter table preset add column changed_data blob");
	}

	bool migratePresetShortcuts(drawdance::Query &query)
	{
		return query.exec(
			"alter table preset add column shortcut text not null default ''");
	}

	static drawdance::Database db;
	QTimer m_presetChangeTimer;
	QHash<int, PresetChange> m_presetChanges;
	QVector<CachedTag> m_tagCache;
	QVector<CachedPreset> m_presetCache;
	PresetShortcutMap m_presetShortcuts;
	BrushPresetModel *m_presetModel = nullptr;
	int m_presetIconSize = BrushPresetModel::THUMBNAIL_SIZE;
	int m_tagIdToFilter = -1;
};

drawdance::Database BrushPresetTagModel::Private::db;

bool Tag::accepts(const QSet<int> &tagIds) const
{
	switch(id) {
	case ALL_ID:
		return true;
	case UNTAGGED_ID:
		return tagIds.isEmpty();
	default:
		return tagIds.contains(id);
	}
}

BrushPresetTagModel::BrushPresetTagModel(QObject *parent)
	: QAbstractItemModel(parent)
	, d(new Private)
{
	d->setPresetModel(new BrushPresetModel(this));
	maybeConvertOldPresets();
	d->initShortcuts();
	connect(
		this, &QAbstractItemModel::modelAboutToBeReset, d->presetModel(),
		&BrushPresetModel::tagsAboutToBeReset);
	connect(
		this, &QAbstractItemModel::modelReset, d->presetModel(),
		&BrushPresetModel::tagsReset);
}

BrushPresetTagModel::~BrushPresetTagModel()
{
	delete d;
}

BrushPresetModel *BrushPresetTagModel::presetModel()
{
	return d->presetModel();
}

int BrushPresetTagModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : d->tagCacheSize() + TAG_OFFSET;
}

int BrushPresetTagModel::columnCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : 1;
}

QModelIndex BrushPresetTagModel::parent(const QModelIndex &) const
{
	return QModelIndex();
}

QModelIndex
BrushPresetTagModel::index(int row, int column, const QModelIndex &parent) const
{
	if(parent.isValid() || row < 0 || row - TAG_OFFSET >= d->tagCacheSize() ||
	   column != 0) {
		return QModelIndex();
	} else {
		return createIndex(row, column);
	}
}

QVariant BrushPresetTagModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid() || !isTagRowInBounds(index.row()) ||
	   index.column() != 0) {
		return QVariant();
	}

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
			return d->getCachedTag(index.row() - TAG_OFFSET).name;
		}
	case Qt::ToolTipRole:
		switch(index.row()) {
		case ALL_ROW:
			return tr("Show all brushes, regardless of tagging.");
		case UNTAGGED_ROW:
			return tr("Show brushes not assigned to any tag.");
		default:
			return d->getCachedTag(index.row() - TAG_OFFSET).name;
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
		return Tag{ALL_ID, data(createIndex(row, 0)).toString()};
	case UNTAGGED_ROW:
		return Tag{UNTAGGED_ID, data(createIndex(row, 0)).toString()};
	default:
		if(isTagRowInBounds(row)) {
			const CachedTag &ct = d->getCachedTag(row - TAG_OFFSET);
			return Tag{ct.id, ct.name};
		} else {
			return Tag{0, QString()};
		}
	}
}

int BrushPresetTagModel::getTagRowById(int tagId) const
{
	switch(tagId) {
	case ALL_ID:
		return ALL_ROW;
	case UNTAGGED_ID:
		return UNTAGGED_ROW;
	default: {
		int count = d->tagCacheSize();
		for(int i = 0; i < count; ++i) {
			if(d->getCachedTag(i).id == tagId) {
				return i + TAG_OFFSET;
			}
		}
		return -1;
	}
	}
}

int BrushPresetTagModel::newTag(const QString &name)
{
	beginResetModel();
	int tagId = d->createTag(name);
	d->refreshTagCache();
	endResetModel();
	return tagId;
}

int BrushPresetTagModel::editTag(int tagId, const QString &name)
{
	beginResetModel();
	d->updateTagName(tagId, name);
	d->refreshTagCache();
	endResetModel();
	return getTagRowById(tagId);
}

void BrushPresetTagModel::deleteTag(int tagId)
{
	beginResetModel();
	d->deleteTagById(tagId);
	d->refreshTagCache();
	endResetModel();
}

int BrushPresetTagModel::getStateInt(const QString &key, int defaultValue) const
{
	return d->readStateInt(key, defaultValue);
}

bool BrushPresetTagModel::setStateInt(const QString &key, int value)
{
	return d->createOrUpdateState(key, value);
}

bool BrushPresetTagModel::isBuiltInTag(int row)
{
	return row == ALL_ROW || row == UNTAGGED_ROW;
}

bool BrushPresetTagModel::isTagRowInBounds(int row) const
{
	if(isBuiltInTag(row)) {
		return true;
	} else {
		int i = row - TAG_OFFSET;
		return i >= 0 && i < d->tagCacheSize();
	}
}

void BrushPresetTagModel::maybeConvertOldPresets()
{
	if(!d->readStateBool("classic_presets_loaded", false) &&
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
	metadata << QPair<QString, QStringList>{
		BrushPresetModel::tr("Default"), QStringList()};

	while(!(line = in.readLine()).isNull()) {
		// # is reserved for comments for now, but might be used for extra
		// metadata fields later
		if(line.isEmpty() || line.at(0) == '#')
			continue;

		// Lines starting with \ start new folders. Nested folders are not
		// supported currently.
		if(line.at(0) == '\\') {
			metadata << QPair<QString, QStringList>{line.mid(1), QStringList()};
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
		brushDir, QStringList() << "*.dpbrush", QDir::Files | QDir::Readable,
		QDirIterator::Subdirectories};

	while(entries.hasNext()) {
		QFile f{entries.next()};
		QString filename = entries.filePath().mid(brushDir.length());

		if(!f.open(QFile::ReadOnly)) {
			qWarning() << "Couldn't open" << filename
					   << "due to:" << f.errorString();
			continue;
		}

		QJsonParseError err;
		QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
		if(doc.isNull()) {
			qWarning() << "Couldn't parse" << filename
					   << "due to:" << err.errorString();
			continue;
		}

		brushes[filename] = OldBrushPreset{
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

	for(const QPair<QString, QStringList> &metaFolder : metadata) {
		folders << OldPresetFolder{metaFolder.first, QVector<OldBrushPreset>()};
		OldPresetFolder &folder = folders.last();

		for(const QString &filename : metaFolder.second) {
			const OldBrushPreset b = brushes.take(filename);
			if(!b.filename.isEmpty()) {
				folder.presets << b;
			}
		}
	}

	// Add any remaining unsorted presets to a default folder
	if(folders.isEmpty()) {
		folders.push_front(
			OldPresetFolder{tr("Default"), QVector<OldBrushPreset>()});
	}

	OldPresetFolder &defaultFolder = folders.first();

	QMapIterator<QString, OldBrushPreset> i{brushes};
	while(i.hasNext()) {
		defaultFolder.presets << i.next().value();
	}

	if(folders.size() > 1 || !folders.first().presets.isEmpty()) {
		ActiveBrush brush(ActiveBrush::CLASSIC);
		int brushCount = 0;

		for(const OldPresetFolder &folder : folders) {
			int tagId = newTag(folder.title);
			for(const OldBrushPreset &preset : folder.presets) {
				++brushCount;
				QString name = tr("Classic Brush %1").arg(brushCount);
				QString description =
					tr("Converted from %1.").arg(preset.filename);
				brush.setClassic(preset.brush);
				std::optional<Preset> opt = d->presetModel()->newPreset(
					name, description, brush.presetThumbnail(), brush, tagId);
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
		result.errors.append(tr("Can't open '%1'.").arg(file));
		return result;
	}

	beginResetModel();
	QVector<ImportBrushGroup> groups = readOrderConf(result, file, zr);
	for(const ImportBrushGroup &group : groups) {
		if(!group.brushes.isEmpty()) {
			readImportBrushes(result, zr, group.name, group.brushes);
		}
	}

	d->refreshTagCache();
	d->refreshPresetCache();
	endResetModel();
	return result;
}

namespace {
static QRegularExpression newlineRe{"[\r\n]+"};
}

QVector<BrushPresetTagModel::ImportBrushGroup>
BrushPresetTagModel::readOrderConf(
	BrushImportResult &result, const QString &file,
	const drawdance::ZipReader &zr)
{
	static QRegularExpression groupRe{"^Group:\\s*(.+)$"};

	drawdance::ZipReader::File orderFile =
		zr.readFile(QStringLiteral("order.conf"));
	if(orderFile.isNull()) {
		qWarning("Error reading order.conf from zip: %s", DP_error());
		result.errors.append(
			tr("Invalid brush pack: order.conf not found inside"));
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
		result.errors.append(
			tr("Invalid brush pack: order.conf contains no brushes"));
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
			QString candidate = QStringLiteral("%1 (%2)").arg(groupName).arg(i);
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
		if(readImportBrush(result, zr, prefix, brush, description, thumbnail)) {
			if(tagId == -1) {
				tagId = d->createTag(tagName);
				if(tagId < 1) {
					result.errors.append(
						tr("Could not create tag '%1'.").arg(tagName));
					break;
				}
				result.importedTags.append({tagId, tagName});
			}

			int slashIndex = prefix.indexOf("/");
			QString name =
				QStringLiteral("%1/%2-%3")
					.arg(tagName)
					.arg(i * 100, 4, 10, QLatin1Char('0'))
					.arg(slashIndex < 0 ? prefix : prefix.mid(slashIndex + 1));

			int presetId = d->createPreset(
				name, description, BrushPresetModel::toPng(thumbnail),
				brush.presetType(), brush.presetData());
			if(presetId < 1) {
				result.errors.append(
					tr("Could not create brush preset '%1'.").arg(name));
			}

			++result.importedBrushCount;

			if(!d->createPresetTag(presetId, tagId)) {
				result.errors.append(
					tr("Could not assign brush '%1' to tag '%2'.")
						.arg(name)
						.arg(tagName));
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
		result.errors.append(tr("Can't read brush file '%1'").arg(mybPath));
		return false;
	}

	QByteArray mybBytes = mybFile.readBytes();
	QJsonValue value;
	switch(guessImportBrushType(mybBytes)) {
	case ImportBrushType::Json:
		value = readImportBrushJson(result, mybPath, mybBytes);
		break;
	case ImportBrushType::Old:
		value = readImportBrushOld(result, mybPath, mybBytes);
		break;
	default:
		result.errors.append(
			tr("Unknown brush format in file '%1'").arg(mybPath));
		return false;
	}

	if(!value.isObject()) {
		return false;
	}

	QJsonObject object = value.toObject();
	if(!outBrush.fromExportJson(object)) {
		result.errors.append(
			tr("Can't load brush from brush file '%1'").arg(mybPath));
		return false;
	}

	outDescription = object["notes"].toString();
	if(outDescription.isNull()) {
		outDescription = object["description"].toString();
		if(outDescription.isNull()) {
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

	if(outThumbnail.width() != BrushPresetModel::THUMBNAIL_SIZE ||
	   outThumbnail.height() != BrushPresetModel::THUMBNAIL_SIZE) {
		outThumbnail = outThumbnail.scaled(
			BrushPresetModel::THUMBNAIL_SIZE, BrushPresetModel::THUMBNAIL_SIZE,
			Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}

	return true;
}

BrushPresetTagModel::ImportBrushType
BrushPresetTagModel::guessImportBrushType(const QByteArray &mybBytes)
{
	for(char c : mybBytes) {
		if(c == '{') {
			return ImportBrushType::Json;
		} else if(c == '#') {
			return ImportBrushType::Old;
		} else if(!std::isspace(c)) {
			break;
		}
	}
	return ImportBrushType::Unknown;
}

QJsonValue BrushPresetTagModel::readImportBrushJson(
	BrushImportResult &result, const QString &mybPath,
	const QByteArray &mybBytes)
{
	QJsonParseError error;
	QJsonDocument json = QJsonDocument::fromJson(mybBytes, &error);
	if(error.error != QJsonParseError::NoError) {
		result.errors.append(
			tr("Brush file '%1' does not contain valid JSON: %1")
				.arg(mybPath)
				.arg(error.errorString()));
		return QJsonValue();
	} else if(!json.isObject()) {
		result.errors.append(
			tr("Brush file '%1' does not contain a JSON object").arg(mybPath));
		return QJsonValue();
	} else {
		return json.object();
	}
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-2.0-or-later
// SDPX—SnippetName: old format parsing from MyPaint
QJsonValue BrushPresetTagModel::readImportBrushOld(
	BrushImportResult &result, const QString &mybPath,
	const QByteArray &mybBytes)
{
	QVector<QPair<QString, QString>> rawSettings;
	int version = 1;
	for(const QString &rawLine :
		QString::fromUtf8(mybBytes).split(newlineRe, compat::SkipEmptyParts)) {
		QString line = rawLine.trimmed();
		if(line.isEmpty() || line.startsWith(QChar('#'))) {
			continue;
		}

		int spaceIndex = line.indexOf(QChar(' '));
		if(spaceIndex <= 0) {
			continue;
		}

		QString rawCname = line.left(spaceIndex);
		QString rawValue = line.mid(spaceIndex + 1);
		if(rawCname == QStringLiteral("version")) {
			version = rawValue.toInt();
			if(version != 1 && version != 2) {
				result.errors.append(
					tr("Brush file '%1' has invalid version %d")
						.arg(mybPath)
						.arg(version));
				return QJsonValue();
			}
		} else {
			rawSettings.append({rawCname, rawValue});
		}
	}

	QJsonObject settings = {
		{QStringLiteral("anti_aliasing"),
		 QJsonObject({
			 {QStringLiteral("base_value"), 0.0},
			 {QStringLiteral("inputs"), QJsonObject()},
		 })},
	};
	for(const QPair<QString, QString> &rawSetting : rawSettings) {
		const QString &rawCname = rawSetting.first;
		const QString &rawValue = rawSetting.second;

		if(rawCname == QStringLiteral("parent_brush_name") ||
		   rawCname == QStringLiteral("group") ||
		   rawCname == QStringLiteral("comment") ||
		   rawCname == QStringLiteral("notes") ||
		   rawCname == QStringLiteral("description")) {
			settings[rawCname] = QUrl::fromPercentEncoding(rawValue.toUtf8());
		} else if(version <= 1 && rawCname == QStringLiteral("color")) {
			QStringList rawColor =
				rawValue.split(QChar(' '), compat::SkipEmptyParts);
			if(rawColor.size() == 3) {
				QColor color(
					qBound(0, rawColor[0].toInt(), 255),
					qBound(0, rawColor[1].toInt(), 255),
					qBound(0, rawColor[2].toInt(), 255));
				if(color.isValid()) {
					settings[QStringLiteral("color_h")] = QJsonObject(
						{{QStringLiteral("base_value"), color.hueF()},
						 {QStringLiteral("inputs"), QJsonObject()}});
					settings[QStringLiteral("color_s")] = QJsonObject(
						{{QStringLiteral("base_value"), color.saturationF()},
						 {QStringLiteral("inputs"), QJsonObject()}});
					settings[QStringLiteral("color_v")] = QJsonObject(
						{{QStringLiteral("base_value"), color.valueF()},
						 {QStringLiteral("inputs"), QJsonObject()}});
					continue;
				}
			}
			result.errors.append(
				tr("Brush file '%1' contains invalid 'color' setting")
					.arg(mybPath));
		} else if(
			(version <= 1 && (rawCname == QStringLiteral("change_radius") ||
							  rawCname == QStringLiteral("painting_time"))) ||
			(version <= 2 &&
			 rawCname == QStringLiteral("adapt_color_from_image"))) {
			result.errors.append(
				tr("Brush file '%1' contains obsolete '%2' setting")
					.arg(mybPath));
		} else {
			QString cname;
			double (*scale)(double) = nullptr;
			if(version <= 1 && rawCname == QStringLiteral("speed")) {
				cname = QStringLiteral("speed1");
			} else if(rawCname == QStringLiteral("color_hue")) {
				cname = QStringLiteral("change_color_h");
				scale = scaleOldColorHue;
			} else if(rawCname == QStringLiteral("color_saturation")) {
				cname = QStringLiteral("change_color_hsv_s");
				scale = scaleOldColorSaturationOrValue;
			} else if(rawCname == QStringLiteral("color_value")) {
				cname = QStringLiteral("change_color_v");
				scale = scaleOldColorSaturationOrValue;
			} else if(rawCname == QStringLiteral("speed_slowness")) {
				cname = QStringLiteral("speed1_slowness");
			} else if(rawCname == QStringLiteral("change_color_s")) {
				cname = QStringLiteral("change_color_hsv_s");
			} else if(rawCname == QStringLiteral("stroke_thresold")) {
				cname = QStringLiteral("stroke_threshold");
			} else {
				cname = rawCname;
			}

			QStringList parts = rawValue.split(QChar('|'));
			if(parts.size() >= 1) {
				double baseValue = parts.takeFirst().toDouble();
				if(scale) {
					baseValue = scale(baseValue);
				}

				QJsonObject inputs;
				for(const QString &part : parts) {
					QString trimmedPart = part.trimmed();
					int partSpaceIndex = trimmedPart.indexOf(QChar(' '));
					if(partSpaceIndex > 0) {
						QJsonArray points;
						if(version <= 1) {
							static QRegularExpression whitespaceRe(
								QStringLiteral("\\s"));
							QStringList rawPoints =
								trimmedPart.mid(partSpaceIndex + 1)
									.split(
										whitespaceRe, compat::SkipEmptyParts);
							points.append(QJsonArray({0.0, 0.0}));
							while(rawPoints.size() >= 2) {
								double x = rawPoints.takeFirst().toDouble();
								if(x == 0.0) {
									break;
								} else {
									double y = rawPoints.takeFirst().toDouble();
									points.append(
										QJsonArray({x, scale ? scale(y) : y}));
								}
							}
						} else {
							for(const QString &rawPoint :
								trimmedPart.mid(partSpaceIndex + 1)
									.split(
										QStringLiteral(", "),
										compat::SkipEmptyParts)) {
								static QRegularExpression pointRe(
									QStringLiteral("\\A\\s*\\((\\S+)\\s+(\\S+)"
												   "\\)\\s*\\z"));
								QRegularExpressionMatch match =
									pointRe.match(rawPoint);
								if(match.hasMatch()) {
									double x = match.captured(1).toDouble();
									double y = match.captured(2).toDouble();
									points.append(
										QJsonArray({x, scale ? scale(y) : y}));
								} else {
									qWarning(
										"Invalid point '%s'",
										qUtf8Printable(rawPoint));
								}
							}
						}

						inputs[trimmedPart.left(partSpaceIndex)] = points;
					}
				}

				settings[cname] = QJsonObject{
					{QStringLiteral("base_value"), baseValue},
					{QStringLiteral("inputs"), inputs},
				};
			}
		}
	}

	return QJsonObject({
		{QStringLiteral("version"), 3},
		{QStringLiteral("settings"), settings},
	});
}

double BrushPresetTagModel::scaleOldColorHue(double y)
{
	return y * 64.0 / 360.0;
}

double BrushPresetTagModel::scaleOldColorSaturationOrValue(double y)
{
	return y * 128.0 / 256.0;
}
// SDPX—SnippetEnd


BrushExportResult BrushPresetTagModel::exportBrushPack(
	const QString &file, const QVector<BrushExportTag> &exportTags) const
{
	BrushExportResult result = {false, {}, 0};

	drawdance::ZipWriter zw{file};
	if(zw.isNull()) {
		qWarning("Error opening '%s': %s", qUtf8Printable(file), DP_error());
		result.errors.append(tr("Can't open '%1'.").arg(file));
		return result;
	}

	QStringList order;
	{
		QSet<QString> tagPaths;
		QSet<QString> tagNames;
		for(const BrushExportTag &tag : exportTags) {
			exportTag(result, order, tagPaths, tagNames, zw, tag);
		}
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
	BrushExportResult &result, QStringList &order, QSet<QString> &tagPaths,
	QSet<QString> &tagNames, drawdance::ZipWriter &zw,
	const BrushExportTag &tag) const
{
	QString tagPath = makeExportNameUnique(tagPaths, makeExportSafe(tag.name));
	if(!zw.addDir(tagPath)) {
		qWarning(
			"Error creating tag directory '%s': %s", qUtf8Printable(tagPath),
			DP_error());
		result.errors.append(tr("Can't export tag '%1'").arg(tag.name));
		return;
	}

	order.append(QStringLiteral("Group: %1")
					 .arg(makeExportNameUnique(tagNames, tag.name)));
	QSet<QString> presetNames;
	for(int presetId : tag.presetIds) {
		exportPreset(result, order, presetNames, zw, tagPath, presetId);
	}
	order.append(QString{});
}

void BrushPresetTagModel::exportPreset(
	BrushExportResult &result, QStringList &order, QSet<QString> &presetNames,
	drawdance::ZipWriter &zw, const QString &tagPath, int presetId) const
{
	std::optional<Preset> opt = d->readPresetById(presetId);
	if(!opt.has_value()) {
		qWarning("Preset %d not found", presetId);
		result.errors.append(tr("Missing preset %1").arg(presetId));
		return;
	}

	const Preset &preset = opt.value();
	QByteArray exportData =
		preset.originalBrush.toExportJson(preset.originalDescription);

	QString presetName = makeExportNameUnique(
		presetNames, getExportName(presetId, preset.originalName));
	QString presetPath = QStringLiteral("%1/%2").arg(tagPath, presetName);

	QString dataPath = QStringLiteral("%1.myb").arg(presetPath);
	if(!zw.addFile(dataPath, exportData)) {
		qWarning(
			"Error exporting preset '%s': %s", qUtf8Printable(dataPath),
			DP_error());
		result.errors.append(
			tr("Can't export preset '%1'").arg(preset.originalName));
		return;
	}

	QString thumbnailPath = QStringLiteral("%1_prev.png").arg(presetPath);
	if(!zw.addFile(
		   thumbnailPath, BrushPresetModel::toPng(preset.originalThumbnail))) {
		qWarning(
			"Error exporting preset thumbnail '%s': %s",
			qUtf8Printable(thumbnailPath), DP_error());
		result.errors.append(
			tr("Can't export preset thumbnail '%1'").arg(preset.originalName));
		return;
	}

	order.append(presetPath);
	++result.exportedBrushCount;
}

QString BrushPresetTagModel::makeExportNameUnique(
	QSet<QString> &names, const QString &name)
{
	if(names.contains(name)) {
		QString newName;
		int i = 2;
		do {
			newName = name + QString::number(i++);
		} while(names.contains(newName));
		names.insert(newName);
		return newName;
	} else {
		names.insert(name);
		return name;
	}
}

QString
BrushPresetTagModel::getExportName(int presetId, const QString presetName)
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
{
}

bool BrushPresetModel::isPresetRowInBounds(int row) const
{
	return row >= 0 && row < d->presetCacheSize();
}

int BrushPresetModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid()) {
		return 0;
	} else {
		return d->presetCacheSize();
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

QModelIndex
BrushPresetModel::index(int row, int column, const QModelIndex &parent) const
{
	if(parent.isValid() || row < 0 || row >= d->presetCacheSize() ||
	   column != 0) {
		return QModelIndex();
	} else {
		return createIndex(row, column, quintptr(0));
	}
}

QModelIndex BrushPresetModel::cachedIndexForId(int presetId)
{
	int i = d->getCachedPresetIndexById(presetId);
	return i == -1 ? QModelIndex() : createIndex(i, 0, quintptr(0));
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
	if(role == Qt::SizeHintRole) {
		return iconSize();
	}

	bool cached = index.internalId() == 0;
	if(cached && (index.column() != 0 || index.row() >= d->presetCacheSize())) {
		return QVariant();
	}

	switch(role) {
	case IdRole:
		if(cached) {
			return d->getCachedPreset(index.row()).id;
		} else {
			return int(index.internalId());
		}
	case Qt::ToolTipRole:
	case FilterRole:
	case EffectiveTitleRole:
		if(cached) {
			return d->getCachedPreset(index.row()).effectiveName();
		} else {
			return d->readPresetEffectiveNameById(index.internalId());
		}
	case EffectiveDescriptionRole:
		if(cached) {
			return d->getCachedPreset(index.row()).effectiveDescription();
		} else {
			return d->readPresetEffectiveDescriptionById(index.internalId());
		}
	case EffectiveThumbnailRole: {
		if(cached) {
			return d->getCachedPreset(index.row()).effectiveThumbnail();
		} else {
			QPixmap pixmap;
			if(pixmap.loadFromData(
				   d->readPresetEffectiveThumbnailById(index.internalId()))) {
				return pixmap;
			} else {
				return QVariant();
			}
		}
	}
	case HasChangesRole:
		if(cached) {
			return d->getCachedPreset(index.row()).hasChanges();
		} else {
			return d->readPresetHasChangesById(index.internalId());
		}
	case PresetRole:
		if(cached) {
			return QVariant::fromValue<Preset>(d->getCachedPreset(index.row()));
		} else {
			std::optional<Preset> opt = d->readPresetById(index.internalId());
			return opt.has_value() ? QVariant::fromValue<Preset>(opt.value())
								   : QVariant();
		}
	default:
		return QVariant();
	}
}

int BrushPresetModel::getIdFromIndex(const QModelIndex &index)
{
	if(index.isValid()) {
		if(index.internalId() == 0) {
			int row = index.row();
			return isPresetRowInBounds(row) ? d->getCachedPreset(row).id : 0;
		} else {
			return index.internalId();
		}
	} else {
		return 0;
	}
}

void BrushPresetModel::setTagIdToFilter(int tagId)
{
	beginResetModel();
	d->setTagIdToFilter(tagId);
	endResetModel();
}

QList<TagAssignment> BrushPresetModel::getTagAssignments(int presetId)
{
	int i = d->getCachedPresetIndexById(presetId);
	if(i == -1) {
		return d->readTagAssignmentsByPresetId(presetId);
	} else {
		const QSet<int> assignedTagIds = d->getCachedPreset(i).tagIds;
		QList<TagAssignment> tagAssignments;
		int tagCount = d->tagCacheSize();
		for(int j = 0; j < tagCount; ++j) {
			const CachedTag &ct = d->getCachedTag(j);
			tagAssignments.append(
				{ct.id, ct.name, assignedTagIds.contains(ct.id)});
		}
		return tagAssignments;
	}
}

std::optional<Preset> BrushPresetModel::searchPresetBrushData(int presetId)
{
	int i = d->getCachedPresetIndexById(presetId);
	if(i == -1) {
		return d->readPresetById(presetId);
	} else {
		return d->getCachedPreset(i);
	}
}

QPixmap BrushPresetModel::searchPresetThumbnail(int presetId)
{
	int i = d->getCachedPresetIndexById(presetId);
	if(i == -1) {
		QPixmap pixmap;
		if(pixmap.loadFromData(d->readPresetEffectiveThumbnailById(presetId))) {
			return pixmap;
		} else {
			return QPixmap();
		}
	} else {
		return d->getCachedPreset(i).effectiveThumbnail();
	}
}

bool BrushPresetModel::changeTagAssignment(
	int presetId, int tagId, bool assigned)
{
	bool ok = assigned ? d->createPresetTag(presetId, tagId)
					   : d->deletePresetTag(presetId, tagId);

	int tagIdToFilter = d->tagIdToFilter();
	switch(tagIdToFilter) {
	case ALL_ID:
		if(int i = d->getCachedPresetIndexById(presetId); i != -1) {
			CachedPreset &cp = d->getCachedPreset(i);
			if(assigned) {
				cp.tagIds.insert(tagId);
			} else {
				cp.tagIds.remove(tagId);
			}
			QModelIndex idx = index(i);
			emit dataChanged(idx, idx);
		}
		break;
	case UNTAGGED_ID:
		if(assigned) {
			int i = d->getCachedPresetIndexById(presetId);
			if(i != -1) {
				beginRemoveRows(QModelIndex(), i, i);
				d->removeCachedPreset(i);
				endRemoveRows();
			}
		}
		break;
	default:
		if(tagId == tagIdToFilter) {
			int i = d->getCachedPresetIndexById(presetId);
			if(assigned && i == -1) {
				beginResetModel();
				d->refreshPresetCache();
				endResetModel();
			} else if(!assigned && i != -1) {
				beginRemoveRows(QModelIndex(), i, i);
				d->removeCachedPreset(i);
				endRemoveRows();
			} else {
				CachedPreset &cp = d->getCachedPreset(i);
				if(assigned) {
					cp.tagIds.insert(tagId);
				} else {
					cp.tagIds.remove(tagId);
				}
				QModelIndex idx = index(i);
				emit dataChanged(idx, idx);
			}
		}
		break;
	}

	return ok;
}

std::optional<Preset> BrushPresetModel::newPreset(
	const QString &name, const QString description, const QPixmap &thumbnail,
	const ActiveBrush &brush, int tagId)
{
	beginResetModel();
	int presetId = d->createPreset(
		name, description, toPng(thumbnail), brush.presetType(),
		brush.presetData());
	if(presetId > 0 && tagId > 0) {
		d->createPresetTag(presetId, tagId);
	}
	d->refreshPresetCache();
	endResetModel();
	d->refreshShortcuts();
	if(presetId > 0) {
		return Preset{
			presetId, name, description, thumbnail, brush, {}, {}, {}, {},
		};
	} else {
		return {};
	}
}

bool BrushPresetModel::updatePreset(
	int presetId, const QString &name, const QString &description,
	const QPixmap &thumbnail, const ActiveBrush &brush)
{
	beginResetModel();
	d->removePresetChange(presetId);
	bool ok = d->updatePreset(
		presetId, name, description, toPng(thumbnail), brush.presetType(),
		brush.presetData());
	d->refreshPresetCache();
	endResetModel();
	d->refreshShortcuts();
	emit presetChanged(presetId, name, description, thumbnail, brush);
	return ok;
}

bool BrushPresetModel::updatePresetBrush(int presetId, const ActiveBrush &brush)
{
	beginResetModel();
	d->removePresetChange(presetId);
	bool ok =
		d->updatePresetBrush(presetId, brush.presetType(), brush.presetData());
	d->refreshPresetCache();
	endResetModel();
	return ok;
}

bool BrushPresetModel::updatePresetShortcut(
	int presetId, const QKeySequence &shortcut)
{
	bool ok = d->updatePresetShortcut(
		presetId, shortcut.toString(QKeySequence::PortableText));
	d->refreshShortcuts();
	emit presetShortcutChanged(presetId, shortcut);
	return ok;
}

bool BrushPresetModel::deletePreset(int presetId)
{
	bool ok = d->deletePresetById(presetId);
	int i = d->getCachedPresetIndexById(presetId);
	if(i != -1) {
		beginRemoveRows(QModelIndex(), i, i);
		d->removeCachedPreset(i);
		endRemoveRows();
	}
	d->refreshShortcuts();
	emit presetRemoved(presetId);
	return ok;
}

void BrushPresetModel::changePreset(
	int presetId, const std::optional<QString> &name,
	const std::optional<QString> &description,
	const std::optional<QPixmap> &thumbnail,
	const std::optional<ActiveBrush> &brush, bool inEraserSlot)
{
	int i = d->getCachedPresetIndexById(presetId);
	if(i != -1) {
		CachedPreset &cp = d->getCachedPreset(i);
		cp.changedName = name;
		cp.changedDescription = description;
		cp.changedThumbnail = thumbnail;
		cp.changedBrush = brush;
		if(brush.has_value() && inEraserSlot) {
			cp.changedBrush->setEraser(cp.originalBrush.isEraser());
		}
		QModelIndex idx = index(i);
		emit dataChanged(idx, idx);
	}
	d->enqueuePresetChange(presetId, {name, description, thumbnail, brush});
}

void BrushPresetModel::resetAllPresetChanges()
{
	beginResetModel();
	d->discardPresetChanges();
	d->resetAllPresetChanges();
	d->resetAllPresetsInCache();
	endResetModel();
	d->refreshShortcuts();
}

void BrushPresetModel::writePresetChanges()
{
	d->writePresetChanges();
}

int BrushPresetModel::countNames(const QString &name) const
{
	return d->readPresetCountByName(name);
}

void BrushPresetModel::getShortcutActions(
	const std::function<
		void(const QString &, const QString &, const QKeySequence &)> &fn) const
{
	d->getShortcutActions(fn);
}

QKeySequence BrushPresetModel::getShortcutForPresetId(int presetId)
{
	return d->getShortcutForPresetId(presetId);
}

QVector<int>
BrushPresetModel::getPresetIdsForShortcut(const QKeySequence &shortcut) const
{
	return d->getPresetIdsForShortcut(shortcut);
}

QVector<ShortcutPreset> BrushPresetModel::getShortcutPresets() const
{
	return d->getShortcutPresets();
}

QSize BrushPresetModel::iconSize() const
{
	int dimension = iconDimension();
	return QSize(dimension, dimension);
}

int BrushPresetModel::iconDimension() const
{
	int dimension = d->presetIconSize();
	return dimension <= 0 ? THUMBNAIL_SIZE : dimension;
}

void BrushPresetModel::setIconDimension(int dimension)
{
	beginResetModel();
	d->setPresetIconSize(dimension);
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
	QString file = fileInfo.path() + QDir::separator() +
				   fileInfo.completeBaseName() + "_prev.png";
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
	buffer.open(DP_QT_WRITE_FLAGS);
	pixmap.save(&buffer, "PNG");
	buffer.close();
	return bytes;
}

}
