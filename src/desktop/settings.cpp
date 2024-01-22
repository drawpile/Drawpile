// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/settings.h"
#include "libshared/util/qtcompat.h"

namespace desktop {
namespace settings {

Settings::Settings(QObject *parent)
	: libclient::settings::Settings::Settings(parent)
{
#ifndef HAVE_QT_COMPAT_QVARIANT_ENUM
	qRegisterMetaTypeStreamOperators<widgets::CanvasView::BrushCursor>("widgets::CanvasView::BrushCursor");
	qRegisterMetaTypeStreamOperators<color_widgets::ColorWheel::AngleEnum>("color_widgets::ColorWheel::AngleEnum");
	qRegisterMetaTypeStreamOperators<color_widgets::ColorWheel::ColorSpaceEnum>("color_widgets::ColorWheel::ColorSpaceEnum");
	qRegisterMetaTypeStreamOperators<color_widgets::ColorWheel::ShapeEnum>("color_widgets::ColorWheel::ShapeEnum");
	qRegisterMetaTypeStreamOperators<tabletinput::Mode>("tabletinput::Mode");
	qRegisterMetaTypeStreamOperators<ThemePalette>("desktop::settings::ThemePalette");
	qRegisterMetaTypeStreamOperators<tools::Tool::Type>("tools::Tool::Type");
	qRegisterMetaTypeStreamOperators<VideoExporter::Format>("VideoExporter::Format");
#endif
}

using libclient::settings::SettingMeta;
using libclient::settings::findKey;
namespace any = libclient::settings::any;

namespace debounceDelayMs {
	int bounded(const QVariant &value)
	{
		bool ok;
		int i = value.toInt(&ok);
		return ok ? qBound(20, i, 1000) : DEBOUNCE_DELAY_MS_DEFAULT;
	}

	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		return bounded(any::get(meta, settings));
	}

	void set(const SettingMeta &meta, QSettings &settings, QVariant value)
	{
		any::set(meta, settings, bounded(value));
	}
}

namespace onionSkinsFrames {
	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		if (const auto key = findKey(settings, meta.baseKey, meta.version)) {
			return any::getGroup(settings, key->key);
		} else if (findKey(settings, "onionskins/frame1", SettingMeta::Version::V0)) {
			Settings::OnionSkinsFramesType frames;
			const auto size = settings.value("onionskins/framecount").toInt();
			for (auto i = 0; i < size; ++i) {
				frames.insert(i, settings.value(QStringLiteral("frame%1").arg(i)).toInt());
			}
			return QVariant::fromValue(frames);
		} else {
			return meta.defaultValue;
		}
	}

	void set(const SettingMeta &meta, QSettings &settings, QVariant value)
	{
		any::forceSet(meta, settings, value);

		if (findKey(settings, "onionskins/frame1", SettingMeta::Version::V0)) {
			const auto frames = value.value<Settings::OnionSkinsFramesType>();
			for (auto entry = frames.cbegin(); entry != frames.cend(); ++entry) {
				settings.setValue(QStringLiteral("frame%1").arg(entry.key()), entry.value());
			}
		}
	}
}

namespace newCanvasSize {
	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		return any::get(meta, settings)
			.toSize()
			.expandedTo({ 100, 100 })
			.boundedTo({ 65535, 65535 });
	}
}

namespace newCanvasBackColor {
	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		const auto color = any::get(meta, settings).value<QColor>();
		if (!color.isValid()) {
			return meta.defaultValue;
		}
		return color;
	}
}

namespace tabletDriver {
	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		using tabletinput::Mode;

		auto mode = meta.defaultValue.value<Mode>();
		if (findKey(settings, meta.baseKey, meta.version)) {
			mode = any::get(meta, settings).value<Mode>();
		} else if (const auto inkKey = findKey(settings, "settings/input/windowsink", SettingMeta::Version::V0)) {
			const auto useInk = settings.value(inkKey->key).toBool();
			const auto modeKey = findKey(settings, "settings/input/relativepenhack", SettingMeta::Version::V0);
			const auto relativeMode = modeKey
				? settings.value(modeKey->key).toBool()
				: false;

			mode = Mode::KisTabletWintab;
			if (relativeMode) {
				mode = Mode::KisTabletWintabRelativePenHack;
			} else if (useInk) {
				mode = Mode::KisTabletWinink;
			}
		}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		if (mode == Mode::Qt5) {
			mode = Mode::Qt6Winink;
		}
#else
		if (mode == Mode::Qt6Winink || mode == Mode::Qt6Wintab) {
			mode = Mode::Qt5;
		}
#endif

		return QVariant::fromValue(mode);
	}

	void set(const SettingMeta &meta, QSettings &settings, QVariant value)
	{
		any::forceSet(meta, settings, value);

		if (const auto oldKey = findKey(settings, "settings/input/windowsink", SettingMeta::Version::V0)) {
			const auto mode = value.value<tabletinput::Mode>();
			auto useInk = false;
			auto relativeMode = false;
			switch (mode) {
			case tabletinput::Mode::KisTabletWintabRelativePenHack:
				relativeMode = true;
				break;
			case tabletinput::Mode::Qt6Winink:
			case tabletinput::Mode::KisTabletWinink:
				useInk = true;
				break;
			default: {}
			}
			settings.setValue(oldKey->key, useInk);
			settings.setValue("settings/input/relativepenhack", relativeMode);
		}
	}
}

namespace tabletEraserAction {
QVariant get(const SettingMeta &meta, QSettings &settings)
{
	using tabletinput::EraserAction;

	int action;
	if(findKey(settings, meta.baseKey, meta.version)) {
		action = any::get(meta, settings).toInt();
	} else {
		std::optional<libclient::settings::FoundKey> oldKey = findKey(
			settings, "settings/input/tableteraser", SettingMeta::Version::V0);
		if(oldKey) {
			action = settings.value(oldKey->key).toBool()
						 ? int(EraserAction::Default)
						 : int(EraserAction::Ignore);
		} else {
			action = meta.defaultValue.toInt();
		}
	}

	switch(action) {
	case int(EraserAction::Ignore):
#ifndef __EMSCRIPTEN__
	case int(EraserAction::Switch):
#endif
	case int(EraserAction::Override):
		return action;
	default:
		return int(EraserAction::Default);
	}
}

void set(const SettingMeta &meta, QSettings &settings, QVariant value)
{
	using tabletinput::EraserAction;

	any::forceSet(meta, settings, value);

	std::optional<libclient::settings::FoundKey> oldKey = findKey(
		settings, "settings/input/tableteraser", SettingMeta::Version::V0);
	if(oldKey) {
		settings.setValue(
			oldKey->key, value.toInt() != int(EraserAction::Ignore));
	}
}
}

namespace themeStyle {
	// Changing the theme style can cause the theme palette to change too
	void notify(const SettingMeta &, libclient::settings::Settings &base) {
		auto &settings = static_cast<Settings &>(base);
		settings.themeStyleChanged(settings.themeStyle());
		settings.themePaletteChanged(settings.themePalette());
	}
}

#define DP_SETTINGS_BODY
#include "desktop/settings_table.h"
#undef DP_SETTINGS_BODY

} // namespace settings
} // namespace desktop
