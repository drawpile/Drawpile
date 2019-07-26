#include "brushpresetmodel.h"
#include "brushpresetmigration.h"
#include "brush.h"

#include <QSettings>

namespace brushes {

static ClassicBrush readBrush(const QSettings &cfg)
{
	ClassicBrush b;
	const int brushmode = cfg.value("brushmode").toInt();
	switch(brushmode) {
	case 0: b.setShape(ClassicBrush::ROUND_PIXEL); break;
	case 1: b.setShape(ClassicBrush::SQUARE_PIXEL); break;
	case 2:
	case 3: b.setShape(ClassicBrush::ROUND_SOFT); break;
	}

	b.setSize(cfg.value("size").toInt());
	b.setOpacity(cfg.value("opacity").toInt() / 100.0);
	b.setHardness(cfg.value("hard").toInt() / 100.0);
	b.setSmudge(cfg.value("smudge").toInt() / 100.0);
	b.setResmudge(cfg.value("resmudge").toInt());
	b.setSpacing(cfg.value("spacing").toInt() / 100.0);
	b.setSizePressure(cfg.value("sizep").toBool());
	b.setOpacityPressure(cfg.value("opacityb").toBool());
	b.setHardnessPressure(cfg.value("hardp").toBool());
	b.setSmudgePressure(cfg.value("smudgep").toBool());
	b.setIncremental(cfg.value("incremental", true).toBool());
	b.setColorPickMode(cfg.value("colorpickmode").toBool());

	return b;
}

bool migrateQSettingsBrushPresets()
{
	QSettings cfg;
	cfg.beginGroup("tools/brushpresets");
	const int size = cfg.beginReadArray("preset");
	for(int i=0;i<size;++i) {
		cfg.setArrayIndex(i);
		BrushPresetModel::writeBrush(readBrush(cfg), QStringLiteral("migrated-%1.dpbrush").arg(i));
	}
	cfg.endArray();

	if(size>0)
		cfg.remove(QString());

	return size>0;
}

}
