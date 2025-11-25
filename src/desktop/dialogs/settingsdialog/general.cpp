// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/general.h"
#include "cmake-config/config.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/config/config.h"
#include "libclient/view/enums.h"
#include "libshared/util/paths.h"
#include "libshared/util/qtcompat.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFormLayout>
#include <QSpinBox>
#include <QString>
#include <QStyleFactory>
#include <QVBoxLayout>
#include <QWidget>

namespace {
struct Theme {
	QString value;
	QString name;

	bool operator<(const Theme &other) const
	{
		return name.compare(other.name, Qt::CaseInsensitive) < 0;
	}
};

class ThemeCollector {
public:
	ThemeCollector(
		QString &&notFoundTemplate, QHash<QString, QString> &&themeNames)
		: m_notFoundTemplate(std::move(notFoundTemplate))
		, m_themeNames(std::move(themeNames))
	{
	}

	void addInternalTheme(const QString &value, const QString &name)
	{
		Q_ASSERT(!m_addedThemeValues.contains(value));
		m_addedThemeValues.insert(value);
		m_internalThemes.append({value, name});
	}

	void handleTheme(const QString &value, bool found = true)
	{
		if(!m_addedThemeValues.contains(value)) {
			m_addedThemeValues.insert(value);

			QString name;
			bool builtin = isBuiltinTheme(value, name);
			Theme theme = {value, found ? name : m_notFoundTemplate.arg(name)};
			if(builtin) {
				m_builtinThemes.append(theme);
			} else {
				m_customThemes.append(theme);
			}
		}
	}

	QComboBox *makeWidget()
	{
		std::sort(m_builtinThemes.begin(), m_builtinThemes.end());
		std::sort(m_customThemes.begin(), m_customThemes.end());
		QComboBox *combo = new QComboBox;
		addThemesTo(combo, m_internalThemes);
		addThemesTo(combo, m_builtinThemes);
		addThemesTo(combo, m_customThemes);
		return combo;
	}

private:
	bool isBuiltinTheme(const QString &value, QString &outName)
	{
		QHash<QString, QString>::const_iterator it =
			m_themeNames.constFind(value);
		if(it == m_themeNames.constEnd()) {
			outName = value;
			return false;
		} else {
			outName = *it;
			return true;
		}
	}

	static void addThemesTo(QComboBox *combo, const QVector<Theme> &themes)
	{
		for(const Theme &theme : themes) {
			combo->addItem(theme.name, theme.value);
		}
	}

	QString m_notFoundTemplate;
	QHash<QString, QString> m_themeNames;
	QSet<QString> m_addedThemeValues;
	QVector<Theme> m_internalThemes;
	QVector<Theme> m_builtinThemes;
	QVector<Theme> m_customThemes;
};
}

namespace dialogs {
namespace settingsdialog {

General::General(config::Config *cfg, QWidget *parent)
	: Page(parent)
{
	init(cfg);
}

void General::setUp(config::Config *cfg, QVBoxLayout *layout)
{
	QFormLayout *themeLanguageSection = utils::addFormSection(layout);
	initTheme(cfg, themeLanguageSection);
#ifndef __EMSCRIPTEN__
	initLanguage(cfg, themeLanguageSection);
#endif

	utils::addFormSeparator(layout);

	QFormLayout *canvasSection = utils::addFormSection(layout);
	initContributing(cfg, canvasSection);
	utils::addFormSpacer(canvasSection);
	initUndo(cfg, canvasSection);
	utils::addFormSpacer(canvasSection);
	initSnapshots(cfg, canvasSection);

	utils::addFormSeparator(layout);

	initPerformance(cfg, utils::addFormSection(layout));
}

void General::initLanguage(config::Config *cfg, QFormLayout *form)
{
	QComboBox *language = new QComboBox;
	language->addItem(tr("System"), QString());

	const QLocale localeC = QLocale::c();
	for(const char *localeName : cmake_config::locales()) {
		QLocale locale(localeName);
		if(locale != localeC) {
			language->addItem(formatLanguage(locale), localeName);
		}
	}

	CFG_BIND_COMBOBOX_USER_STRING(cfg, Language, language);
	form->addRow(tr("Language:"), language);
	form->addRow(
		nullptr, utils::formNote(
					 tr("Language changes apply after you restart Drawpile.")));
}

QString General::formatLanguage(const QLocale &locale)
{
	bool needsCountryDisambiguation = false;
	for(const char *localeName : cmake_config::locales()) {
		QLocale other(localeName);
		if(locale != other && locale.language() == other.language()) {
			needsCountryDisambiguation = true;
			break;
		}
	}

	if(needsCountryDisambiguation) {
		return tr("%1 (%2) / %3 (%4)")
			.arg(locale.nativeLanguageName())
			.arg(compat::nativeTerritoryName(locale))
			.arg(QLocale::languageToString(locale.language()))
			.arg(compat::territoryToString(locale));
	} else {
		return tr("%1 / %2")
			.arg(locale.nativeLanguageName())
			.arg(QLocale::languageToString(locale.language()));
	}
}

void General::initContributing(config::Config *cfg, QFormLayout *form)
{
	QCheckBox *enableContributing =
		new QCheckBox(tr("Show contribution, donation and feedback links"));
	CFG_BIND_CHECKBOX(cfg, DonationLinksEnabled, enableContributing);
	form->addRow(tr("Contributing:"), enableContributing);
}

void General::initPerformance(config::Config *cfg, QFormLayout *form)
{
	QComboBox *canvasImplementation = new QComboBox;
	canvasImplementation->setSizeAdjustPolicy(QComboBox::AdjustToContents);

	//: One of the canvas renderer options. "Qt" is a software framework.
	QString graphicsViewName = tr("Qt Graphics View");
	//: One of the canvas renderer options. Hardware meaning it uses the GPU.
	QString openGlName = tr("Hardware");
	//: One of the canvas renderer options. Software meaning it uses the CPU.
	QString softwareName = tr("Software");
	QPair<QString, int> implementations[] = {
		//: One of the canvas renderer options.
		{tr("Default"), int(view::CanvasImplementation::Default)},
		{graphicsViewName, int(view::CanvasImplementation::GraphicsView)},
		{openGlName, int(view::CanvasImplementation::OpenGl)},
		{softwareName, int(view::CanvasImplementation::Software)},
	};
	for(const QPair<QString, int> &p : implementations) {
		canvasImplementation->addItem(p.first, QVariant::fromValue(p.second));
	}

	CFG_BIND_COMBOBOX_USER_INT(cfg, RenderCanvas, canvasImplementation);
	form->addRow(tr("Renderer:"), canvasImplementation);

	QString currentName;
	switch(dpApp().canvasImplementation()) {
	case int(view::CanvasImplementation::GraphicsView):
		currentName = graphicsViewName;
		break;
	case int(view::CanvasImplementation::OpenGl):
		currentName = openGlName;
		break;
	case int(view::CanvasImplementation::Software):
		currentName = softwareName;
		break;
	default:
		//: Refers to an unknown canvas renderer, should never happen.
		currentName = tr("Unknown", "CanvasImplementation");
		break;
	}
	form->addRow(
		nullptr, utils::formNote(tr("Current renderer: %1. Changes apply after "
									"you restart Drawpile.")
									 .arg(currentName)));

	QCheckBox *renderSmooth =
		new QCheckBox(tr("Interpolate when view is zoomed or rotated"));
	CFG_BIND_CHECKBOX(cfg, RenderSmooth, renderSmooth);
	form->addRow(nullptr, renderSmooth);

	QCheckBox *renderUpdateFull =
		new QCheckBox(tr("Prevent jitter at certain zoom and rotation levels"));
	CFG_BIND_CHECKBOX(cfg, RenderUpdateFull, renderUpdateFull);
	form->addRow(nullptr, renderUpdateFull);

	QCheckBox *useMipmaps =
		new QCheckBox(tr("Improve zoom quality (hardware renderer only)"));
	CFG_BIND_CHECKBOX(cfg, UseMipmaps, useMipmaps);
	form->addRow(nullptr, useMipmaps);
	CFG_BIND_SET_FN(cfg, RenderSmooth, useMipmaps, [useMipmaps](bool smooth) {
		useMipmaps->setEnabled(smooth);
		useMipmaps->setVisible(smooth);
	});

	form->addRow(
		nullptr,
		utils::formNote(tr(
			"Enabling these options may impact performance on some systems.")));
}

void General::initSnapshots(config::Config *cfg, QFormLayout *form)
{
	QSpinBox *snapshotCount = new QSpinBox;
	snapshotCount->setRange(0, 99);
	CFG_BIND_SPINBOX(cfg, EngineSnapshotCount, snapshotCount);
	utils::EncapsulatedLayout *snapshotCountLayout =
		utils::encapsulate(tr("Keep %1 canvas snapshots"), snapshotCount);
	snapshotCountLayout->setControlTypes(QSizePolicy::CheckBox);
	form->addRow(tr("Canvas snapshots:"), snapshotCountLayout);

	QSpinBox *snapshotInterval = new QSpinBox;
	snapshotInterval->setRange(1, 600);
	snapshotInterval->setSingleStep(5);
	CFG_BIND_SPINBOX(cfg, EngineSnapshotInterval, snapshotInterval);
	CFG_BIND_SET_FN(
		cfg, EngineSnapshotCount, snapshotInterval,
		[snapshotInterval](int count) {
			snapshotInterval->setEnabled(count != 0);
		});
	utils::EncapsulatedLayout *snapshotIntervalLayout = utils::encapsulate(
		tr("Take one snapshot every %1 seconds"), snapshotInterval);
	snapshotIntervalLayout->setControlTypes(QSizePolicy::CheckBox);
	form->addRow(nullptr, snapshotIntervalLayout);

	QString snapshotNote =
		tr("Snapshots can be restored from the Session ▸ Reset… menu.");
#ifdef Q_OS_ANDROID
	// The Android font can't deal with this character.
	snapshotNote.replace(QStringLiteral("▸"), QStringLiteral(">"));
#endif
	form->addRow(nullptr, utils::formNote(snapshotNote));
}

void General::initTheme(config::Config *cfg, QFormLayout *form)
{
	QComboBox *style = new QComboBox;
	style->addItem(tr("System"), QString());
	const QStringList styleNames = QStyleFactory::keys();
	for(const QString &styleName : styleNames) {
		// Qt5 does not give a proper name for the macOS style
		if(styleName == QStringLiteral("macintosh")) {
			style->addItem(QStringLiteral("macOS"), styleName);
		} else {
			style->addItem(styleName, styleName);
		}
	}
	CFG_BIND_COMBOBOX_USER_STRING(cfg, ThemeStyle, style);
	form->addRow(tr("Style:"), style);

	ThemeCollector themeCollector(
		//: %1 is the name of a theme, whose file was not found.
		tr("%1 (not found)"),
		{
			//: The name for a color scheme that's blueish green.
			{QStringLiteral("blueapatite.colors"), tr("Blue Apatite")},
			//: The name for a color scheme that's yellow and red.
			{QStringLiteral("hotdogstand.colors"), tr("Hotdog Stand")},
			//: The name for a color scheme that's purple.
			{QStringLiteral("indigo.colors"), tr("Indigo")},
			//: The name for a color scheme. "Krita" is a name.
			{QStringLiteral("kritabright.colors"), tr("Krita Bright")},
			//: The name for a color scheme. "Krita" is a name.
			{QStringLiteral("kritadark.colors"), tr("Krita Dark")},
			//: The name for a color scheme. "Krita" is a name.
			{QStringLiteral("kritadarker.colors"), tr("Krita Darker")},
			//: The name for a color scheme that is dark like the night.
			{QStringLiteral("nightmode.colors"), tr("Night Mode")},
			//: The name for a color scheme that's a deep blue.
			{QStringLiteral("oceandeep.colors"), tr("Ocean Deep")},
			//: The name for a color scheme that's green and orange.
			{QStringLiteral("pooltable.colors"), tr("Pool Table")},
			//: The name for a color scheme that's light and pink.
			{QStringLiteral("rosequartz.colors"), tr("Rose Quartz")},
			//: The name for a color scheme that's orange and brown.
			{QStringLiteral("rust.colors"), tr("Rust")},
			//: The name for a color scheme that's green and pink.
			{QStringLiteral("watermelon.colors"), tr("Watermelon")},
		});

	//: The name for the system color scheme.
	themeCollector.addInternalTheme(QStringLiteral("system"), tr("System"));
#ifdef Q_OS_MACOS
	//: The name for the light system color scheme.
	themeCollector.addInternalTheme(QStringLiteral("light"), tr("Light"));
	//: The name for the dark system color scheme.
	themeCollector.addInternalTheme(QStringLiteral("dark"), tr("Dark"));
#endif
	//: The name of a color theme. Qt and Fusion are names.
	themeCollector.addInternalTheme(QStringLiteral("fusion"), tr("Qt Fusion"));

	for(const QString &dataPath : utils::paths::dataPaths()) {
		QDir dir(dataPath);
		dir.setNameFilters({QStringLiteral("*.colors")});
		dir.setFilter(QDir::Files | QDir::Readable);
		for(const QString &entry : dir.entryList()) {
			themeCollector.handleTheme(entry);
		}
	}

	themeCollector.handleTheme(cfg->getThemePalette(), false);

	QComboBox *theme = themeCollector.makeWidget();
	CFG_BIND_COMBOBOX_USER_STRING(cfg, ThemePalette, theme);
	form->addRow(tr("Color scheme:"), theme);
}

void General::initUndo(config::Config *cfg, QFormLayout *form)
{
	QSpinBox *undoLimit = new QSpinBox;
	undoLimit->setRange(3, 255);
	CFG_BIND_SPINBOX(cfg, EngineUndoDepth, undoLimit);
	utils::EncapsulatedLayout *undoLimitLayout =
		utils::encapsulate(tr("%1 offline undo levels by default"), undoLimit);
	undoLimitLayout->setControlTypes(QSizePolicy::CheckBox);
	form->addRow(tr("Session history:"), undoLimitLayout);
}

}
}
