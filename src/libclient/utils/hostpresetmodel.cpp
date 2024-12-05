// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpmsg/acl.h>
}
#include "libclient/utils/hostpresetmodel.h"
#include "libshared/util/paths.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QPair>

namespace utils {

class StateDatabase;

HostPresetModel::HostPresetModel(StateDatabase &state, QObject *parent)
	: QAbstractItemModel(parent)
	, m_state(state)
{
	loadPresets();
}

QModelIndex
HostPresetModel::index(int row, int column, const QModelIndex &parent) const
{
	if(row >= 0 && row < rowCount(parent) && column == 0) {
		return createIndex(row, column);
	} else {
		return QModelIndex();
	}
}

QModelIndex HostPresetModel::parent(const QModelIndex &child) const
{
	Q_UNUSED(child);
	return QModelIndex();
}

int HostPresetModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_presets.size() + 1;
}

int HostPresetModel::columnCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : 1;
}

QVariant HostPresetModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && !index.parent().isValid() && index.column() == 0) {
		int row = index.row();
		if(row == 0) {
			switch(role) {
			case Qt::DisplayRole:
			case TitleRole:
				return tr("Defaults");
			case SortRole:
				return QStringLiteral("0");
			case DefaultRole:
				return true;
			default:
				break;
			}
		} else if(int i = row - 1; i >= 0 && i < m_presets.size()) {
			switch(role) {
			case Qt::DisplayRole:
			case TitleRole:
				return m_presets[i].title;
			case IdRole:
				return m_presets[i].id;
			case VersionRole:
				return m_presets[i].version;
			case DataRole:
				return m_presets[i].data;
			case SortRole:
				return QStringLiteral("1%1").arg(m_presets[i].title);
			case DefaultRole:
				return false;
			default:
				break;
			}
		}
	}
	return QVariant();
}

QSet<int> HostPresetModel::getPresetIdsByTitle(const QString &title) const
{
	return getPresetIdsByTitleWith(m_state, title);
}

QSet<int> HostPresetModel::getPresetIdsByTitleWith(
	StateDatabase &state, const QString &title)
{
	QSet<int> ids;
	QString sql = QStringLiteral("select id from host_presets where title = ?");
	StateDatabase::Query qry = state.query();
	if(qry.exec(sql, {title})) {
		while(qry.next()) {
			ids.insert(qry.value(0).toInt());
		}
	}
	return ids;
}

bool HostPresetModel::renamePresetById(int id, const QString &title)
{
	int count = m_presets.size();
	for(int i = 0; i < count; ++i) {
		if(m_presets[i].id == id) {
			m_presets[i].title = title;
			QModelIndex idx = index(i, 0);
			emit dataChanged(idx, idx, {Qt::DisplayRole, TitleRole, SortRole});

			int j = 0;
			while(j < count) {
				if(m_presets[j].id != id && m_presets[j].title == title) {
					beginRemoveRows(QModelIndex(), j, j);
					m_presets.removeAt(j);
					endRemoveRows();
					--count;
				} else {
					++j;
				}
			}

			m_state.tx([&](StateDatabase::Query &qry) {
				return qry.exec(
						   QStringLiteral("delete from host_presets "
										  "where id <> ? and title = ?"),
						   {id, title}) &&
					   qry.exec(
						   QStringLiteral("update host_presets "
										  "set title = ? where id = ?"),
						   {title, id});
			});
			return true;
		}
	}
	return false;
}

bool HostPresetModel::deletePresetById(int id)
{
	int count = m_presets.size();
	for(int i = 0; i < count; ++i) {
		if(m_presets[i].id == id) {
			m_state.query().exec(
				QStringLiteral("delete from host_presets where id = ?"), {id});
			beginRemoveRows(QModelIndex(), i, i);
			m_presets.removeAt(i);
			endRemoveRows();
			return true;
		}
	}
	return false;
}

void HostPresetModel::migrateOldPresets(
	StateDatabase &state, const QString &oldPresetsPath)
{
	state.tx([&](StateDatabase::Query &qry) {
		QString key =
			QStringLiteral("hostpresetmodel/presetsmigratedfromfiles");
		if(qry.get(key).toBool()) {
			return true;
		} else {
			return qry.put(key, true) && loadOldPresets(qry, oldPresetsPath);
		}
	});
}

bool HostPresetModel::loadOldPresets(
	StateDatabase::Query &qry, const QString &oldPresetsPath)
{
	QDir dir(oldPresetsPath);
	QStringList entries = dir.entryList(
		{QStringLiteral("*.preset")}, QDir::Files | QDir::Readable, QDir::Name);
	if(!entries.isEmpty()) {
		if(!qry.prepare(
			   QStringLiteral("insert into host_presets (version, title, data)"
							  "values (?, ?, ?)"))) {
			return false;
		}

		qry.bindValue(0, 1);

		for(const QString &entry : entries) {
			QString title = entry;
			title.truncate(entry.lastIndexOf('.'));
			title = title.trimmed();
			QByteArray data;
			if(!title.isEmpty() && loadOldPreset(dir.filePath(entry), data)) {
				qry.bindValue(1, title);
				qry.bindValue(2, data);
				if(!qry.execPrepared()) {
					return false;
				}
			}
		}
	}
	return true;
}

bool HostPresetModel::loadOldPreset(const QString &path, QByteArray &outData)
{
	QByteArray bytes;
	QString error;
	if(!paths::slurp(path, bytes, error)) {
		qWarning(
			"Could not read old preset '%s': %s", qUtf8Printable(path),
			qUtf8Printable(error));
		return false;
	}

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if(parseError.error != QJsonParseError::NoError) {
		qWarning(
			"Could not parse old preset '%s': %s", qUtf8Printable(path),
			qUtf8Printable(parseError.errorString()));
		return false;
	}

	if(!doc.isObject()) {
		qWarning("Old preset '%s' data is not an object", qUtf8Printable(path));
		return false;
	}

	QPair<QString, int> keyMappings[] = {
		{QStringLiteral("permPutImage"), int(DP_FEATURE_PUT_IMAGE)},
		{QStringLiteral("permRegionMove"), int(DP_FEATURE_REGION_MOVE)},
		{QStringLiteral("permBackground"), int(DP_FEATURE_BACKGROUND)},
		{QStringLiteral("permResize"), int(DP_FEATURE_RESIZE)},
		{QStringLiteral("permEditLayers"), int(DP_FEATURE_EDIT_LAYERS)},
		{QStringLiteral("permOwnLayers"), int(DP_FEATURE_OWN_LAYERS)},
		{QStringLiteral("permCreateAnnotation"),
		 int(DP_FEATURE_CREATE_ANNOTATION)},
		{QStringLiteral("permLaser"), int(DP_FEATURE_LASER)},
		{QStringLiteral("permUndo"), int(DP_FEATURE_UNDO)},
		{QStringLiteral("permTimeline"), int(DP_FEATURE_TIMELINE)},
		{QStringLiteral("permMyPaint"), int(DP_FEATURE_MYPAINT)},
		{QStringLiteral("deputies"), -1},
	};
	QJsonObject permissions;
	for(const QPair<QString, int> &p : keyMappings) {
		QJsonValue value = doc[p.first];
		if(value.isDouble()) {
			QString name = p.second < 0
							   ? QStringLiteral("deputies")
							   : QString::fromUtf8(DP_feature_name(p.second));
			permissions.insert(name, value.toDouble());
		}
	}

	QJsonObject data = {{QStringLiteral("permissions"), permissions}};
	outData = QJsonDocument(data).toJson(QJsonDocument::Compact);
	return true;
}

void HostPresetModel::loadPresets()
{
	QString sql = QStringLiteral(
		"select id, version, title, data from host_presets order by id");
	StateDatabase::Query qry = m_state.query();
	if(qry.exec(sql)) {
		while(qry.next()) {
			int id = qry.value(0).toInt();
			int version = qry.value(1).toInt();
			if(version != 1) {
				qWarning("Preset %d has unsupported version %d", id, version);
				continue;
			}

			QJsonParseError parseError;
			QJsonDocument doc = QJsonDocument::fromJson(
				qry.value(3).toByteArray(), &parseError);
			if(parseError.error != QJsonParseError::NoError) {
				qWarning(
					"Failed to parse data of preset %d: %s", id,
					qUtf8Printable(parseError.errorString()));
				continue;
			}

			if(!doc.isObject()) {
				qWarning("Data of preset %d is not an object", id);
				continue;
			}

			m_presets.append(
				{id, version, qry.value(2).toString(), doc.object()});
		}
	}
}

}
