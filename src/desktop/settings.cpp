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

namespace {
template <typename T>
static QVariant getV0EnumToV1Int(const SettingMeta &meta, QSettings &settings)
{
	std::optional<libclient::settings::FoundKey> intFoundKey =
		findKeyExactVersion(settings, meta.baseKey, meta.version);
	if (intFoundKey.has_value()) {
		return settings.value(intFoundKey->key);
	}

	std::optional<libclient::settings::FoundKey> enumFoundKey =
		findKeyExactVersion(settings, meta.baseKey, SettingMeta::Version::V0);
	if (enumFoundKey.has_value()) {
		QVariant enumValue = settings.value(enumFoundKey->key);
		return int(enumValue.value<T>());
	}

	return meta.getDefaultValue();
}
}

namespace colorWheelAngle {
	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		return getV0EnumToV1Int<color_widgets::ColorWheel::AngleEnum>(meta, settings);
	}
}

namespace colorWheelShape {
	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		return getV0EnumToV1Int<color_widgets::ColorWheel::ShapeEnum>(meta, settings);
	}
}

namespace colorWheelSpace {
	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		return getV0EnumToV1Int<color_widgets::ColorWheel::ColorSpaceEnum>(meta, settings);
	}
}

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

namespace lastHostServer {
	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		std::optional<libclient::settings::FoundKey> lastHostServer =
			findKey(settings, meta.baseKey, meta.version);
		if (lastHostServer.has_value()) {
			return settings.value(lastHostServer->key);
		}

		std::optional<libclient::settings::FoundKey> hostRemoteKey =
			findKey(settings, "history/hostremote", SettingMeta::Version::V0);
		if (hostRemoteKey.has_value() && !settings.value(hostRemoteKey->key).toBool()) {
			return 2;
		}

		return 1;
	}
}

namespace lastTool {
	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		return getV0EnumToV1Int<tools::Tool::Type>(meta, settings);
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
				case int(view::OneFingerTouchAction::Nothing):
				case int(view::OneFingerTouchAction::Draw):
				case int(view::OneFingerTouchAction::Pan):
				case int(view::OneFingerTouchAction::Guess):
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
			return int(view::OneFingerTouchAction::Draw);
		}

		std::optional<libclient::settings::FoundKey> touchScrollKey = findKey(
			settings, "settings/input/touchscroll", SettingMeta::Version::V0);
		if(touchScrollKey.has_value() && settings.value(touchScrollKey->key).toBool()) {
			return int(view::OneFingerTouchAction::Pan);
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
				case int(view::TwoFingerPinchAction::Nothing):
				case int(view::TwoFingerPinchAction::Zoom):
					return value;
				default:
					break;
				}
			}
			return int(view::TwoFingerPinchAction::Zoom);
		}

		std::optional<libclient::settings::FoundKey> touchPinchKey = findKey(
			settings, "settings/input/touchpinch", SettingMeta::Version::V0);
		if(touchPinchKey.has_value() &&
		settings.value(touchPinchKey->key).toBool()) {
			return settings.value(touchPinchKey->key).toBool()
					? int(view::TwoFingerPinchAction::Zoom)
					: int(view::TwoFingerPinchAction::Nothing);
		}

		return int(view::TwoFingerPinchAction::Zoom);
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
				case int(view::TwoFingerTwistAction::Nothing):
				case int(view::TwoFingerTwistAction::Rotate):
				case int(view::TwoFingerTwistAction::RotateNoSnap):
				case int(view::TwoFingerTwistAction::RotateDiscrete):
					return value;
				default:
					break;
				}
			}
			return int(view::TwoFingerTwistAction::Rotate);
		}

		std::optional<libclient::settings::FoundKey> touchTwistKey = findKey(
			settings, "settings/input/touchtwist", SettingMeta::Version::V0);
		if(touchTwistKey.has_value() &&
		settings.value(touchTwistKey->key).toBool()) {
			return settings.value(touchTwistKey->key).toBool()
					? int(view::TwoFingerTwistAction::Rotate)
					: int(view::TwoFingerTwistAction::Nothing);
		}

		return int(view::TwoFingerTwistAction::Rotate);
	}
}

namespace tabletDriver {
	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		QVariant value = getV0EnumToV1Int<tabletinput::Mode>(meta, settings);
		switch(value.toInt()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		case int(tabletinput::Mode::Qt5):
			return int(tabletinput::Mode::Qt6Winink);
#else
		case int(tabletinput::Mode::Qt6Winink):
		case int(tabletinput::Mode::Qt6Wintab):
			return int(tabletinput::Mode::Qt5);
#endif
		default:
			return value;
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
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_ANDROID)
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
	QVariant get(const SettingMeta &meta, QSettings &settings)
	{
		std::optional<libclient::settings::FoundKey> stringFoundKey =
			findKeyExactVersion(settings, meta.baseKey, meta.version);
		if (stringFoundKey.has_value()) {
			return settings.value(stringFoundKey->key);
		}

		std::optional<libclient::settings::FoundKey> enumFoundKey =
			findKey(settings, meta.baseKey, SettingMeta::Version::V3);
		if (enumFoundKey.has_value()) {
			QVariant enumValue = settings.value(enumFoundKey->key);
			switch(enumValue.value<desktop::settings::ThemePalette>()) {
			case desktop::settings::ThemePalette::System:
				return QStringLiteral("system");
			case desktop::settings::ThemePalette::Light:
#ifdef Q_OS_MACOS
				return QStringLiteral("light");
#else
				return QStringLiteral("kritabright.colors");
#endif
			case desktop::settings::ThemePalette::Dark:
#ifdef Q_OS_MACOS
				return QStringLiteral("dark");
#else
				return QStringLiteral("nightmode.colors");
#endif
			case desktop::settings::ThemePalette::KritaBright:
				return QStringLiteral("kritabright.colors");
			case desktop::settings::ThemePalette::KritaDark:
				return QStringLiteral("kritadark.colors");
			case desktop::settings::ThemePalette::KritaDarker:
				return QStringLiteral("kritadarker.colors");
			case desktop::settings::ThemePalette::Fusion:
				return QStringLiteral("fusion");
			case desktop::settings::ThemePalette::HotdogStand:
				return QStringLiteral("hotdogstand.colors");
			case desktop::settings::ThemePalette::Indigo:
				return QStringLiteral("indigo.colors");
			case desktop::settings::ThemePalette::PoolTable:
				return QStringLiteral("pooltable.colors");
			case desktop::settings::ThemePalette::Rust:
				return QStringLiteral("rust.colors");
			case desktop::settings::ThemePalette::BlueApatite:
				return QStringLiteral("blueapatite.colors");
			case desktop::settings::ThemePalette::OceanDeep:
				return QStringLiteral("oceandeep.colors");
			case desktop::settings::ThemePalette::RoseQuartz:
				return QStringLiteral("rosequartz.colors");
			case desktop::settings::ThemePalette::Watermelon:
				return QStringLiteral("watermelon.colors");
			}
		}

		return meta.getDefaultValue();
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
	void set(const SettingMeta &meta, QSettings &settings, QVariant value)
	{
		any::forceSet(meta, settings, value);
		settings.remove(meta.baseKey);
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
