// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/settings.h"
#include "libshared/util/qtcompat.h"

namespace desktop {
namespace settings {

#ifdef __EMSCRIPTEN__
QString globalPressureCurveDefault = QStringLiteral("0,0;1,1;");
#endif

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
			return meta.getDefaultValue();
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
			return meta.getDefaultValue();
		}
		return color;
	}
}

namespace oneFingerTouch {
	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		if (findKey(settings, meta.baseKey, meta.version)) {
			bool ok;
			int value = any::get(meta, settings).toInt(&ok);
			if(ok) {
				switch(value) {
				case int(OneFingerTouchAction::Nothing):
				case int(OneFingerTouchAction::Draw):
				case int(OneFingerTouchAction::Pan):
				case int(OneFingerTouchAction::Guess):
					return value;
				default:
					break;
				}
			}
			return int(ONE_FINGER_TOUCH_DEFAULT);
		}

		std::optional<libclient::settings::FoundKey> touchDrawKey = findKey(
			settings, "settings/input/touchdraw", SettingMeta::Version::V0);
		if(touchDrawKey.has_value() && settings.value(touchDrawKey->key).toBool()) {
			return int(OneFingerTouchAction::Draw);
		}

		std::optional<libclient::settings::FoundKey> touchScrollKey = findKey(
			settings, "settings/input/touchscroll", SettingMeta::Version::V0);
		if(touchScrollKey.has_value() && settings.value(touchScrollKey->key).toBool()) {
			return int(OneFingerTouchAction::Pan);
		}

		return int(ONE_FINGER_TOUCH_DEFAULT);
	}
}

namespace twoFingerPinch {
	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		if (findKey(settings, meta.baseKey, meta.version)) {
			bool ok;
			int value = any::get(meta, settings).toInt(&ok);
			if(ok) {
				switch(value) {
				case int(TwoFingerPinchAction::Nothing):
				case int(TwoFingerPinchAction::Zoom):
					return value;
				default:
					break;
				}
			}
			return int(TwoFingerPinchAction::Zoom);
		}

		std::optional<libclient::settings::FoundKey> touchPinchKey = findKey(
			settings, "settings/input/touchpinch", SettingMeta::Version::V0);
		if(touchPinchKey.has_value() &&
		settings.value(touchPinchKey->key).toBool()) {
			return settings.value(touchPinchKey->key).toBool()
					? int(TwoFingerPinchAction::Zoom)
					: int(TwoFingerPinchAction::Nothing);
		}

		return int(TwoFingerPinchAction::Zoom);
	}
}

namespace twoFingerTwist {
	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		if (findKey(settings, meta.baseKey, meta.version)) {
			bool ok;
			int value = any::get(meta, settings).toInt(&ok);
			if(ok) {
				switch(value) {
				case int(TwoFingerTwistAction::Nothing):
				case int(TwoFingerTwistAction::Rotate):
				case int(TwoFingerTwistAction::RotateNoSnap):
				case int(TwoFingerTwistAction::RotateDiscrete):
					return value;
				default:
					break;
				}
			}
			return int(TwoFingerTwistAction::Rotate);
		}

		std::optional<libclient::settings::FoundKey> touchTwistKey = findKey(
			settings, "settings/input/touchtwist", SettingMeta::Version::V0);
		if(touchTwistKey.has_value() &&
		settings.value(touchTwistKey->key).toBool()) {
			return settings.value(touchTwistKey->key).toBool()
					? int(TwoFingerTwistAction::Rotate)
					: int(TwoFingerTwistAction::Nothing);
		}

		return int(TwoFingerTwistAction::Rotate);
	}
}

namespace tabletDriver {
	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		using tabletinput::Mode;

		auto mode = meta.getDefaultValue().value<Mode>();
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
			action = meta.getDefaultValue().toInt();
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

namespace themePalette {
	void set(const SettingMeta &meta, QSettings &settings, QVariant value)
	{
		using Key = std::optional<libclient::settings::FoundKey>;
		any::forceSet(meta, settings, value);
		if (Key oldKey = findKey(settings, meta.baseKey, SettingMeta::Version::V2)) {
			ThemePalette oldValue = value.value<ThemePalette>();
			if(int(oldValue) > int(ThemePalette::HotdogStand)) {
				oldValue = THEME_PALETTE_DEFAULT;
			}
			settings.setValue(oldKey->key, QVariant::fromValue(oldValue));
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

namespace viewCursor {
	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		using Key = std::optional<libclient::settings::FoundKey>;
		if (Key currentKey = findKey(settings, meta.baseKey, meta.version)) {
			return settings.value(currentKey->key).toInt();
		} else if (Key oldKey = findKey(settings, meta.baseKey, SettingMeta::Version::V0)) {
			return int(settings.value(currentKey->key).value<widgets::CanvasView::BrushCursor>());
		} else {
			return meta.getDefaultValue().toInt();
		}
	}

	void set(const SettingMeta &meta, QSettings &settings, QVariant value)
	{
		using Key = std::optional<libclient::settings::FoundKey>;
		any::forceSet(meta, settings, value);
		if (Key oldKey = findKey(settings, meta.baseKey, SettingMeta::Version::V0)) {
			settings.setValue(oldKey->key, QVariant::fromValue(widgets::CanvasView::BrushCursor(value.toInt())));
		}
	}
}

#define DP_SETTINGS_BODY
#include "desktop/settings_table.h"
#undef DP_SETTINGS_BODY

void initializeTypes()
{
	libclient::settings::initializeTypes();
#define DP_SETTINGS_INIT
#include "desktop/settings_table.h"
#undef DP_SETTINGS_INIT
}

} // namespace settings
} // namespace desktop
