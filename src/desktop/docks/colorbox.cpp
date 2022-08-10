/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2021 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QtColorWidgets/color_wheel.hpp>
#include <QtColorWidgets/hue_slider.hpp>
#include <QtColorWidgets/swatch.hpp>
#include <QtColorWidgets/color_palette_model.hpp>
#include <QtColorWidgets/color_dialog.hpp>

#include "main.h"
#include "colorbox.h"
#include "titlewidget.h"
#include "widgets/groupedtoolbutton.h"
#include "utils/icon.h"
#include "../libshared/util/paths.h"

#include <QSettings>
#include <QMessageBox>
#include <QMenu>
#include <QFileDialog>
#include <QBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QCursor>

namespace docks {

static const int LASTUSED_COLOR_COUNT = 8;

static color_widgets::ColorPaletteModel *getSharedPaletteModel()
{
	static color_widgets::ColorPaletteModel *model;
	if(!model) {
		model = new color_widgets::ColorPaletteModel;
		QString localPalettePath = utils::paths::writablePath(QString()) + "/palettes/";

		QDir localPaletteDir(localPalettePath);
		localPaletteDir.mkpath(".");

		model->setSearchPaths(QStringList() << localPalettePath);
		model->setSavePath(localPalettePath);
		model->load();

		// If the user palette directory is empty, copy default palettes in
		if(model->rowCount() == 0) {
			const auto datapaths = utils::paths::dataPaths();
			for(const auto &path : datapaths) {
				const QString systemPalettePath = path + "/palettes/";
				if(systemPalettePath == localPalettePath)
					continue;

				const QDir pd(systemPalettePath);
				const auto palettes = pd.entryList(QStringList() << "*.gpl", QDir::Files);
				for(const auto &p : palettes) {
					QFile::copy(pd.filePath(p), localPaletteDir.filePath(p));
				}
			}
		}

		model->load();
	}

	return model;
}

struct ColorBox::Private {
	// Alternate color history
	color_widgets::ColorPalette lastusedAlt;
	color_widgets::Swatch *lastUsedSwatch;
    QColor altColor;

	// Palette page
	QComboBox *paletteChoiceBox = nullptr;
	color_widgets::Swatch *paletteSwatch = nullptr;
	QMenu *palettePopupMenu = nullptr;
	QToolButton *readonlyPalette = nullptr;

	// Sliders page
	color_widgets::HueSlider *hue = nullptr;
	color_widgets::GradientSlider *saturation = nullptr;
	color_widgets::GradientSlider *value = nullptr;
	color_widgets::GradientSlider *red = nullptr;
	color_widgets::GradientSlider *green = nullptr;
	color_widgets::GradientSlider *blue = nullptr;

	QSpinBox *huebox = nullptr;
	QSpinBox *saturationbox = nullptr;
	QSpinBox *valuebox = nullptr;
	QSpinBox *redbox = nullptr;
	QSpinBox *greenbox = nullptr;
	QSpinBox *bluebox = nullptr;

	// Color wheel page
	color_widgets::ColorWheel *colorwheel = nullptr;

    bool updating = false;

	void saveCurrentPalette()
	{
		if(paletteSwatch->palette().dirty()) {
			auto *pm = getSharedPaletteModel();
			int index = -1;
			for(int i=0;i<pm->rowCount();++i) {
				if(pm->palette(i).name() == paletteSwatch->palette().name()) {
					index = i;
					break;
				}
			}
			if(index >= 0) {
				pm->updatePalette(index, paletteSwatch->palette());
			}
		}
	}
};

ColorBox::ColorBox(const QString& title, QWidget *parent)
	: QDockWidget(title, parent), d(new Private)
{
	// Create main UI widget
	auto *w = new QTabWidget(this);
	w->setTabPosition(QTabWidget::East);
	w->setDocumentMode(true);
	w->setIconSize(QSize(32, 32));
	w->setUsesScrollButtons(false);
	
	QColor highlight = palette().color(QPalette::Highlight);
	QColor hover = highlight; hover.setAlphaF(0.5);

	w->setStyleSheet(QStringLiteral(
		"QTabBar {"
			"alignment: middle;"
		"}"
		"QTabWidget::pane {"
			"border: none;"
		"}"
		"QTabBar::tab {"
			"padding: 15px 5px 0 2px;"
			"margin: 0 2px 0 0;"
			"border: none;"
			"border-left: 3px solid transparent;"
			"background: %3;"
		"}"
		"QTabBar::tab:hover {"
			"border-left: 3px solid %2;"
		"}"
		"QTabBar::tab:selected {"
			"border-left: 3px solid %1;"
		"}"
	).arg(
		highlight.name(),
		QString("rgba(%1, %2, %3, %4)").arg(hover.red()).arg(hover.green()).arg(hover.blue()).arg(hover.alphaF()),
		palette().color(QPalette::Window).name() // this is needed OSX. TODO how to set the whole tabbar background?
	));

	setWidget(w);

	w->addTab(createPalettePage(), QIcon(":/icons/colorbox-pal.png"), QString());
	w->addTab(createSlidersPage(), QIcon(":/icons/colorbox-hsv.png"), QString());
	w->addTab(createWheelPage(), QIcon(":/icons/colorbox-wheel.png"), QString());

	// Create title bar widget
	// The last used colors palette lives here
	auto *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);

	d->lastUsedSwatch = new color_widgets::Swatch(titlebar);
	d->lastUsedSwatch->setForcedRows(1);
	d->lastUsedSwatch->setForcedColumns(LASTUSED_COLOR_COUNT);
	d->lastUsedSwatch->setReadOnly(true);
	d->lastUsedSwatch->setBorder(Qt::NoPen);
	d->lastUsedSwatch->setMinimumHeight(24);

	const auto margins = this->layout()->contentsMargins();
	const int btnWidth = d->readonlyPalette->width() + margins.left() + this->layout()->spacing();
	titlebar->addSpace(btnWidth);
	titlebar->addCustomWidget(d->lastUsedSwatch, true);
	titlebar->addSpace(btnWidth);

	connect(d->lastUsedSwatch, &color_widgets::Swatch::colorSelected, this, &ColorBox::colorChanged);
	connect(d->lastUsedSwatch, &color_widgets::Swatch::colorSelected, this, &ColorBox::setColor);

	// Restore UI state
	QSettings cfg;
	w->setCurrentIndex(cfg.value("history/lastcolortab", 0).toInt());
	int lastPalette = cfg.value("history/lastpalette", 0).toInt();
	lastPalette = qBound(0, lastPalette, d->paletteChoiceBox->model()->rowCount());

	if(lastPalette>0)
		d->paletteChoiceBox->setCurrentIndex(lastPalette);
	else
		paletteChanged(0);

	connect(static_cast<DrawpileApp*>(qApp), &DrawpileApp::settingsChanged,
			this, &ColorBox::updateSettings);
	updateSettings();
}

QWidget *ColorBox::createPalettePage()
{
	auto *w = new QWidget(this);
	auto *layout = new QGridLayout;
	layout->setSpacing(3);
	w->setLayout(layout);

	d->readonlyPalette = new widgets::GroupedToolButton(w);
	d->readonlyPalette->setToolTip("Write protect");
	d->readonlyPalette->setIcon(icon::fromTheme("object-locked"));
	d->readonlyPalette->setCheckable(true);
	layout->addWidget(d->readonlyPalette, 0, 0);

	d->paletteChoiceBox = new QComboBox(w);
	d->paletteChoiceBox->setCompleter(nullptr);
	d->paletteChoiceBox->setInsertPolicy(QComboBox::NoInsert); // we want to handle editingFinished signal ourselves
	d->paletteChoiceBox->setModel(getSharedPaletteModel());
	layout->addWidget(d->paletteChoiceBox, 0, 1);
	layout->setColumnStretch(1, 1);

	auto *menuButton = new widgets::GroupedToolButton(w);
	menuButton->setIcon(icon::fromTheme("application-menu"));
	layout->addWidget(menuButton, 0, 2);

	d->paletteSwatch = new color_widgets::Swatch(w);
	layout->addWidget(d->paletteSwatch, 1, 0, 1, 3);

	QMenu *paletteMenu = new QMenu(this);
	paletteMenu->addAction(tr("New"), this, &ColorBox::addPalette);
	paletteMenu->addAction(tr("Duplicate"), this, &ColorBox::copyPalette);
	paletteMenu->addAction(tr("Delete"), this, &ColorBox::deletePalette);
	paletteMenu->addAction(tr("Rename"), this, &ColorBox::renamePalette);

	paletteMenu->addSeparator();
	paletteMenu->addAction(tr("Import..."), this, &ColorBox::importPalette);
	paletteMenu->addAction(tr("Export..."), this, &ColorBox::exportPalette);

	menuButton->setMenu(paletteMenu);
	menuButton->setPopupMode(QToolButton::InstantPopup);
	menuButton->setStyleSheet("QToolButton::menu-indicator { image: none }");

	d->palettePopupMenu = new QMenu(this);
	d->palettePopupMenu->addAction(tr("Add"))->setProperty("menuIdx", 0);
	d->palettePopupMenu->addAction(tr("Remove"))->setProperty("menuIdx", 1);
	d->palettePopupMenu->addSeparator();
	d->palettePopupMenu->addAction(tr("Less columns"))->setProperty("menuIdx", 2);;
	d->palettePopupMenu->addAction(tr("More columns"))->setProperty("menuIdx", 3);

	connect(d->paletteSwatch, &color_widgets::Swatch::clicked, this, &ColorBox::paletteClicked);
	connect(d->paletteSwatch, &color_widgets::Swatch::doubleClicked, this, &ColorBox::paletteDoubleClicked);
	connect(d->paletteSwatch, &color_widgets::Swatch::rightClicked, this, &ColorBox::paletteRightClicked);
	connect(d->paletteChoiceBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ColorBox::paletteChanged);
	connect(d->readonlyPalette, &QToolButton::clicked, this, [this](bool checked) {
		int idx = d->paletteChoiceBox->currentIndex();
		if(idx >= 0) {
			auto *pm = getSharedPaletteModel();
			auto pal = pm->palette(idx);
			pal.setProperty("editable", !checked);
			pm->updatePalette(idx, pal, false);
			setPaletteReadonly(checked);
		}
	});
	return w;
}

QWidget *ColorBox::createSlidersPage()
{
	Q_ASSERT(d->hue == nullptr);
	QWidget *w = new QWidget;
	auto *layout = new QGridLayout;
	w->setLayout(layout);

	int row = 0;

	d->hue = new color_widgets::HueSlider(w);
	d->hue->setMaximum(359);
	d->huebox = new QSpinBox(w);
	d->huebox->setMaximum(359);
	d->hue->setFixedHeight(d->huebox->height());
	layout->addWidget(d->hue, row, 0);
	layout->addWidget(d->huebox, row, 1);

	++row;

	d->saturation = new color_widgets::GradientSlider(w);
	d->saturation->setMaximum(255);
	d->saturationbox = new QSpinBox(w);
	d->saturationbox->setMaximum(255);
	d->saturation->setFixedHeight(d->saturationbox->height());
	layout->addWidget(d->saturation, row, 0);
	layout->addWidget(d->saturationbox, row, 1);

	++row;

	d->value = new color_widgets::GradientSlider(w);
	d->value->setMaximum(255);
	d->valuebox = new QSpinBox(w);
	d->valuebox->setMaximum(255);
	d->value->setFixedHeight(d->valuebox->height());
	layout->addWidget(d->value, row, 0);
	layout->addWidget(d->valuebox, row, 1);

	++row;

	d->red = new color_widgets::GradientSlider(w);
	d->red->setMaximum(255);
	d->redbox = new QSpinBox(w);
	d->redbox->setMaximum(255);
	d->red->setFixedHeight(d->redbox->height());
	layout->addWidget(d->red, row, 0);
	layout->addWidget(d->redbox, row, 1);

	++row;

	d->green = new color_widgets::GradientSlider(w);
	d->green->setMaximum(255);
	d->greenbox = new QSpinBox(w);
	d->greenbox->setMaximum(255);
	d->green->setFixedHeight(d->greenbox->height());
	layout->addWidget(d->green, row, 0);
	layout->addWidget(d->greenbox, row, 1);

	++row;

	d->blue = new color_widgets::GradientSlider(w);
	d->blue->setMaximum(255);
	d->bluebox = new QSpinBox(w);
	d->bluebox->setMaximum(255);
	d->blue->setFixedHeight(d->bluebox->height());
	layout->addWidget(d->blue, row, 0);
	layout->addWidget(d->bluebox, row, 1);

	++row;

	layout->setSpacing(3);
	layout->setRowStretch(row, 1);

	connect(d->hue, QOverload<int>::of(&color_widgets::HueSlider::valueChanged), this, &ColorBox::updateFromHsvSliders);
	connect(d->huebox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorBox::updateFromHsvSpinbox);

	connect(d->saturation, QOverload<int>::of(&color_widgets::GradientSlider::valueChanged), this, &ColorBox::updateFromHsvSliders);
	connect(d->saturationbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorBox::updateFromHsvSpinbox);

	connect(d->value, QOverload<int>::of(&color_widgets::GradientSlider::valueChanged), this, &ColorBox::updateFromHsvSliders);
	connect(d->valuebox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorBox::updateFromHsvSpinbox);

	connect(d->red, QOverload<int>::of(&color_widgets::GradientSlider::valueChanged), this, &ColorBox::updateFromRgbSliders);
	connect(d->redbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorBox::updateFromRgbSpinbox);

	connect(d->green, QOverload<int>::of(&color_widgets::GradientSlider::valueChanged), this, &ColorBox::updateFromRgbSliders);
	connect(d->greenbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorBox::updateFromRgbSpinbox);

	connect(d->blue, QOverload<int>::of(&color_widgets::GradientSlider::valueChanged), this, &ColorBox::updateFromRgbSliders);
	connect(d->bluebox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorBox::updateFromRgbSpinbox);

	return w;
}

QWidget *ColorBox::createWheelPage()
{
	Q_ASSERT(d->colorwheel == nullptr);
	d->colorwheel = new color_widgets::ColorWheel(this);

	connect(d->colorwheel, &color_widgets::ColorWheel::colorSelected, this, &ColorBox::colorChanged);
	connect(d->colorwheel, &color_widgets::ColorWheel::colorSelected, this, &ColorBox::setColor);

	return d->colorwheel;
}

ColorBox::~ColorBox()
{
	d->saveCurrentPalette();

	QSettings cfg;
	cfg.setValue("history/lastpalette", d->paletteChoiceBox->currentIndex());
	cfg.setValue("history/lastcolortab", static_cast<QTabWidget*>(widget())->currentIndex());

	delete d;
}

void ColorBox::paletteChanged(int index)
{
	if(index>=0) {
		// switching palettes cancels rename operation (if in progress)
		d->paletteChoiceBox->setEditable(false);

		// Save current palette if it has any changes before switching to another one
		d->saveCurrentPalette();

		d->paletteSwatch->setPalette(getSharedPaletteModel()->palette(index));
		setPaletteReadonly(!d->paletteSwatch->palette().property("editable").toBool());
	}
}

void ColorBox::importPalette()
{
	const QString &filename = QFileDialog::getOpenFileName(
		this,
		tr("Import palette"),
		QString(),
		tr("Palettes (%1)").arg("*.gpl") + ";;" +
		tr("All files (*)")
	);

	if(!filename.isEmpty()) {
		auto pal = color_widgets::ColorPalette::fromFile(filename);
		if(pal.count()==0)
			return;
		pal.setFileName(QString());

		getSharedPaletteModel()->addPalette(pal);
		d->paletteChoiceBox->setCurrentIndex(getSharedPaletteModel()->rowCount()-1);
	}
}

void ColorBox::exportPalette()
{
	const QString &filename = QFileDialog::getSaveFileName(
		this,
		tr("Export palette"),
		QString(),
		tr("GIMP palette (%1)").arg("*.gpl")
	);

	if(!filename.isEmpty()) {
		auto pal = d->paletteSwatch->palette();

		if(!pal.save(filename))
			QMessageBox::warning(this, tr("Error"), tr("Couldn't save file"));
	}
}

/**
 * The user has changed the name of a palette. Update the palette
 * and rename the file if it has one.
 */
void ColorBox::paletteRenamed()
{
	const QString newName = d->paletteChoiceBox->currentText();
	if(!newName.isEmpty() && d->paletteSwatch->palette().name() != newName) {
		auto *pm = getSharedPaletteModel();
		d->paletteSwatch->palette().setName(newName);
		pm->updatePalette(d->paletteChoiceBox->currentIndex(), d->paletteSwatch->palette());
		d->paletteSwatch->setPalette(pm->palette(d->paletteChoiceBox->currentIndex()));
	}

	d->paletteChoiceBox->setEditable(false);
}

void ColorBox::setPaletteReadonly(bool readonly)
{
	d->readonlyPalette->setChecked(readonly);
	d->paletteSwatch->setReadOnly(readonly);
	d->palettePopupMenu->setEnabled(!readonly);
}

void ColorBox::renamePalette()
{
	d->paletteChoiceBox->setEditable(true);
	connect(d->paletteChoiceBox->lineEdit(), &QLineEdit::editingFinished, this, &ColorBox::paletteRenamed, Qt::UniqueConnection);
}

void ColorBox::addPalette()
{
	auto pal = color_widgets::ColorPalette();
	pal.setProperty("editable", true);
	pal.appendColor(d->colorwheel->color());
	getSharedPaletteModel()->addPalette(pal, false);
	d->paletteChoiceBox->setCurrentIndex(d->paletteChoiceBox->model()->rowCount()-1);
	renamePalette();
	setPaletteReadonly(false);
}

void ColorBox::copyPalette()
{
	color_widgets::ColorPalette pal = d->paletteSwatch->palette();
	pal.setFileName(QString());
	pal.setName(QString());
	pal.setProperty("editable", true);

	auto *pm = getSharedPaletteModel();
	pm->addPalette(pal, false);
	d->paletteChoiceBox->setCurrentIndex(pm->rowCount()-1);
	renamePalette();
	setPaletteReadonly(false);
}

void ColorBox::deletePalette()
{
	const int current = d->paletteChoiceBox->currentIndex();
	if(current >= 0) {
	const int ret = QMessageBox::question(
			this,
			tr("Delete"),
			tr("Delete palette \"%1\"?").arg(d->paletteChoiceBox->currentText()),
			QMessageBox::Yes|QMessageBox::No);

		if(ret == QMessageBox::Yes) {
			getSharedPaletteModel()->removePalette(current);
		}
	}
}

void ColorBox::paletteClicked(int index)
{
	if(index >= 0) {
		const auto c = d->paletteSwatch->palette().colorAt(index);
		setColor(c);
		emit colorChanged(c);
	 }
}

void ColorBox::paletteDoubleClicked(int index)
{
	if(d->readonlyPalette->isChecked())
		return;

	color_widgets::ColorDialog dlg;
	dlg.setAlphaEnabled(false);
	dlg.setButtonMode(color_widgets::ColorDialog::OkCancel);
	dlg.setColor(d->paletteSwatch->palette().colorAt(index));
	if(dlg.exec() == QDialog::Accepted) {
		if(index < 0)
			d->paletteSwatch->palette().appendColor(dlg.color());
		else
			d->paletteSwatch->palette().setColorAt(index, dlg.color());
		setColor(dlg.color());
		emit colorChanged(dlg.color());
	}
}

void ColorBox::paletteRightClicked(int index)
{
	const QAction *act = d->palettePopupMenu->exec(QCursor::pos());
	if(!act)
		return;

	int columns = d->paletteSwatch->palette().columns();
	if(columns == 0)
		columns = d->paletteSwatch->palette().count();

	switch(act->property("menuIdx").toInt()) {
	case 0:
		d->paletteSwatch->palette().insertColor(index < 0 ? d->paletteSwatch->palette().count() : index+1, d->colorwheel->color());
		break;
	case 1:
		if(d->paletteSwatch->palette().count() > 1 && index >= 0)
			d->paletteSwatch->palette().eraseColor(index);
		break;
	case 2:
		if(columns > 1)
			d->paletteSwatch->palette().setColumns(columns - 1);
		break;
	case 3:
		d->paletteSwatch->palette().setColumns(columns + 1);
		break;
	}
}

static int findPaletteColor(const color_widgets::ColorPalette &pal, const QColor &color)
{
	const auto colors = pal.colors();
	const auto rgb = color.rgb();
	for(int i=0;i<colors.length();++i)
		if(colors.at(i).first.rgb() == rgb) {
			return i;
	}

	return -1;
}

void ColorBox::setColor(const QColor& color)
{
	d->updating = true;

	d->lastUsedSwatch->setSelected(findPaletteColor(d->lastUsedSwatch->palette(), color));
	d->paletteSwatch->setSelected(findPaletteColor(d->paletteSwatch->palette(), color));

	d->hue->setColorSaturation(color.saturationF());
	d->hue->setColorValue(color.valueF());
	d->hue->setValue(color.hue());
	d->huebox->setValue(color.hue());

	d->saturation->setFirstColor(QColor::fromHsv(color.hue(), 0, color.value()));
	d->saturation->setLastColor(QColor::fromHsv(color.hue(), 255, color.value()));
	d->saturation->setValue(color.saturation());
	d->saturationbox->setValue(color.saturation());

	d->value->setFirstColor(QColor::fromHsv(color.hue(), color.saturation(), 0));
	d->value->setLastColor(QColor::fromHsv(color.hue(), color.saturation(), 255));
	d->value->setValue(color.value());
	d->valuebox->setValue(color.value());

	d->red->setFirstColor(QColor(0, color.green(), color.blue()));
	d->red->setLastColor(QColor(255, color.green(), color.blue()));
	d->red->setValue(color.red());
	d->redbox->setValue(color.red());

	d->green->setFirstColor(QColor(color.red(), 0, color.blue()));
	d->green->setLastColor(QColor(color.red(), 255, color.blue()));
	d->green->setValue(color.green());
	d->greenbox->setValue(color.green());

	d->blue->setFirstColor(QColor(color.red(), color.green(), 0));
	d->blue->setLastColor(QColor(color.red(), color.green(), 255));
	d->blue->setValue(color.blue());
	d->bluebox->setValue(color.blue());

	d->colorwheel->setColor(color);
	d->updating = false;
}

void ColorBox::updateFromRgbSliders()
{
	if(!d->updating) {
		QColor color(d->red->value(), d->green->value(), d->blue->value());

		setColor(color);
		emit colorChanged(color);
	}
}

void ColorBox::updateFromRgbSpinbox()
{
	if(!d->updating) {
		QColor color(d->redbox->value(), d->greenbox->value(), d->bluebox->value());
		setColor(color);
		emit colorChanged(color);
	}
}

void ColorBox::updateFromHsvSliders()
{
	if(!d->updating) {
		QColor color = QColor::fromHsv(d->hue->value(), d->saturation->value(), d->value->value());

		setColor(color);
		emit colorChanged(color);
	}
}

void ColorBox::updateFromHsvSpinbox()
{
	if(!d->updating) {
		QColor color = QColor::fromHsv(d->huebox->value(), d->saturationbox->value(), d->valuebox->value());

		setColor(color);
		emit colorChanged(color);
	}
}

void ColorBox::addLastUsedColor(const QColor &color)
{
	color_widgets::ColorPalette &pal = d->lastUsedSwatch->palette();
	{
		const auto c = pal.colorAt(0);
		if(c.isValid() && c.rgb() == color.rgb())
			return;
	}

	// Move color to the front of the palette
	pal.insertColor(0, color);
	for(int i=1;i<pal.count();++i) {
		if(pal.colorAt(i).rgb() == color.rgb()) {
			pal.eraseColor(i);
			break;
		}
	}

	// Limit number of remembered colors
	if(pal.count() > LASTUSED_COLOR_COUNT)
		pal.eraseColor(LASTUSED_COLOR_COUNT);

	d->lastUsedSwatch->setSelected(0);
}

void ColorBox::swapLastUsedColors()
{
	auto lastUsedPal = d->lastUsedSwatch->palette();
	auto lastUsedCol = d->colorwheel->color();

	d->lastUsedSwatch->setPalette(d->lastusedAlt);
	setColor(d->altColor);
	emit colorChanged(d->altColor);

	d->lastusedAlt = lastUsedPal;
	d->altColor = lastUsedCol;
}

void ColorBox::updateSettings()
{
	QSettings cfg;
	cfg.beginGroup("settings/colorwheel");
	d->colorwheel->setSelectorShape(
			static_cast<color_widgets::ColorWheel::ShapeEnum>(cfg.value("shape").toInt()));
	d->colorwheel->setRotatingSelector(
			static_cast<color_widgets::ColorWheel::AngleEnum>(cfg.value("rotate").toInt()));
	d->colorwheel->setColorSpace(
			static_cast<color_widgets::ColorWheel::ColorSpaceEnum>(cfg.value("space").toInt()));
}

}

