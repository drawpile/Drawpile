// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtColorWidgets/swatch.hpp>
#include <QtColorWidgets/color_palette_model.hpp>

#include "desktop/docks/colorpalette.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/dialogs/colordialog.h"
#include "desktop/main.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "libshared/util/paths.h"

#include <QIcon>
#include <QMessageBox>
#include <QMenu>
#include <QFileDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QCursor>

namespace docks {

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

struct ColorPaletteDock::Private {
	QComboBox *paletteChoiceBox = nullptr;
	color_widgets::Swatch *paletteSwatch = nullptr;
	QMenu *palettePopupMenu = nullptr;
	QToolButton *readonlyPalette = nullptr;

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

ColorPaletteDock::ColorPaletteDock(const QString& title, QWidget *parent)
	: DockBase(title, parent), d(new Private)
{
	auto *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);

	auto *menuButton = new widgets::GroupedToolButton;
	menuButton->setIcon(QIcon::fromTheme("application-menu"));
	titlebar->addCustomWidget(menuButton);

	d->readonlyPalette = new widgets::GroupedToolButton;
	d->readonlyPalette->setToolTip(tr("Write protect"));
	d->readonlyPalette->setIcon(QIcon::fromTheme("object-locked"));
	d->readonlyPalette->setCheckable(true);
	titlebar->addCustomWidget(d->readonlyPalette);

	d->paletteChoiceBox = new QComboBox;
	d->paletteChoiceBox->setInsertPolicy(QComboBox::NoInsert); // we want to handle editingFinished signal ourselves
	d->paletteChoiceBox->setModel(getSharedPaletteModel());
	titlebar->addCustomWidget(d->paletteChoiceBox, true);

	titlebar->addSpace(24);

	d->paletteSwatch = new color_widgets::Swatch(this);
	setWidget(d->paletteSwatch);

	QMenu *paletteMenu = new QMenu(this);
	paletteMenu->addAction(tr("New"), this, &ColorPaletteDock::addPalette);
	paletteMenu->addAction(tr("Duplicate"), this, &ColorPaletteDock::copyPalette);
	paletteMenu->addAction(tr("Delete"), this, &ColorPaletteDock::deletePalette);
	paletteMenu->addAction(tr("Rename"), this, &ColorPaletteDock::renamePalette);

	paletteMenu->addSeparator();
	paletteMenu->addAction(tr("Import..."), this, &ColorPaletteDock::importPalette);
	paletteMenu->addAction(tr("Export..."), this, &ColorPaletteDock::exportPalette);

	menuButton->setMenu(paletteMenu);
	menuButton->setPopupMode(QToolButton::InstantPopup);
	menuButton->setStyleSheet("QToolButton::menu-indicator { image: none }");

	d->palettePopupMenu = new QMenu(this);
	d->palettePopupMenu->addAction(tr("Add"))->setProperty("menuIdx", 0);
	d->palettePopupMenu->addAction(tr("Remove"))->setProperty("menuIdx", 1);
	d->palettePopupMenu->addSeparator();
	d->palettePopupMenu->addAction(tr("Less columns"))->setProperty("menuIdx", 2);;
	d->palettePopupMenu->addAction(tr("More columns"))->setProperty("menuIdx", 3);

	connect(d->paletteSwatch, &color_widgets::Swatch::clicked, this, &ColorPaletteDock::paletteClicked);
	connect(d->paletteSwatch, &color_widgets::Swatch::doubleClicked, this, &ColorPaletteDock::paletteDoubleClicked);
	connect(d->paletteSwatch, &color_widgets::Swatch::rightClicked, this, &ColorPaletteDock::paletteRightClicked);
	connect(d->paletteChoiceBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ColorPaletteDock::paletteChanged);
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

	dpApp().settings().bindLastPalette(d->paletteChoiceBox, [=](int index) {
		const auto lastPalette = qBound(0, index, d->paletteChoiceBox->model()->rowCount());
		if(lastPalette > 0)
			d->paletteChoiceBox->setCurrentIndex(lastPalette);
		else
			paletteChanged(0);
	}, QOverload<int>::of(&QComboBox::currentIndexChanged));
}

ColorPaletteDock::~ColorPaletteDock()
{
	d->saveCurrentPalette();
	delete d;
}

void ColorPaletteDock::paletteChanged(int index)
{
	if(index>=0) {
		// switching palettes cancels rename operation (if in progress)
		d->paletteChoiceBox->setEditable(false);

		// Save current palette if it has any changes before switching to another one
		d->saveCurrentPalette();

		color_widgets::ColorPaletteModel *model = getSharedPaletteModel();
		if(index < model->count()) {
			d->paletteSwatch->setPalette(model->palette(index));
			setPaletteReadonly(!d->paletteSwatch->palette().property("editable").toBool());
		}
	}
}

void ColorPaletteDock::importPalette()
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

void ColorPaletteDock::exportPalette()
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
void ColorPaletteDock::paletteRenamed()
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

void ColorPaletteDock::setPaletteReadonly(bool readonly)
{
	d->readonlyPalette->setChecked(readonly);
	d->paletteSwatch->setReadOnly(readonly);
	d->palettePopupMenu->setEnabled(!readonly);
}

void ColorPaletteDock::renamePalette()
{
	d->paletteChoiceBox->setEditable(true);
	connect(d->paletteChoiceBox->lineEdit(), &QLineEdit::editingFinished, this, &ColorPaletteDock::paletteRenamed, Qt::UniqueConnection);
}

void ColorPaletteDock::addPalette()
{
	auto pal = color_widgets::ColorPalette();
	pal.setProperty("editable", true);
	//pal.appendColor(d->colorwheel->color());
	pal.appendColor(Qt::blue); // FIXME current color
	getSharedPaletteModel()->addPalette(pal, false);
	d->paletteChoiceBox->setCurrentIndex(d->paletteChoiceBox->model()->rowCount()-1);
	renamePalette();
	setPaletteReadonly(false);
}

void ColorPaletteDock::copyPalette()
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

void ColorPaletteDock::deletePalette()
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

void ColorPaletteDock::paletteClicked(int index)
{
	if(index >= 0) {
		const auto c = d->paletteSwatch->palette().colorAt(index);
		setColor(c);
		emit colorSelected(c);
	 }
}

void ColorPaletteDock::paletteDoubleClicked(int index)
{
	if(d->readonlyPalette->isChecked())
		return;

	color_widgets::ColorDialog dlg;
	dialogs::applyColorDialogSettings(&dlg);
	dlg.setAlphaEnabled(false);
	dlg.setButtonMode(color_widgets::ColorDialog::OkCancel);
	dlg.setColor(d->paletteSwatch->palette().colorAt(index));
	if(dlg.exec() == QDialog::Accepted) {
		if(index < 0)
			d->paletteSwatch->palette().appendColor(dlg.color());
		else
			d->paletteSwatch->palette().setColorAt(index, dlg.color());
		setColor(dlg.color());
		emit colorSelected(dlg.color());
	}
}

void ColorPaletteDock::paletteRightClicked(int index)
{
	const QAction *act = d->palettePopupMenu->exec(QCursor::pos());
	if(!act)
		return;

	int columns = d->paletteSwatch->palette().columns();
	if(columns == 0)
		columns = d->paletteSwatch->palette().count();

	switch(act->property("menuIdx").toInt()) {
	case 0:
		// FIXME current color
		//d->paletteSwatch->palette().insertColor(index < 0 ? d->paletteSwatch->palette().count() : index+1, d->colorwheel->color());
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

int findPaletteColor(const color_widgets::ColorPalette &pal, const QColor &color)
{
	const auto colors = pal.colors();
	const auto rgb = color.rgb();
	for(int i=0;i<colors.length();++i)
		if(colors.at(i).first.rgb() == rgb) {
			return i;
	}

	return -1;
}

void ColorPaletteDock::setColor(const QColor& color)
{

	d->paletteSwatch->setSelected(findPaletteColor(d->paletteSwatch->palette(), color));
}

}

