// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBCLIENT_SETTINGS_H
#define LIBCLIENT_SETTINGS_H

#include "cmake-config/config.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/parentalcontrols/parentalcontrols.h"

#include <QDebug>
#include <QHash>
#include <QLoggingCategory>
#include <QSettings>
#include <QVariant>
#include <memory>
#include <optional>

Q_DECLARE_LOGGING_CATEGORY(lcDpSettings)

namespace libclient {
namespace settings {

// For avoiding `-Wuseless-cast` in disgusting macro-generated default code
template <typename To, typename From>
To maybe_static_cast(From &from)
{
	if constexpr (std::is_same_v<To, From>) {
		return from;
	} else {
		return static_cast<To>(from);
	}
}

// On most platforms, tablet input comes at a very high precision and frequency,
// so some smoothing is sensible by default. On Android (at least on a Samsung
// Galaxy S6 Lite, a Samsung Galaxy S8 Ultra and reports from unknown devices)
// the input is already pretty smooth though, so we'll leave it at zero there.
#ifdef Q_OS_ANDROID
inline constexpr int defaultSmoothing = 0;
#else
inline constexpr int defaultSmoothing = 3;
#endif

class Settings;

struct SettingMeta final {
	enum class Version : int {
		V0 = 0,
		V1,
		V2,
	};

	Version version;
	const char *baseKey;
	QVariant defaultValue;
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

	void reset(const QString &path = QString());

	QString path() const { return m_settings.fileName(); }

	bool autoCommit() const { return m_autoCommit; }
	void setAutoCommit(bool autoCommit) { m_autoCommit = autoCommit; }

#ifdef Q_OS_WIN
	void migrateFromNativeFormat(bool force);
#endif

#	define DP_SETTINGS_HEADER
#	include "libclient/settings_table.h"
#	undef DP_SETTINGS_HEADER

public slots:
	void revert();
	bool submit();

protected:
	QVariant get(const SettingMeta &setting) const;
	bool set(const SettingMeta &setting, QVariant value);

	template <
		typename T,
		typename Object,
		typename Receiver,
		typename Signal = std::nullptr_t,
		typename Self,
		typename = std::enable_if_t<
			std::is_invocable_v<Receiver, Object *, T>
			|| std::is_invocable_v<Receiver, Object *>
			|| std::is_invocable_v<Receiver, T>
			|| std::is_invocable_v<Receiver>
		>
	>
	inline auto bind(
		T value, void (Self::*changed)(T), void (Self::*setter)(T),
		Object *object, Receiver receiver, Signal signal = nullptr
	)
	{
		static_assert(std::is_base_of_v<Settings, Self>);

		if constexpr (
			std::is_member_function_pointer_v<Receiver>
			&& std::is_invocable_v<Receiver, Object *, T>
		) {
			std::invoke(receiver, object, value);
		} else if constexpr (
			std::is_member_function_pointer_v<Receiver>
			&& std::is_invocable_v<Receiver, Object *>
		) {
			std::invoke(receiver, object);
		} else if constexpr (std::is_invocable_v<Receiver, T>) {
			std::invoke(receiver, value);
		} else {
			std::invoke(receiver);
		}

		const auto connection = connect(static_cast<Self *>(this), changed, object, receiver);

		if constexpr (
			std::is_member_function_pointer_v<Signal>
			&& std::is_invocable_v<Signal, Object *, T>
		) {
			return std::pair {
				connection,
				connect(object, signal, static_cast<Self *>(this), setter)
			};
		} else {
			return connection;
		}
	}

	template <
		typename T,
		typename Receiver,
		typename Self,
		typename = std::enable_if_t<
			std::is_invocable_v<Receiver, T>
			|| std::is_invocable_v<Receiver>
		>
	>
	inline auto bind(T value, void (Self::*changed)(T), void (Self::*)(T), Receiver receiver)
	{
		static_assert(std::is_base_of_v<Settings, Self>);
		if constexpr (std::is_invocable_v<Receiver, T>) {
			std::invoke(receiver, value);
		} else {
			std::invoke(receiver);
		}
		return connect(static_cast<Self *>(this), changed, receiver);
	}

	template <
		typename U,
		typename T,
		typename Object,
		typename Receiver,
		typename Signal = std::nullptr_t,
		typename Self,
		typename = std::enable_if_t<
			std::is_invocable_v<Receiver, Object *, U>
			|| std::is_invocable_v<Receiver, U>
		>
	>
	inline auto bindAs(T initialValue, void (Self::*changed)(T), void (Self::*setter)(T), Object *object, Receiver receiver, Signal signal = nullptr)
	{
		static_assert(std::is_base_of_v<Settings, Self>);

		QMetaObject::Connection connection;

		if constexpr (
			std::is_member_function_pointer_v<Receiver>
			&& std::is_invocable_v<Receiver, Object *, U>
		) {
			std::invoke(receiver, object, U(initialValue));

			connection = connect(static_cast<Self *>(this), changed, object, [=](T value) {
				std::invoke(receiver, object, U(value));
			});
		} else {
			std::invoke(receiver, initialValue);

			connection = connect(static_cast<Self *>(this), changed, object, [=](T value) {
				std::invoke(receiver, U(value));
			});
		}

		if constexpr (
			std::is_member_function_pointer_v<Signal>
			&& std::is_invocable_v<Signal, Object *, U>
		) {
			return std::pair {
				connection,
				connect(object, signal, static_cast<Self *>(this), [=](U value) {
					std::invoke(setter, static_cast<Self *>(this), T(value));
				})
			};
		} else {
			return connection;
		}
	}

	bool m_autoCommit = true;
	mutable QSettings m_settings; // Annoying group interface requires mutable.
	QHash<const SettingMeta *, QVariant> m_pending;
};

QString formatSettingKey(const char *baseKey, int version);

struct FoundKey {
	SettingMeta::Version version;
	QString key;
	bool isGroup;
};

std::optional<FoundKey> findKey(QSettings &settings, const char *baseKey, SettingMeta::Version version);

// These are exposed in the header only because libclient/desktop are split
// so desktop-only settings need access to these too.
namespace any {
	QVariant get(const SettingMeta &meta, QSettings &settings);
	QVariant getGroup(QSettings &settings, const QString &key);
	QVariant getList(QSettings &settings, const QString &key);
	void set(const SettingMeta &meta, QSettings &settings, QVariant value);
	void setGroup(QSettings &settings, const QString &key, const QVariant &value);
	void setList(QSettings &settings, const QString &key, const QVariant &value);
} // namespace any

} // namespace settings
} // namespace libclient

#endif
