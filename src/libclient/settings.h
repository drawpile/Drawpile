// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_SETTINGS_H
#define LIBCLIENT_SETTINGS_H
#include "cmake-config/config.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/config/config.h"
#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libclient/view/enums.h"
#include "libshared/net/messagequeue.h"
#include <QDebug>
#include <QHash>
#include <QLoggingCategory>
#include <QRecursiveMutex>
#include <QSettings>
#include <QVariant>
#include <memory>
#include <optional>

Q_DECLARE_LOGGING_CATEGORY(lcDpSettings)

namespace libclient {
Q_NAMESPACE
namespace settings {
Q_NAMESPACE

// For avoiding `-Wuseless-cast` in disgusting macro-generated default code
template <typename To, typename From> To maybe_static_cast(From &from)
{
	if constexpr(std::is_same_v<To, From>) {
		return from;
	} else {
		return static_cast<To>(from);
	}
}

class Settings;

struct SettingMeta final {
	enum class Version : int {
		V0 = 0,
		V1,
		V2,
		V3,
		V4,
	};

	Version version;
	const char *baseKey;
	QVariant (*const getDefaultValue)();
	QVariant (*const get)(const SettingMeta &, QSettings &);
	void (*const set)(const SettingMeta &, QSettings &, QVariant);
	void (*const notify)(const SettingMeta &, Settings &);
};

/**
 * An interface to application-wide settings.
 */
class Settings : public QObject {
	Q_OBJECT
public:
	Settings(QObject *parent = nullptr);
	~Settings() override;

	void reset(const QString &path = QString());

	QString path() const { return m_settings.fileName(); }

	QSettings *scalingSettings();

#define DP_SETTINGS_HEADER
#include "libclient/settings_table.h"
#undef DP_SETTINGS_HEADER

	void revert();
	bool submit();
	void trySubmit();

protected:
	QVariant get(const SettingMeta &setting) const;
	bool set(const SettingMeta &setting, QVariant value);

	mutable QSettings m_settings; // Annoying group interface requires mutable.
	QHash<const SettingMeta *, QVariant> m_pending;
	mutable QRecursiveMutex m_mutex;
	QString m_resetPath;
	QSettings *m_scalingSettings = nullptr;
};

QString formatSettingKey(const char *baseKey, int version);

struct FoundKey {
	SettingMeta::Version version;
	QString key;
	bool isGroup;
};

std::optional<FoundKey>
findKey(QSettings &settings, const char *baseKey, SettingMeta::Version version);

std::optional<FoundKey> findKeyExactVersion(
	QSettings &settings, const char *baseKey, SettingMeta::Version version);

template <typename T>
QVariant getEnumReplacedByInt(const SettingMeta &meta, QSettings &settings)
{
	std::optional<libclient::settings::FoundKey> intFoundKey =
		findKeyExactVersion(settings, meta.baseKey, meta.version);
	if(intFoundKey.has_value()) {
		return settings.value(intFoundKey->key);
	}

	std::optional<libclient::settings::FoundKey> enumFoundKey = findKey(
		settings, meta.baseKey, SettingMeta::Version(int(meta.version) - 1));
	if(enumFoundKey.has_value()) {
		QVariant enumValue = settings.value(enumFoundKey->key);
		return int(enumValue.value<T>());
	}

	return meta.getDefaultValue();
}

// These are exposed in the header only because libclient/desktop are split
// so desktop-only settings need access to these too.
namespace any {
QVariant get(const SettingMeta &meta, QSettings &settings);
QVariant getExactVersion(const SettingMeta &meta, QSettings &settings);
QVariant getGroup(QSettings &settings, const QString &key);
QVariant getList(QSettings &settings, const QString &key);
void forceSetKey(QSettings &settings, const QString &key, QVariant value);
void forceSet(const SettingMeta &meta, QSettings &settings, QVariant value);
void set(const SettingMeta &meta, QSettings &settings, QVariant value);
void setGroup(QSettings &settings, const QString &key, const QVariant &value);
void setList(QSettings &settings, const QString &key, const QVariant &value);
}

void initializeTypes();

}
}

#endif
