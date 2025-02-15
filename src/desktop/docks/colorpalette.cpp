// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/colorpalette.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/docks/toolsettingsdock.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/palettewidget.h"
#include "libclient/utils/wasmpersistence.h"
#include "libshared/util/paths.h"
#include <QComboBox>
#include <QCursor>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPaintEvent>
#include <QVBoxLayout>
#include <QtColorWidgets/color_palette_model.hpp>
#include <QtColorWidgets/swatch.hpp>

namespace docks {

namespace {

// Palette icons look really skrunkly, turn them off.
class IconlessColorPaletteModel : public color_widgets::ColorPaletteModel {
	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override
	{
		return role == Qt::DecorationRole
				   ? QVariant{}
				   : color_widgets::ColorPaletteModel::data(index, role);
	}
};

}

static color_widgets::ColorPaletteModel *getSharedPaletteModel()
{
	static color_widgets::ColorPaletteModel *model;
	if(!model) {
		model = new IconlessColorPaletteModel;
		QString localPalettePath =
			utils::paths::writablePath(QString()) + "/palettes/";

		QDir localPaletteDir(localPalettePath);
		localPaletteDir.mkpath(".");

		model->setSearchPaths(QStringList() << localPalettePath);
		model->setSavePath(localPalettePath);
		model->load();

		// If the user palette directory is empty, copy default palettes in
		if(model->rowCount() == 0) {
			QStringList datapaths = utils::paths::dataPaths();
			for(const QString &path : datapaths) {
				QString systemPalettePath = path + "/palettes/";
				if(systemPalettePath == localPalettePath)
					continue;

				QDir pd(systemPalettePath);
				QStringList palettes =
					pd.entryList(QStringList() << "*.gpl", QDir::Files);
				for(const QString &p : palettes) {
					QFile::copy(pd.filePath(p), localPaletteDir.filePath(p));
				}
			}
		}

		model->load();
	}

	return model;
}

struct ColorPaletteDock::Private {
	color_widgets::Swatch *lastUsedSwatch = nullptr;
	widgets::GroupedToolButton *addColumnButton = nullptr;
	widgets::GroupedToolButton *removeColumnButton = nullptr;
	QComboBox *paletteChoiceBox = nullptr;
	widgets::PaletteWidget *paletteWidget = nullptr;
	QColor color = Qt::black;

	void saveCurrentPalette()
	{
		if(paletteWidget->colorPalette().dirty()) {
			color_widgets::ColorPaletteModel *pm = getSharedPaletteModel();
			int index = -1;
			for(int i = 0; i < pm->rowCount(); ++i) {
				if(pm->palette(i).name() ==
				   paletteWidget->colorPalette().name()) {
					index = i;
					break;
				}
			}
			if(index >= 0) {
				pm->updatePalette(index, paletteWidget->colorPalette());
				DRAWPILE_FS_PERSIST();
			}
		}
	}
};

ColorPaletteDock::ColorPaletteDock(QWidget *parent)
	: DockBase(
		  tr("Color Palette"), tr("Palette"),
		  QIcon::fromTheme("drawpile_colorpalette"), parent)
	, d(new Private)
{
	TitleWidget *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);

	widgets::GroupedToolButton *menuButton = new widgets::GroupedToolButton;
	menuButton->setIcon(QIcon::fromTheme("application-menu"));

	QMenu *paletteMenu = new QMenu(this);
	paletteMenu->addAction(
		tr("New palette"), this, &ColorPaletteDock::addPalette);
	paletteMenu->addAction(
		tr("Duplicate palette"), this, &ColorPaletteDock::copyPalette);
	paletteMenu->addAction(
		tr("Delete palette"), this, &ColorPaletteDock::deletePalette);
	paletteMenu->addAction(
		tr("Rename palette"), this, &ColorPaletteDock::renamePalette);
#ifndef __EMSCRIPTEN__
	paletteMenu->addSeparator();
	paletteMenu->addAction(
		tr("Import palette…"), this, &ColorPaletteDock::importPalette);
	paletteMenu->addAction(
		tr("Export palette…"), this, &ColorPaletteDock::exportPalette);
#endif
	menuButton->setMenu(paletteMenu);
	menuButton->setPopupMode(QToolButton::InstantPopup);

	d->lastUsedSwatch = new color_widgets::Swatch(titlebar);
	d->lastUsedSwatch->setForcedRows(1);
	d->lastUsedSwatch->setForcedColumns(
		docks::ToolSettings::LASTUSED_COLOR_COUNT);
	d->lastUsedSwatch->setReadOnly(true);
	d->lastUsedSwatch->setBorder(Qt::NoPen);
	d->lastUsedSwatch->setMinimumHeight(24);

	titlebar->addCustomWidget(menuButton);
	titlebar->addSpace(4);
	titlebar->addCustomWidget(d->lastUsedSwatch, 1);
	titlebar->addSpace(4);

	connect(
		d->lastUsedSwatch, &color_widgets::Swatch::colorSelected, this,
		&ColorPaletteDock::colorSelected);

	QWidget *widget = new QWidget;
	QVBoxLayout *layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);
	widget->setLayout(layout);
	setWidget(widget);

	QHBoxLayout *choiceLayout = new QHBoxLayout;
	choiceLayout->setContentsMargins(2, 0, 0, 0);
	choiceLayout->setSpacing(0);
	layout->addLayout(choiceLayout);

	d->addColumnButton =
		new widgets::GroupedToolButton{widgets::GroupedToolButton::GroupLeft};
	d->addColumnButton->setIcon(
		QIcon::fromTheme("edit-table-insert-column-right"));
	d->addColumnButton->setToolTip(tr("Add column"));
	choiceLayout->addWidget(d->addColumnButton);
	connect(
		d->addColumnButton, &QAbstractButton::clicked, this,
		&ColorPaletteDock::addColumn);

	d->removeColumnButton =
		new widgets::GroupedToolButton{widgets::GroupedToolButton::GroupRight};
	d->removeColumnButton->setIcon(
		QIcon::fromTheme("edit-table-delete-column"));
	d->removeColumnButton->setToolTip(tr("Remove column"));
	choiceLayout->addWidget(d->removeColumnButton);
	connect(
		d->removeColumnButton, &QAbstractButton::clicked, this,
		&ColorPaletteDock::removeColumn);

	choiceLayout->addSpacing(4);

	d->paletteChoiceBox = new QComboBox;
	d->paletteChoiceBox->setInsertPolicy(QComboBox::NoInsert);
	d->paletteChoiceBox->setModel(getSharedPaletteModel());
	d->paletteChoiceBox->setMinimumWidth(24);
	choiceLayout->addWidget(d->paletteChoiceBox, 1);

	choiceLayout->addSpacing(4);

	d->paletteWidget = new widgets::PaletteWidget{this};
	d->paletteWidget->setMinimumHeight(20);
	layout->addWidget(d->paletteWidget, 1);
	connect(
		d->paletteWidget, &widgets::PaletteWidget::colorSelected, this,
		&ColorPaletteDock::selectColor);
	connect(
		d->paletteWidget, &widgets::PaletteWidget::columnsChanged, this,
		&ColorPaletteDock::updateColumnButtons);

	connect(
		d->paletteChoiceBox,
		QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&ColorPaletteDock::paletteChanged);

	dpApp().settings().bindLastPalette(
		d->paletteChoiceBox,
		[=](int index) {
			int lastPalette =
				qBound(0, index, d->paletteChoiceBox->model()->rowCount());
			if(lastPalette > 0) {
				d->paletteChoiceBox->setCurrentIndex(lastPalette);
			} else {
				paletteChanged(0);
			}
		},
		QOverload<int>::of(&QComboBox::currentIndexChanged));
}

ColorPaletteDock::~ColorPaletteDock()
{
	d->saveCurrentPalette();
	delete d;
}

void ColorPaletteDock::setColor(const QColor &color)
{
	d->color = color;
	d->paletteWidget->setNextColor(color);
	d->lastUsedSwatch->setSelected(
		findPaletteColor(d->lastUsedSwatch->palette(), d->color));
}

void ColorPaletteDock::setLastUsedColors(const color_widgets::ColorPalette &pal)
{
	d->lastUsedSwatch->setPalette(pal);
	d->lastUsedSwatch->setSelected(
		findPaletteColor(d->lastUsedSwatch->palette(), d->color));
}

void ColorPaletteDock::paletteChanged(int index)
{
	if(index >= 0) {
		d->saveCurrentPalette();
		color_widgets::ColorPaletteModel *model = getSharedPaletteModel();
		if(index < model->count()) {
			d->paletteWidget->setColorPalette(model->palette(index));
		}
	}
}

void ColorPaletteDock::addColumn()
{
	d->paletteWidget->setColumns(d->paletteWidget->columns() + 1);
}

void ColorPaletteDock::removeColumn()
{
	d->paletteWidget->setColumns(d->paletteWidget->columns() - 1);
}

void ColorPaletteDock::updateColumnButtons(int columns)
{
	d->addColumnButton->setEnabled(
		columns < widgets::PaletteWidget::MAX_COLUMNS);
	d->removeColumnButton->setEnabled(columns > 1);
}

void ColorPaletteDock::addPalette()
{
	utils::getInputText(
		this, tr("New Palette"), tr("Name"), QString(),
		[this](const QString &text) {
			QString name = text.trimmed();
			if(!name.isEmpty()) {
				color_widgets::ColorPalette pal = color_widgets::ColorPalette();
				pal.setName(name);
				pal.setColumns(8);
				getSharedPaletteModel()->addPalette(pal, false);
				d->paletteChoiceBox->setCurrentIndex(
					d->paletteChoiceBox->model()->rowCount() - 1);
				DRAWPILE_FS_PERSIST();
			}
		});
}

void ColorPaletteDock::copyPalette()
{
	color_widgets::ColorPalette pal = d->paletteWidget->colorPalette();
	pal.setFileName(QString());
	pal.setName(QString());

	color_widgets::ColorPaletteModel *pm = getSharedPaletteModel();
	pm->addPalette(pal, false);
	d->paletteChoiceBox->setCurrentIndex(pm->rowCount() - 1);
	renamePalette();

	DRAWPILE_FS_PERSIST();
}

void ColorPaletteDock::deletePalette()
{
	const int current = d->paletteChoiceBox->currentIndex();
	if(current >= 0) {
		QMessageBox *box = utils::showQuestion(
			this, tr("Delete"),
			tr("Delete palette \"%1\"?")
				.arg(d->paletteChoiceBox->currentText()));
		connect(box, &QMessageBox::accepted, this, [current] {
			getSharedPaletteModel()->removePalette(current);
			DRAWPILE_FS_PERSIST();
		});
	}
}

void ColorPaletteDock::renamePalette()
{
	utils::getInputText(
		this, tr("Rename Palette"), tr("Name"), QString(),
		[this](const QString &text) {
			QString name = text.trimmed();
			if(!name.isEmpty()) {
				color_widgets::ColorPaletteModel *pm = getSharedPaletteModel();
				d->paletteWidget->colorPalette().setName(name);
				pm->updatePalette(
					d->paletteChoiceBox->currentIndex(),
					d->paletteWidget->colorPalette());
				d->paletteWidget->setColorPalette(
					pm->palette(d->paletteChoiceBox->currentIndex()));
				DRAWPILE_FS_PERSIST();
			}
		});
}

#ifndef __EMSCRIPTEN__
void ColorPaletteDock::importPalette()
{
	QString filename = QFileDialog::getOpenFileName(
		this, tr("Import palette"), QString(),
		tr("Palettes (%1)").arg("*.gpl") + ";;" + tr("All files (*)"));

	if(!filename.isEmpty()) {
		color_widgets::ColorPalette pal =
			color_widgets::ColorPalette::fromFile(filename);
		if(pal.count() == 0)
			return;
		pal.setFileName(QString());

		getSharedPaletteModel()->addPalette(pal);
		d->paletteChoiceBox->setCurrentIndex(
			getSharedPaletteModel()->rowCount() - 1);
	}
}

void ColorPaletteDock::exportPalette()
{
	const QString &filename = QFileDialog::getSaveFileName(
		this, tr("Export palette"), QString(),
		tr("GIMP palette (%1)").arg("*.gpl"));

	if(!filename.isEmpty()) {
		color_widgets::ColorPalette pal = d->paletteWidget->colorPalette();

		if(!pal.save(filename))
			utils::showWarning(this, tr("Error"), tr("Couldn't save file"));
	}
}
#endif

void ColorPaletteDock::selectColor(const QColor &color)
{
	setColor(color);
	emit colorSelected(color);
}

int findPaletteColor(
	const color_widgets::ColorPalette &pal, const QColor &color)
{
	QVector<QPair<QColor, QString>> colors = pal.colors();
	QRgb rgb = color.rgb();
	for(int i = 0; i < colors.length(); ++i) {
		if(colors.at(i).first.rgb() == rgb) {
			return i;
		}
	}
	return -1;
}

}
