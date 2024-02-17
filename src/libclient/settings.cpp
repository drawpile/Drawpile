// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/settings.h"
#include "libshared/util/paths.h"
#include "libshared/util/qtcompat.h"

#include <QAssociativeIterable>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMetaType>
#include <QMutexLocker>
#include <QSequentialIterable>
#include <QSet>
#include <QStandardPaths>
#include <QtMath>
#include <algorithm>
#include <utility>

Q_LOGGING_CATEGORY(lcDpSettings, "net.drawpile.settings", QtWarningMsg)

template <typename From, typename To, typename Fn>
static void registerConverter(Fn &&fn)
{
	const auto ok = QMetaType::registerConverter<From, To>(std::forward<Fn>(fn));
	if (!ok) {
		qCWarning(lcDpSettings)
			<< "could not register converter from"
			<< QMetaType::fromType<From>().name()
			<< "to"
			<< QMetaType::fromType<To>().name();
	}
}

template <typename Value>
static void registerListConverter()
{
	registerConverter<QVariantList, QVector<Value>>([](const QVariantList &list)
	{
		QVector<Value> result;
		for (const auto &value : list) {
			if (!value.canConvert<Value>()) {
				qCWarning(lcDpSettings)
					<< "could not convert" << value << "to"
					<< QMetaType::fromType<Value>().name();
			}
			result.append(value.value<Value>());
		}
		return result;
	});
}

template <typename Value>
static void registerMapConverter()
{
	registerConverter<QVariantMap, QMap<QString, Value>>([](const QVariantMap &map)
	{
		QMap<QString, Value> result;
		for (auto entry = map.begin(); entry != map.end(); ++entry) {
			const auto value = entry.value();
			if (!value.canConvert<Value>()) {
				qCWarning(lcDpSettings)
					<< "could not convert" << value << "to"
					<< QMetaType::fromType<Value>().name();
			}
			result.insert(entry.key(), value.value<Value>());
		}
		return result;
	});
}

static void registerConverters()
{
	registerListConverter<QVariantMap>();
	registerMapConverter<bool>();
	registerMapConverter<int>();
	registerMapConverter<QHash<QString, QVariant>>();
	registerConverter<QVariantMap, QMap<int, int>>([](const QVariantMap &map) {
		QMap<int, int> result;
		for (auto entry = map.begin(); entry != map.end(); ++entry) {
			bool ok;
			const auto key = entry.key().toInt(&ok);
			if (!ok) {
				qCWarning(lcDpSettings) << "could not convert" << key << "to int";
			}
			const auto value = entry.value();
			if (!value.canConvert<int>()) {
				qCWarning(lcDpSettings) << "could not convert" << value << "to int";
			}
			result.insert(key, value.toInt());
		}
		return result;
	});
}

Q_COREAPP_STARTUP_FUNCTION(registerConverters)

namespace libclient {
namespace settings {

const QVector<qreal> &zoomLevels()
{
	static QVector<qreal> levels;
	if(levels.isEmpty()) {
		// SPDX-SnippetBegin
		// SPDX-License-Identifier: GPL-3.0-or-later
		// SDPX—SnippetName: zoom steps calculation from Krita
		int steps = 2;
		qreal k = steps / M_LN2;

		int first = ceil(log(zoomMin) * k);
		int size = floor(log(zoomMax) * k) - first + 1;
		levels.resize(size);

		// enforce zoom levels relating to thirds (33.33%, 66.67%, ...)
		QVector<qreal> snap(steps);
		if(steps > 1) {
			qreal third = log(4.0 / 3.0) * k;
			int i = round(third);
			snap[(i - first) % steps] = third - i;
		}

		k = 1.0 / k;
		for(int i = 0; i < steps; i++) {
			qreal f = exp((i + first + snap[i]) * k);
			f = floor(f * 0x1p48 + 0.5) / 0x1p48; // round off inaccuracies
			for(int j = i; j < size; j += steps, f *= 2.0) {
				levels[j] = f;
			}
		}
		// SPDX-SnippetEnd
	}
	return levels;
}

Settings::Settings(QObject *parent)
	: QObject(parent)
{
	reset();
}

Settings::~Settings()
{
	if(!m_pending.isEmpty()) {
		qWarning("Programming error: pending settings on destruction");
		trySubmit();
	}
}

static void cacheGroups(QSettings &settings, QSet<QString> &groupKeys, const QString &prefix = QString())
{
	for (const auto &group : settings.childGroups()) {
		QString key = prefix + group;
		if(!groupKeys.contains(key)) {
			groupKeys.insert(key);
			settings.beginGroup(group);
			cacheGroups(settings, groupKeys, key + '/');
			settings.endGroup();
		}
	}
}

void Settings::reset(const QString &path)
{
	QMutexLocker locker{&m_mutex};
	// QSettings disables assignment operators.
	m_settings.~QSettings();
	m_resetPath = path;
	delete m_scalingSettings;
	m_scalingSettings = nullptr;

	if (!path.isEmpty()) {
		new (&m_settings) QSettings(path, QSettings::IniFormat);
	} else {
#ifdef Q_OS_WIN
		// For whatever reason Qt does not provide a QSettings constructor that
		// only takes the format, so it is necessary to do this global state
		// swap
		const auto oldFormat = QSettings::defaultFormat();
		QSettings::setDefaultFormat(QSettings::IniFormat);
		new (&m_settings) QSettings();
		QSettings::setDefaultFormat(oldFormat);
#else
		new (&m_settings) QSettings();
#endif
	}

	// We never use or want fallbacks to any system settings, especially on
	// macOS where a bunch of extra stuff from the OS is exposed in fallback
	m_settings.setFallbacksEnabled(false);

	QSet<QString> groupKeys;
	cacheGroups(m_settings, groupKeys);
	m_settings.setProperty("allGroupKeys", QVariant::fromValue(groupKeys));
}

QSettings *Settings::scalingSettings()
{
	if(!m_scalingSettings) {
		// GenericConfigLocation because of QSettings issues, see main.cpp.
		QString path =
			m_resetPath.isEmpty()
				? utils::paths::writablePath(
					  QStandardPaths::GenericConfigLocation,
					  QStringLiteral("drawpile"),
					  QStringLiteral("scaling.ini"))
				: QFileInfo(m_resetPath)
					  .dir()
					  .absoluteFilePath(QStringLiteral("scaling.ini"));
		m_scalingSettings = new QSettings(path, QSettings::IniFormat, this);
	}
	return m_scalingSettings;
}

#ifdef Q_OS_WIN
void Settings::migrateFromNativeFormat(bool force)
{
	QMutexLocker locker{&m_mutex};
	if (m_settings.allKeys().isEmpty() || force) {
		m_settings.clear();
		QSettings oldSettings;
		oldSettings.setFallbacksEnabled(false);
		for(const QString &key : oldSettings.allKeys()) {
			m_settings.setValue(key, oldSettings.value(key));
		}
	}
}
#endif

void Settings::revert()
{
	QMutexLocker locker{&m_mutex};
	const auto pending = m_pending.keys();

	// Changes must be cleared before emitting events or else they will be
	// used when the value is retrieved
	m_pending.clear();

	for (const auto *setting : pending) {
		(setting->notify)(*setting, *this);
	}
}

bool Settings::submit()
{
	QMutexLocker locker{&m_mutex};
	if(m_pending.isEmpty()) {
		return true;
	} else {
		for (auto entry = m_pending.cbegin(); entry != m_pending.cend(); ++entry) {
			const auto setting = entry.key();
			const auto &newValue = entry.value();
			(setting->set)(*setting, m_settings, newValue);
		}
		m_settings.sync();
		m_pending.clear();
		return m_settings.status() == QSettings::NoError;
	}
}

void Settings::trySubmit()
{
	if(!submit()) {
		qWarning("Error submitting settings");
	}
}

QVariant Settings::get(const SettingMeta &setting) const
{
	QMutexLocker locker{&m_mutex};
	if (m_pending.contains(&setting)) {
		return m_pending[&setting];
	} else {
		return (setting.get)(setting, m_settings);
	}
}

bool Settings::set(const SettingMeta &setting, QVariant value)
{
	QMutexLocker locker{&m_mutex};
	if (!m_pending.contains(&setting) || m_pending[&setting] != value) {
		m_pending[&setting] = value;
		(setting.notify)(setting, *this);
		return true;
	}

	return false;
}

QString formatSettingKey(const char *baseKey, int version)
{
	return version == 0
		? baseKey
		: QStringLiteral("v%1/%2").arg(version).arg(baseKey);
}

std::optional<FoundKey> findKey(QSettings &settings, const char *baseKey, SettingMeta::Version version)
{
	const auto groupKeys = settings.property("allGroupKeys").value<QSet<QString>>();
	for (auto candidate = int(version); candidate > 0; --candidate) {
		const auto versionedKey = formatSettingKey(baseKey, candidate);
		if (settings.contains(versionedKey)) {
			return {{ SettingMeta::Version(candidate), versionedKey, false }};
		}
		if (groupKeys.contains(versionedKey)) {
			return {{ SettingMeta::Version(candidate), versionedKey, true }};
		}
	}

	if (settings.contains(baseKey)) {
		return {{ SettingMeta::Version::V0, baseKey, false }};
	}
	if (groupKeys.contains(baseKey)) {
		return {{ SettingMeta::Version::V0, baseKey, true }};
	}

	return {};
}

static void ensureGroup(QSettings &settings, const QString &key)
{
	auto groupKeys = settings.property("allGroupKeys").value<QSet<QString>>();
	groupKeys.insert(key);
	settings.setProperty("allGroupKeys", QVariant::fromValue(groupKeys));
}

static void removeSetting(QSettings &settings, const QString &key)
{
	settings.remove(key);
	auto groupKeys = settings.property("allGroupKeys").value<QSet<QString>>();
	groupKeys.remove(key);
	settings.setProperty("allGroupKeys", QVariant::fromValue(groupKeys));
}

static bool hasOnlySizeChildKey(const QSettings &settings)
{
	bool hasSize = false;
	for(const QString &itemKey : settings.childKeys()) {
		if(itemKey == QStringLiteral("size")) {
			hasSize = true;
		} else {
			return false;
		}
	}
	return hasSize && settings.value(QStringLiteral("size")).canConvert<int>();
}

static bool allChildGroupsIndexed(const QSettings &settings)
{
	for (const QString &itemKey : settings.childGroups()) {
		bool ok;
		int index = itemKey.toInt(&ok);
		if(!ok || index < 1) {
			return false;
		}
	}
	return true;
}

static bool looksLikeListSetting(const QSettings &settings)
{
	return hasOnlySizeChildKey(settings) && allChildGroupsIndexed(settings);
}

namespace any {
	QVariant getGroup(QSettings &settings, const QString &key)
	{
		settings.beginGroup(key);
		// Intuit if this looks like a list. This is really fragile because of
		// how ambiguous Qt's settings format is. It should work unless there's
		// ever an entry that could have a single integer "size" key though.
		if (looksLikeListSetting(settings)) {
			settings.endGroup();
			return getList(settings, key);
		}

		QVariantMap map;
		for (const auto &itemKey : settings.childGroups()) {
			map.insert(itemKey, getGroup(settings, itemKey));
		}
		for (const auto &itemKey : settings.childKeys()) {
			map.insert(itemKey, settings.value(itemKey));
		}
		settings.endGroup();
		return map;
	}

	QVariant getList(QSettings &settings, const QString &key)
	{
		QVariantList list;
		const auto size = settings.beginReadArray(key);
		for (auto i = 1; i <= size; ++i) {
			list.append(getGroup(settings, QString::number(i)));
		}
		settings.endArray();
		return list;
	}

	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		if (const auto key = findKey(settings, meta.baseKey, meta.version)) {
			if (key->isGroup) {
				return getGroup(settings, key->key);
			} else {
				return settings.value(key->key);
			}
		} else {
			return meta.getDefaultValue();
		}
	}

	void setGroup(QSettings &settings, const QString &key, const QVariant &value)
	{
		const auto map = value.value<QAssociativeIterable>();

		if (map.begin() == map.end()) {
			removeSetting(settings, key);
			return;
		}

		settings.beginGroup(key);
		QSet<QString> keysToDelete;
		for(const QString &childKey : settings.childKeys()) {
			keysToDelete.insert(childKey);
		}
		for(const QString &childGroupKey : settings.childGroups()) {
			keysToDelete.insert(childGroupKey);
		}

		for (auto entry = map.begin(); entry != map.end(); ++entry) {
			QString mapKey = entry.key().toString();
			keysToDelete.remove(mapKey);

			QVariant mapValue = entry.value();
			if (mapValue.canConvert<QVariantList>()) {
				setList(settings, mapKey, mapValue);
			} else if (mapValue.canConvert<QVariantMap>()) {
				setGroup(settings, mapKey, mapValue);
			} else {
				settings.setValue(mapKey, mapValue);
			}
		}

		for(const QString &keyToDelete : keysToDelete) {
			settings.remove(keyToDelete);
		}

		settings.endGroup();
		ensureGroup(settings, key);
	}

	void setList(QSettings &settings, const QString &key, const QVariant &value)
	{
		const auto list = value.value<QSequentialIterable>();

		const auto size = list.size();
		if (size == 0) {
			removeSetting(settings, key);
			return;
		}

		// QSettings only really supports lists of maps with `beginWriteArray`
		// because it expects authors to use `setArrayIndex` for the index and
		// then `setValue` requires a key, so if the list is not a list of maps
		// we should just store it directly
		if (!list.at(0).canConvert<QVariantMap>() && !list.at(0).canConvert<QVariantHash>()) {
			settings.setValue(key, value);
			return;
		}

		settings.beginWriteArray(key, size);
		auto i = 1;
		for (const auto &itemValue : list) {
			const auto itemKey = QString::number(i++);
			setGroup(settings, itemKey, itemValue);
		}
		settings.endArray();
		ensureGroup(settings, key);
	}

	void forceSetKey(QSettings &settings, const QString &key, QVariant value)
	{
		// `QString` is convertible to `QStringList` for some reason,
		// which is obviously not desired
		if (compat::metaTypeFromVariant(value) != QMetaType::QString
			&& compat::metaTypeFromVariant(value) != QMetaType::QByteArray
			&& value.canConvert<QVariantList>()
		) {
			qCDebug(lcDpSettings) << "set list" << key;
			setList(settings, key, value);
		} else if (value.canConvert<QVariantMap>() || value.canConvert<QVariantHash>()) {
			qCDebug(lcDpSettings) << "set group" << key;
			setGroup(settings, key, value);
		} else {
			qCDebug(lcDpSettings) << "set value" << key << "to" << value;
			settings.setValue(key, value);
		}
	}

	void forceSet(const SettingMeta &meta, QSettings &settings, QVariant value)
	{
		const auto key = formatSettingKey(meta.baseKey, int(meta.version));
		forceSetKey(settings, key, value);
	}

	void set(const SettingMeta &meta, QSettings &settings, QVariant value)
	{
		const auto key = formatSettingKey(meta.baseKey, int(meta.version));
		if (meta.version != SettingMeta::Version::V0 || value != meta.getDefaultValue()) {
			forceSetKey(settings, key, value);
		} else {
			qCDebug(lcDpSettings) << "remove" << key;
			removeSetting(settings, key);
		}
	}
}

#define DP_SETTINGS_BODY
#include "libclient/settings_table.h"
#undef DP_SETTINGS_BODY

void initializeTypes()
{
#define DP_SETTINGS_INIT
#include "libclient/settings_table.h"
#undef DP_SETTINGS_INIT
}

} // namespace settings
} // namespace libclient
