// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/layoutsdialog.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "ui_layoutsdialog.h"
#include <QByteArray>
#include <QTimer>
#include <QVector>

namespace dialogs {

struct LayoutsDialog::Layout {
	QString title;
	QByteArray state;
	QString originalTitle;
	bool wasTransient;
	bool transient;
	bool deleted;

	Layout()
		: title{}
		, state{}
		, originalTitle{}
		, wasTransient{false}
		, transient{false}
		, deleted{false}
	{
	}

	Layout(QString title_, QByteArray state_)
		: title{title_}
		, state{state_}
		, originalTitle{}
		, wasTransient{false}
		, transient{false}
		, deleted{false}
	{
	}

	explicit Layout(QByteArray state_)
		: title{}
		, state{state_}
		, originalTitle{}
		, wasTransient{true}
		, transient{true}
		, deleted{false}
	{
	}
};

struct LayoutsDialog::Private {
	struct Predefs {
		QByteArray defaultLayout;
		QByteArray defaultAnimationLayout;
		QByteArray fireAlpacaEsqueLayout;
		QByteArray horizontalLayout;
		QByteArray hyenaLayout;
		QByteArray kritaEsqueLayout;
		QByteArray mediBangEsqueLayout;
		QByteArray minkLayout;
		QByteArray paintNetEsqueLayout;
		QByteArray saiEsqueLayout;
		QByteArray spiderLayout;
		QHash<QByteArray, const QByteArray *> compatibilityMappings;
	};

	Ui_LayoutsDialog ui;
	QVector<Layout> layouts;
	QByteArray originalState;
	bool updateInProgress;

	// Defined at the bottom of the file because it's annoyingly large.
	Predefs getPredefs();

	Private(const QByteArray &currentState)
		: ui{}
		, layouts{}
		, originalState{currentState}
		, updateInProgress{false}
	{
		layouts.append(Layout{currentState});
		const auto savedLayouts = dpApp().settings().layouts();
		Predefs predefs = getPredefs();
		if(savedLayouts.isEmpty()) {
			createDefaultLayouts(predefs);
		} else {
			for(const auto &savedLayout : savedLayouts) {
				QByteArray state = savedLayout.value("state").toByteArray();
				applyCompatibilityMapping(
					predefs, savedLayout.value("title").toString(), state);
				layouts.append(
					Layout{savedLayout.value("title").toString(), state});
			}
		}
	}

	void createDefaultLayouts(const Predefs &predefs)
	{
		layouts = {
			LayoutsDialog::Layout{tr("Default"), predefs.defaultLayout},
			LayoutsDialog::Layout{
				tr("Default Animation"), predefs.defaultAnimationLayout},
			LayoutsDialog::Layout{
				tr("FireAlpaca-esque"), predefs.fireAlpacaEsqueLayout},
			LayoutsDialog::Layout{tr("Horizontal"), predefs.horizontalLayout},
			LayoutsDialog::Layout{tr("Hyena"), predefs.hyenaLayout},
			LayoutsDialog::Layout{tr("Krita-esque"), predefs.kritaEsqueLayout},
			LayoutsDialog::Layout{
				tr("MediBang-esque"), predefs.mediBangEsqueLayout},
			LayoutsDialog::Layout{tr("Mink"), predefs.kritaEsqueLayout},
			LayoutsDialog::Layout{
				tr("Paint.NET-esque"), predefs.paintNetEsqueLayout},
			LayoutsDialog::Layout{tr("SAI-esque"), predefs.saiEsqueLayout},
			LayoutsDialog::Layout{tr("Spider"), predefs.spiderLayout},
		};
	}

	void applyCompatibilityMapping(
		const Predefs &predefs, const QString &s, QByteArray &state)
	{
		QHash<QByteArray, const QByteArray *>::const_iterator it =
			predefs.compatibilityMappings.constFind(state);
		if(it != predefs.compatibilityMappings.constEnd()) {
			qDebug() << s << "mapped";
			state = *(it.value());
		}
	}

	void saveLayouts()
	{
		desktop::settings::Settings::LayoutsType savedLayouts;
		for(const Layout &layout : layouts) {
			if(!layout.transient && !layout.deleted) {
				savedLayouts.append(
					{{"title", layout.title}, {"state", layout.state}});
			}
		}
		dpApp().settings().setLayouts(savedLayouts);
	}

	QListWidgetItem *selectedItem()
	{
		QList<QListWidgetItem *> selected = ui.list->selectedItems();
		return selected.isEmpty() ? nullptr : selected.first();
	}

	Layout *selectedLayout()
	{
		QListWidgetItem *item = selectedItem();
		return item ? item->data(Qt::UserRole).value<Layout *>() : nullptr;
	}

	Layout *transientLayout()
	{
		for(Layout &layout : layouts) {
			if(layout.transient) {
				return &layout;
			}
		}
		return nullptr;
	}

	void updateItem(QListWidgetItem *item)
	{
		Layout *layout = item->data(Qt::UserRole).value<Layout *>();
		if(layout->transient) {
			item->setText(tr("Current (unsaved)"));
		} else if(layout->wasTransient) {
			item->setText(tr("%1 (new)").arg(layout->title));
		} else if(layout->deleted) {
			item->setText(tr("%1 (deleted)").arg(layout->title));
		} else if(!layout->originalTitle.isNull()) {
			item->setText(tr("%1 (renamed from %2)")
							  .arg(layout->title)
							  .arg(layout->originalTitle));
		} else {
			item->setText(layout->title);
		}

		QFont font = item->font();
		font.setItalic(
			layout->wasTransient || layout->deleted ||
			!layout->originalTitle.isNull());
		item->setFont(font);
	}

	void updateList(Layout *selected)
	{
		updateInProgress = true;
		ui.list->clear();
		QListWidgetItem *transientItem = nullptr;
		QListWidgetItem *itemToSelect = nullptr;

		for(Layout &layout : layouts) {
			QListWidgetItem *item = new QListWidgetItem;
			item->setData(Qt::UserRole, QVariant::fromValue(&layout));
			updateItem(item);

			if(layout.transient) {
				transientItem = item;
			} else {
				ui.list->addItem(item);
			}

			if(&layout == selected) {
				itemToSelect = item;
			}
		}

		ui.list->sortItems();
		if(transientItem) {
			ui.list->insertItem(0, transientItem);
		}

		if(itemToSelect) {
			itemToSelect->setSelected(true);
		}
		updateInProgress = false;
	}
};

LayoutsDialog::LayoutsDialog(const QByteArray &currentState, QWidget *parent)
	: QDialog{parent}
	, d{new Private{currentState}}
{
	d->ui.setupUi(this);

	connect(
		d->ui.list, &QListWidget::itemSelectionChanged, this,
		&LayoutsDialog::updateButtons);
	connect(
		d->ui.list, &QListWidget::itemSelectionChanged, this,
		&LayoutsDialog::applySelected);
	connect(
		d->ui.saveButton, &QPushButton::clicked, this, &LayoutsDialog::save);
	connect(
		d->ui.renameButton, &QPushButton::clicked, this,
		&LayoutsDialog::rename);
	connect(
		d->ui.deleteButton, &QPushButton::clicked, this,
		&LayoutsDialog::toggleDeleted);
	connect(this, &QDialog::finished, this, &LayoutsDialog::onFinish);

	d->updateList(d->transientLayout());
	updateButtons();
}

LayoutsDialog::~LayoutsDialog()
{
	delete d;
}

void LayoutsDialog::save()
{
	Layout *layout = d->transientLayout();
	promptTitle(layout, [this, layout](const QString &title) {
		layout->title = title;
		layout->transient = false;
		d->updateList(layout);
		updateButtons();
	});
}

void LayoutsDialog::rename()
{
	Layout *layout = d->selectedLayout();
	promptTitle(layout, [this, layout](const QString &title) {
		if(!layout->wasTransient) {
			if(layout->originalTitle.isNull()) {
				layout->originalTitle = layout->title;
			} else if(layout->originalTitle == title) {
				layout->originalTitle = QString{};
			}
		}
		layout->title = title;
		d->updateList(layout);
		updateButtons();
	});
}

void LayoutsDialog::toggleDeleted()
{
	QListWidgetItem *item = d->selectedItem();
	if(item) {
		Layout *layout = item->data(Qt::UserRole).value<Layout *>();
		if(!layout->transient) {
			if(layout->wasTransient) {
				layout->title = QString{};
				layout->transient = true;
				d->updateList(layout);
				updateButtons();
			} else {
				layout->deleted = !layout->deleted;
				d->updateItem(item);
				updateButtons();
			}
		}
	}
}

void LayoutsDialog::updateButtons()
{
	Layout *selected = d->selectedLayout();
	bool persistentSelected = selected && !selected->transient;
	d->ui.saveButton->setEnabled(selected && selected->transient);
	d->ui.renameButton->setEnabled(persistentSelected);
	d->ui.deleteButton->setEnabled(persistentSelected);
	if(selected && selected->deleted) {
		d->ui.deleteButton->setText(tr("Undelete"));
	} else {
		d->ui.deleteButton->setText(tr("Delete"));
	}
}

void LayoutsDialog::applySelected()
{
	if(!d->updateInProgress) {
		Layout *selected = d->selectedLayout();
		if(selected) {
			emit applyState(selected->state);
			qApp->processEvents();
			activateWindow();
			raise();
		}
	}
}

void LayoutsDialog::onFinish(int result)
{
	if(result == Accepted) {
		d->saveLayouts();
	} else {
		emit applyState(d->originalState);
	}
}

void LayoutsDialog::promptTitle(
	Layout *layout, const std::function<void(const QString &)> &fn)
{
	if(layout) {
		utils::getInputText(
			this, d->ui.saveButton->text(), tr("Layout Name:"), layout->title,
			[layout, fn](const QString &title) {
				QString trimmedTitle = title.trimmed();
				if(!trimmedTitle.isEmpty() && trimmedTitle != layout->title) {
					fn(title);
				}
			});
	}
}


LayoutsDialog::Private::Predefs LayoutsDialog::Private::getPredefs()
{
	Predefs predefs;
	predefs.defaultLayout = QByteArray::fromBase64(
		"AAAA/wAAAAD9AAAAAwAAAAAAAAFcAAAD+vwCAAAAAvsAAAAYAFQAbwBvAGwAUwBlAH"
		"QAdABpAG4AZwBzAQAAAD4AAAFCAAABPwD////7AAAAGABCAHIAdQBzAGgAUABhAGwA"
		"ZQB0AHQAZQEAAAGGAAACsgAAAGMA////AAAAAQAAAQAAAAP6/AIAAAAD/AAAAD4AAA"
		"FHAAAAuwEAABz6AAAAAgIAAAAF+wAAACAAYwBvAGwAbwByAHAAYQBsAGUAdAB0AGUA"
		"ZABvAGMAawEAAAAA/////wAAAE0A////+wAAAB4AYwBvAGwAbwByAHMAbABpAGQAZQ"
		"ByAGQAbwBjAGsBAAAAAP////8AAACeAP////sAAAAgAGMAbwBsAG8AcgBzAHAAaQBu"
		"AG4AZQByAGQAbwBjAGsBAAAAAP////8AAABkAP////sAAAAeAGMAbwBsAG8AcgBjAG"
		"kAcgBjAGwAZQBkAG8AYwBrAQAAAAD/////AAAAbgD////7AAAAGgByAGUAZgBlAHIA"
		"ZQBuAGMAZQBkAG8AYwBrAQAAAAD/////AAAAfAD////7AAAAEgBMAGEAeQBlAHIATA"
		"BpAHMAdAEAAAGLAAACrQAAAJwA////+wAAABoAbgBhAHYAaQBnAGEAdABvAHIAZABv"
		"AGMAawAAAAAA/////wAAAD4A////AAAAAgAABRgAAAC4/AEAAAAB/AAAAWIAAAUYAA"
		"AAAAD////6/////wEAAAAC+wAAABAAVABpAG0AZQBsAGkAbgBlAAAAAAD/////AAAC"
		"UAD////7AAAAFABvAG4AaQBvAG4AcwBrAGkAbgBzAAAAAAD/////AAABZQD///8AAA"
		"UYAAAD+gAAAAEAAAACAAAAAQAAAAL8AAAAAQAAAAIAAAADAAAAGABmAGkAbABlAHQA"
		"bwBvAGwAcwBiAGEAcgEAAAAA/////wAAAAAAAAAAAAAAGABlAGQAaQB0AHQAbwBvAG"
		"wAcwBiAGEAcgEAAACV/////wAAAAAAAAAAAAAAGABkAHIAYQB3AHQAbwBvAGwAcwBi"
		"AGEAcgEAAAFr/////wAAAAAAAAAA");
	predefs.defaultAnimationLayout = QByteArray::fromBase64(
		"AAAA/wAAAAD9AAAAAwAAAAAAAAFcAAAD+vwCAAAAAvsAAAAYAFQAbwBvAGwAUwBlAH"
		"QAdABpAG4AZwBzAQAAAD4AAAFCAAABPwD////7AAAAGABCAHIAdQBzAGgAUABhAGwA"
		"ZQB0AHQAZQEAAAGGAAACsgAAAGMA////AAAAAQAAAQAAAAP6/AIAAAAD/AAAAD4AAA"
		"FHAAAAuwEAABz6AAAAAgIAAAAF+wAAACAAYwBvAGwAbwByAHAAYQBsAGUAdAB0AGUA"
		"ZABvAGMAawEAAAAA/////wAAAE0A////+wAAAB4AYwBvAGwAbwByAHMAbABpAGQAZQ"
		"ByAGQAbwBjAGsBAAAAAP////8AAACeAP////sAAAAgAGMAbwBsAG8AcgBzAHAAaQBu"
		"AG4AZQByAGQAbwBjAGsBAAAAAP////8AAABkAP////sAAAAeAGMAbwBsAG8AcgBjAG"
		"kAcgBjAGwAZQBkAG8AYwBrAQAAAAD/////AAAAbgD////7AAAAGgByAGUAZgBlAHIA"
		"ZQBuAGMAZQBkAG8AYwBrAQAAAAD/////AAAAfAD////7AAAAEgBMAGEAeQBlAHIATA"
		"BpAHMAdAEAAAGLAAACrQAAAJwA////+wAAABoAbgBhAHYAaQBnAGEAdABvAHIAZABv"
		"AGMAawAAAAAA/////wAAAD4A////AAAAAgAABRgAAAC4/AEAAAAB/AAAAWIAAAUYAA"
		"ACUAD////6AAAAAAEAAAAC+wAAABAAVABpAG0AZQBsAGkAbgBlAQAAAAD/////AAAC"
		"UAD////7AAAAFABvAG4AaQBvAG4AcwBrAGkAbgBzAQAAAAD/////AAABZQD///8AAA"
		"UYAAADPAAAAAEAAAACAAAAAQAAAAL8AAAAAQAAAAIAAAADAAAAGABmAGkAbABlAHQA"
		"bwBvAGwAcwBiAGEAcgEAAAAA/////wAAAAAAAAAAAAAAGABlAGQAaQB0AHQAbwBvAG"
		"wAcwBiAGEAcgEAAACV/////wAAAAAAAAAAAAAAGABkAHIAYQB3AHQAbwBvAGwAcwBi"
		"AGEAcgEAAAFr/////wAAAAAAAAAA");
	predefs.fireAlpacaEsqueLayout = QByteArray::fromBase64(
		"AAAA/wAAAAD9AAAAAwAAAAAAAAFkAAAD5/wCAAAAA/wAAAA9AAAA2QAAANkA/////A"
		"EAAAAC/AAAACgAAADeAAAAXgD////6AAAAAAIAAAAC+wAAAB4AYwBvAGwAbwByAHMA"
		"bABpAGQAZQByAGQAbwBjAGsBAAAAPQAAAQMAAADZAP////sAAAAgAGMAbwBsAG8Acg"
		"BwAGEAbABlAHQAdABlAGQAbwBjAGsAAAAAAP////8AAAB9AP////sAAAAgAGMAbwBs"
		"AG8AcgBzAHAAaQBuAG4AZQByAGQAbwBjAGsBAAABDAAAAIAAAABVAP////sAAAAYAF"
		"QAbwBvAGwAUwBlAHQAdABpAG4AZwBzAQAAARwAAAFqAAABIgD////7AAAAGABCAHIA"
		"dQBzAGgAUABhAGwAZQB0AHQAZQEAAAKMAAABmAAAAGMA////AAAAAQAAAQAAAAPn/A"
		"IAAAAC+wAAABoAbgBhAHYAaQBnAGEAdABvAHIAZABvAGMAawEAAAA9AAABFQAAABsA"
		"////+wAAABIATABhAHkAZQByAEwAaQBzAHQBAAABWAAAAswAAACcAP///wAAAAIAAA"
		"UQAAAAuPwBAAAAAfwAAAFqAAAFEAAAAAAA////+v////8BAAAAAvsAAAAUAG8AbgBp"
		"AG8AbgBzAGsAaQBuAHMAAAAAAP////8AAAFlAP////sAAAAQAFQAaQBtAGUAbABpAG"
		"4AZQAAAAAA/////wAAATwA////AAAE6AAAA+cAAAABAAAAAgAAAAEAAAAC/AAAAAIA"
		"AAAAAAAAAQAAABgAZAByAGEAdwB0AG8AbwBsAHMAYgBhAHIDAAAAAP////8AAAAAAA"
		"AAAAAAAAIAAAACAAAAGABlAGQAaQB0AHQAbwBvAGwAcwBiAGEAcgEAAAAA/////wAA"
		"AAAAAAAAAAAAGABmAGkAbABlAHQAbwBvAGwAcwBiAGEAcgEAAAC1AAAGywAAAAAAAA"
		"AA");
	predefs.horizontalLayout = QByteArray::fromBase64(
		"AAAA/wAAAAD9AAAAAwAAAAEAAAHyAAAD+vwCAAAAAfsAAAAaAG4AYQB2AGkAZwBhAH"
		"QAbwByAGQAbwBjAGsAAAAAAP////8AAAA+AP///wAAAAIAAAeAAAABPPwBAAAAAfsA"
		"AAAUAG8AbgBpAG8AbgBzAGsAaQBuAHMAAAAAAAAAB4AAAAFlAP///wAAAAMAAAeAAA"
		"ABP/wBAAAABPwAAAAAAAABJwAAAJcA////+gAAAAICAAAABfsAAAAaAHIAZQBmAGUA"
		"cgBlAG4AYwBlAGQAbwBjAGsBAAAAAP////8AAAB8AP////sAAAAeAGMAbwBsAG8Acg"
		"BjAGkAcgBjAGwAZQBkAG8AYwBrAQAAAAD/////AAAAbgD////7AAAAIABjAG8AbABv"
		"AHIAcwBwAGkAbgBuAGUAcgBkAG8AYwBrAQAAA0sAAADZAAAAZAD////7AAAAHgBjAG"
		"8AbABvAHIAcwBsAGkAZABlAHIAZABvAGMAawEAAAAA/////wAAAJ4A////+wAAACAA"
		"YwBvAGwAbwByAHAAYQBsAGUAdAB0AGUAZABvAGMAawEAAAAA/////wAAAE0A////+w"
		"AAABgAVABvAG8AbABTAGUAdAB0AGkAbgBnAHMBAAABLQAAAZIAAADrAP////wAAALF"
		"AAACrQAAAG8A/////AIAAAAC+wAAABgAQgByAHUAcwBoAFAAYQBsAGUAdAB0AGUBAA"
		"AC+QAAAT8AAABjAP////sAAAAQAFQAaQBtAGUAbABpAG4AZQAAAAOWAAAAjgAAAHcA"
		"////+wAAABIATABhAHkAZQByAEwAaQBzAHQBAAAFeAAAAggAAACTAP///wAAB4AAAA"
		"K1AAAAAQAAAAIAAAABAAAAAvwAAAABAAAAAgAAAAMAAAAYAGYAaQBsAGUAdABvAG8A"
		"bABzAGIAYQByAQAAAAD/////AAAAAAAAAAAAAAAYAGUAZABpAHQAdABvAG8AbABzAG"
		"IAYQByAQAAAJX/////AAAAAAAAAAAAAAAYAGQAcgBhAHcAdABvAG8AbABzAGIAYQBy"
		"AQAAAWv/////AAAAAAAAAAA=");
	predefs.hyenaLayout = QByteArray::fromBase64(
		"AAAA/wAAAAD9AAAAAgAAAAAAAAEvAAAD+vwCAAAABPsAAAAaAG4AYQB2AGkAZwBhAH"
		"QAbwByAGQAbwBjAGsBAAAAFgAAAKYAAAA+AP////sAAAASAEwAYQB5AGUAcgBMAGkA"
		"cwB0AQAAAMIAAADoAAAAnAD////8AAABsAAAAV8AAAFcAQAAHPoAAAAAAQAAAAL7AA"
		"AAGABUAG8AbwBsAFMAZQB0AHQAaQBuAGcAcwEAAAAA/////wAAAOsA////+wAAABgA"
		"QgByAHUAcwBoAFAAYQBsAGUAdAB0AGUBAAAAAAAAAS8AAABvAP////wAAAMVAAAA+w"
		"AAALsBAAAc+gAAAAABAAAABfsAAAAgAGMAbwBsAG8AcgBzAHAAaQBuAG4AZQByAGQA"
		"bwBjAGsBAAAAAAAAAS8AAABIAP////sAAAAeAGMAbwBsAG8AcgBzAGwAaQBkAGUAcg"
		"BkAG8AYwBrAQAAAAD/////AAAAiQD////7AAAAIABjAG8AbABvAHIAcABhAGwAZQB0"
		"AHQAZQBkAG8AYwBrAQAAAAD/////AAAAUgD////7AAAAHgBjAG8AbABvAHIAYwBpAH"
		"IAYwBsAGUAZABvAGMAawEAAAAA/////wAAAFIA////+wAAABoAcgBlAGYAZQByAGUA"
		"bgBjAGUAZABvAGMAawEAAAAA/////wAAAJcA////AAAAAgAABRAAAAC4/AEAAAAB/A"
		"AAAWoAAAUQAAAAAAD////6/////wEAAAAC+wAAABQAbwBuAGkAbwBuAHMAawBpAG4A"
		"cwAAAAAA/////wAAAWUA////+wAAABAAVABpAG0AZQBsAGkAbgBlAAAAAAD/////AA"
		"ACUAD///8AAAZLAAAD+gAAAAEAAAACAAAAAQAAAAL8AAAAAQAAAAMAAAADAAAAGABl"
		"AGQAaQB0AHQAbwBvAGwAcwBiAGEAcgEAAAAA/////wAAAAAAAAAAAAAAGABmAGkAbA"
		"BlAHQAbwBvAGwAcwBiAGEAcgEAAADWAAADDgAAAAAAAAAAAAAAGABkAHIAYQB3AHQA"
		"bwBvAGwAcwBiAGEAcgEAAAPkAAADvQAAAAAAAAAA");
	predefs.kritaEsqueLayout = QByteArray::fromBase64(
		"AAAA/wAAAAD9AAAAAwAAAAAAAAFTAAAD5/wCAAAAAfsAAAASAGMAbwBsAG8AcgBkAG"
		"8AYwBrAQAAAD0AAADaAAAAAAAAAAAAAAABAAABSQAAA+f8AgAAAAf8AAAAPQAAAVkA"
		"AAE/AQAAHPoAAAAAAQAAAAL7AAAAIABjAG8AbABvAHIAcwBwAGkAbgBuAGUAcgBkAG"
		"8AYwBrAQAABjcAAAFJAAAAVQD////7AAAAGABUAG8AbwBsAFMAZQB0AHQAaQBuAGcA"
		"cwEAAAAA/////wAAAMYA////+wAAABIATABhAHkAZQByAEwAaQBzAHQBAAABnAAAAP"
		"cAAACcAP////sAAAAYAEIAcgB1AHMAaABQAGEAbABlAHQAdABlAQAAApkAAAGLAAAA"
		"YwD////8AAAAPQAAA+cAAAAAAP////oAAAAAAQAAAAH7AAAAGgBJAG4AcAB1AHQAUw"
		"BlAHQAdABpAG4AZwBzAQAAAAD/////AAAAAAAAAAD7AAAAGgBuAGEAdgBpAGcAYQB0"
		"AG8AcgBkAG8AYwBrAAAAAD0AAAPnAAAAGwD////7AAAAIABjAG8AbABvAHIAcABhAG"
		"wAZQB0AHQAZQBkAG8AYwBrAAAAAD0AAAPnAAAAfQD////7AAAAHgBjAG8AbABvAHIA"
		"cwBsAGkAZABlAHIAZABvAGMAawAAAAAWAAAEDgAAANkA////AAAAAgAABgkAAAEi/A"
		"EAAAAC+wAAABAAVABpAG0AZQBsAGkAbgBlAAAAAXwAAAKxAAABPAD////7AAAAFABv"
		"AG4AaQBvAG4AcwBrAGkAbgBzAAAAAWoAAAXuAAABZQD///8AAAYJAAAD5wAAAAEAAA"
		"ACAAAAAQAAAAL8AAAAAgAAAAAAAAABAAAAGABkAHIAYQB3AHQAbwBvAGwAcwBiAGEA"
		"cgMAAAAA/////wAAAAAAAAAAAAAAAgAAAAIAAAAYAGYAaQBsAGUAdABvAG8AbABzAG"
		"IAYQByAQAAAAD/////AAAAAAAAAAAAAAAYAGUAZABpAHQAdABvAG8AbABzAGIAYQBy"
		"AQAAAJQAAAbsAAAAAAAAAAA=");
	predefs.mediBangEsqueLayout = QByteArray::fromBase64(
		"AAAA/wAAAAD9AAAAAwAAAAAAAAExAAAD+vwCAAAAA/wAAAA+AAABMAAAALsBAAAc+g"
		"AAAAACAAAABfsAAAAgAGMAbwBsAG8AcgBzAHAAaQBuAG4AZQByAGQAbwBjAGsBAAAA"
		"PQAAANkAAABkAP////sAAAAeAGMAbwBsAG8AcgBzAGwAaQBkAGUAcgBkAG8AYwBrAQ"
		"AAAAD/////AAAAngD////7AAAAIABjAG8AbABvAHIAcABhAGwAZQB0AHQAZQBkAG8A"
		"YwBrAQAAAAD/////AAAATQD////7AAAAHgBjAG8AbABvAHIAYwBpAHIAYwBsAGUAZA"
		"BvAGMAawEAAAAA/////wAAAG4A////+wAAABoAcgBlAGYAZQByAGUAbgBjAGUAZABv"
		"AGMAawEAAAAA/////wAAAHwA////+wAAABgAVABvAG8AbABTAGUAdAB0AGkAbgBnAH"
		"MBAAABdAAAAT8AAAE/AP////sAAAAYAEIAcgB1AHMAaABQAGEAbABlAHQAdABlAQAA"
		"ArkAAAF/AAAAYwD///8AAAABAAABJAAAA/r8AgAAAAL7AAAAGgBuAGEAdgBpAGcAYQ"
		"B0AG8AcgBkAG8AYwBrAQAAAD4AAAFpAAAAPgD////7AAAAEgBMAGEAeQBlAHIATABp"
		"AHMAdAEAAAGtAAACiwAAAJwA////AAAAAgAABRAAAAC4/AEAAAAB/AAAAWoAAAUQAA"
		"AAAAD////6/////wEAAAAC+wAAABQAbwBuAGkAbwBuAHMAawBpAG4AcwAAAAAA////"
		"/wAAAWUA////+wAAABAAVABpAG0AZQBsAGkAbgBlAAAAAAD/////AAACUAD///8AAA"
		"T3AAAD+gAAAAEAAAACAAAAAQAAAAL8AAAAAgAAAAAAAAABAAAAGABkAHIAYQB3AHQA"
		"bwBvAGwAcwBiAGEAcgMAAAAA/////wAAAAAAAAAAAAAAAgAAAAIAAAAYAGYAaQBsAG"
		"UAdABvAG8AbABzAGIAYQByAQAAAAD/////AAAAAAAAAAAAAAAYAGUAZABpAHQAdABv"
		"AG8AbABzAGIAYQByAQAAAJUAAAbsAAAAAAAAAAA=");
	predefs.minkLayout = QByteArray::fromBase64(
		"AAAA/wAAAAD9AAAAAwAAAAAAAAFTAAAEDvwCAAAABPsAAAASAGMAbwBsAG8AcgBkAG"
		"8AYwBrAQAAAD0AAADaAAAAAAAAAAD8AAAAFgAAAP4AAABjAP////wBAAAAAvsAAAAg"
		"AGMAbwBsAG8AcgBzAHAAaQBuAG4AZQByAGQAbwBjAGsBAAAAAAAAAM4AAABVAP////"
		"sAAAAYAEIAcgB1AHMAaABQAGEAbABlAHQAdABlAQAAANQAAAB/AAAAfQD////7AAAA"
		"EgBMAGEAeQBlAHIATABpAHMAdAEAAAEaAAABZwAAAH8A////+wAAABgAVABvAG8AbA"
		"BTAGUAdAB0AGkAbgBnAHMBAAAChwAAAZ0AAAEiAP///wAAAAEAAAFJAAAEDvwCAAAA"
		"BPwAAAA9AAAD5wAAAAAA////+gAAAAABAAAAAfsAAAAaAEkAbgBwAHUAdABTAGUAdA"
		"B0AGkAbgBnAHMBAAAAAP////8AAAAAAAAAAPsAAAAaAG4AYQB2AGkAZwBhAHQAbwBy"
		"AGQAbwBjAGsAAAAAPQAAA+cAAAAbAP////sAAAAgAGMAbwBsAG8AcgBwAGEAbABlAH"
		"QAdABlAGQAbwBjAGsAAAAAPQAAA+cAAAB9AP////sAAAAeAGMAbwBsAG8AcgBzAGwA"
		"aQBkAGUAcgBkAG8AYwBrAAAAABYAAAQOAAAA2QD///8AAAACAAAF7gAAAN38AQAAAA"
		"L7AAAAEABUAGkAbQBlAGwAaQBuAGUAAAABfAAAArEAAAE8AP////sAAAAUAG8AbgBp"
		"AG8AbgBzAGsAaQBuAHMAAAABagAABe4AAAFlAP///wAABf8AAAQOAAAAAQAAAAIAAA"
		"ABAAAAAvwAAAABAAAAAQAAAAMAAAAYAGYAaQBsAGUAdABvAG8AbABzAGIAYQByAwAA"
		"AAD/////AAAAAAAAAAAAAAAYAGUAZABpAHQAdABvAG8AbABzAGIAYQByAwAAAJD///"
		"//AAAAAAAAAAAAAAAYAGQAcgBhAHcAdABvAG8AbABzAGIAYQByAwAAAUD/////AAAA"
		"AAAAAAA=");
	predefs.paintNetEsqueLayout = QByteArray::fromBase64(
		"AAAA/wAAAAD9AAAAAwAAAAAAAAFkAAAD5/wCAAAAAvsAAAAYAFQAbwBvAGwAUwBlAH"
		"QAdABpAG4AZwBzAwAAA6MAAAByAAABFgAAAV77AAAAGABCAHIAdQBzAGgAUABhAGwA"
		"ZQB0AHQAZQMAAABAAAAAiQAAAScAAADCAAAAAQAAAQAAAAPn/AIAAAAD/AAAAD0AAA"
		"J0AAAAAAD////6/////wIAAAAD+wAAACAAYwBvAGwAbwByAHAAYQBsAGUAdAB0AGUA"
		"ZABvAGMAawAAAAAA/////wAAAH0A////+wAAAB4AYwBvAGwAbwByAHMAbABpAGQAZQ"
		"ByAGQAbwBjAGsCAAAKwAAAAcUAAAEAAAACV/sAAAAgAGMAbwBsAG8AcgBzAHAAaQBu"
		"AG4AZQByAGQAbwBjAGsDAAAATwAAAb4AAAEHAAABEvsAAAASAEwAYQB5AGUAcgBMAG"
		"kAcwB0AwAAA9QAAAHyAAABEAAAAS/7AAAAGgBuAGEAdgBpAGcAYQB0AG8AcgBkAG8A"
		"YwBrAAAAAAD/////AAAAGwD///8AAAACAAAGFgAAALj8AQAAAAH8AAABagAABhYAAA"
		"AAAP////r/////AQAAAAL7AAAAFABvAG4AaQBvAG4AcwBrAGkAbgBzAAAAAAD/////"
		"AAABZQD////7AAAAEABUAGkAbQBlAGwAaQBuAGUAAAAAAP////8AAAE8AP///wAAB4"
		"AAAAP7AAAAAQAAAAIAAAABAAAAAvwAAAABAAAAAgAAAAMAAAAYAGYAaQBsAGUAdABv"
		"AG8AbABzAGIAYQByAQAAAAD/////AAAAAAAAAAAAAAAYAGUAZABpAHQAdABvAG8AbA"
		"BzAGIAYQByAQAAAJT/////AAAAAAAAAAAAAAAYAGQAcgBhAHcAdABvAG8AbABzAGIA"
		"YQByAQAAAUn/////AAAAAAAAAAA=");
	predefs.saiEsqueLayout = QByteArray::fromBase64(
		"AAAA/wAAAAD9AAAAAgAAAAAAAAIFAAAD+vwCAAAAAvwAAAA+AAABVwAAALsA/////A"
		"EAAAAC+wAAABoAbgBhAHYAaQBnAGEAdABvAHIAZABvAGMAawEAAAAAAAAA+gAAAHoA"
		"/////AAAAQAAAAEFAAAAlwD////6AAAAAAIAAAAF+wAAACAAYwBvAGwAbwByAHMAcA"
		"BpAG4AbgBlAHIAZABvAGMAawEAAAA9AAABOwAAAGQA////+wAAACAAYwBvAGwAbwBy"
		"AHAAYQBsAGUAdAB0AGUAZABvAGMAawEAAAAA/////wAAAE0A////+wAAAB4AYwBvAG"
		"wAbwByAHMAbABpAGQAZQByAGQAbwBjAGsBAAAAAP////8AAACeAP////sAAAAeAGMA"
		"bwBsAG8AcgBjAGkAcgBjAGwAZQBkAG8AYwBrAQAAAAD/////AAAAbgD////7AAAAGg"
		"ByAGUAZgBlAHIAZQBuAGMAZQBkAG8AYwBrAQAAAAD/////AAAAfAD////8AAABmwAA"
		"Ap0AAAGoAP////wBAAAAAvsAAAASAEwAYQB5AGUAcgBMAGkAcwB0AQAAAAAAAAD6AA"
		"AAkwD////8AAABAAAAAQUAAADrAP////wCAAAAAvsAAAAYAEIAcgB1AHMAaABQAGEA"
		"bABlAHQAdABlAQAAAZsAAAEaAAAAYwD////7AAAAGABUAG8AbwBsAFMAZQB0AHQAaQ"
		"BuAGcAcwEAAAK7AAABfQAAAT8A////AAAAAgAABRAAAAC4/AEAAAAB/AAAAWoAAAUQ"
		"AAAAAAD////6/////wEAAAAC+wAAABQAbwBuAGkAbwBuAHMAawBpAG4AcwAAAAAA//"
		"///wAAAWUA////+wAAABAAVABpAG0AZQBsAGkAbgBlAAAAAAD/////AAACUAD///8A"
		"AAV1AAAD+gAAAAEAAAACAAAAAQAAAAL8AAAAAQAAAAIAAAADAAAAGABmAGkAbABlAH"
		"QAbwBvAGwAcwBiAGEAcgEAAAAA/////wAAAAAAAAAAAAAAGABlAGQAaQB0AHQAbwBv"
		"AGwAcwBiAGEAcgEAAACV/////wAAAAAAAAAAAAAAGABkAHIAYQB3AHQAbwBvAGwAcw"
		"BiAGEAcgEAAAFr/////wAAAAAAAAAA");
	predefs.spiderLayout = QByteArray::fromBase64(
		"AAAA/wAAAAD9AAAAAwAAAAAAAAEvAAAD+vwCAAAAAfsAAAAYAEIAcgB1AHMAaABQAG"
		"EAbABlAHQAdABlAQAAAD4AAAP6AAAAYwD///8AAAABAAABPgAAA/r8AgAAAAT7AAAA"
		"GABUAG8AbwBsAFMAZQB0AHQAaQBuAGcAcwEAAAA+AAABSAAAAT8A/////AAAAYwAAA"
		"E3AAAAuwEAABz6AAAAAgIAAAAF+wAAACAAYwBvAGwAbwByAHAAYQBsAGUAdAB0AGUA"
		"ZABvAGMAawEAAAAA/////wAAAE0A////+wAAAB4AYwBvAGwAbwByAHMAbABpAGQAZQ"
		"ByAGQAbwBjAGsBAAAAAP////8AAACeAP////sAAAAgAGMAbwBsAG8AcgBzAHAAaQBu"
		"AG4AZQByAGQAbwBjAGsBAAAAAP////8AAABkAP////sAAAAeAGMAbwBsAG8AcgBjAG"
		"kAcgBjAGwAZQBkAG8AYwBrAQAAAAD/////AAAAbgD////7AAAAGgByAGUAZgBlAHIA"
		"ZQBuAGMAZQBkAG8AYwBrAQAAAAD/////AAAAfAD////7AAAAEgBMAGEAeQBlAHIATA"
		"BpAHMAdAEAAALJAAABbwAAAJwA////+wAAABoAbgBhAHYAaQBnAGEAdABvAHIAZABv"
		"AGMAawAAAAAA/////wAAAD4A////AAAAAgAABRAAAAC4/AEAAAAB/AAAAWoAAAUQAA"
		"AAAAD////6/////wEAAAAC+wAAABQAbwBuAGkAbwBuAHMAawBpAG4AcwAAAAAA////"
		"/wAAAWUA////+wAAABAAVABpAG0AZQBsAGkAbgBlAAAAAAD/////AAACUAD///8AAA"
		"UHAAAD+gAAAAEAAAACAAAAAQAAAAL8AAAAAQAAAAIAAAADAAAAGABmAGkAbABlAHQA"
		"bwBvAGwAcwBiAGEAcgEAAAAA/////wAAAAAAAAAAAAAAGABlAGQAaQB0AHQAbwBvAG"
		"wAcwBiAGEAcgEAAACV/////wAAAAAAAAAAAAAAGABkAHIAYQB3AHQAbwBvAGwAcwBi"
		"AGEAcgEAAAFr/////wAAAAAAAAAA");
	predefs.compatibilityMappings.insert(
		QByteArray::fromBase64(
			"AAAA/wAAAAD9AAAAAwAAAAAAAAFcAAAD5/wCAAAAAvsAAAAYAFQAbwBvAGwAUwBlAH"
			"QAdABpAG4AZwBzAQAAAD0AAAE8AAABPAD////7AAAAGABCAHIAdQBzAGgAUABhAGwA"
			"ZQB0AHQAZQEAAAF/AAACpQAAAGMA////AAAAAQAAAQAAAAPn/AIAAAAD/AAAAD0AAA"
			"FBAAAA9gEAABz6AAAAAgIAAAAD+wAAACAAYwBvAGwAbwByAHAAYQBsAGUAdAB0AGUA"
			"ZABvAGMAawEAAAAA/////wAAAH0A////+wAAAB4AYwBvAGwAbwByAHMAbABpAGQAZQ"
			"ByAGQAbwBjAGsBAAAAAP////8AAADZAP////sAAAAgAGMAbwBsAG8AcgBzAHAAaQBu"
			"AG4AZQByAGQAbwBjAGsBAAAAAP////8AAAAcAP////sAAAASAEwAYQB5AGUAcgBMAG"
			"kAcwB0AQAAAYQAAAKgAAAAnAD////7AAAAGgBuAGEAdgBpAGcAYQB0AG8AcgBkAG8A"
			"YwBrAAAAAAD/////AAAAGwD///8AAAACAAAFGAAAALj8AQAAAAH8AAABYgAABRgAAA"
			"AAAP////r/////AQAAAAL7AAAAEABUAGkAbQBlAGwAaQBuAGUAAAAAAP////8AAAE8"
			"AP////sAAAAUAG8AbgBpAG8AbgBzAGsAaQBuAHMAAAAAAP////8AAAFlAP///wAABR"
			"gAAAPnAAAAAQAAAAIAAAABAAAAAvwAAAABAAAAAgAAAAMAAAAYAGYAaQBsAGUAdABv"
			"AG8AbABzAGIAYQByAQAAAAD/////AAAAAAAAAAAAAAAYAGUAZABpAHQAdABvAG8AbA"
			"BzAGIAYQByAQAAAJT/////AAAAAAAAAAAAAAAYAGQAcgBhAHcAdABvAG8AbABzAGIA"
			"YQByAQAAAUn/////AAAAAAAAAAA="),
		&predefs.defaultLayout);
	predefs.compatibilityMappings.insert(
		QByteArray::fromBase64(
			"AAAA/wAAAAD9AAAAAwAAAAAAAAFcAAAD5/wCAAAAAvsAAAAYAFQAbwBvAGwAUwBlAH"
			"QAdABpAG4AZwBzAQAAAD0AAAE8AAABPAD////7AAAAGABCAHIAdQBzAGgAUABhAGwA"
			"ZQB0AHQAZQEAAAF/AAACpQAAAGMA////AAAAAQAAAQAAAAPn/AIAAAAD/AAAAD0AAA"
			"FBAAAA9gEAABz6AAAAAgIAAAAD+wAAACAAYwBvAGwAbwByAHAAYQBsAGUAdAB0AGUA"
			"ZABvAGMAawEAAAAA/////wAAAH0A////+wAAAB4AYwBvAGwAbwByAHMAbABpAGQAZQ"
			"ByAGQAbwBjAGsBAAAAAP////8AAADZAP////sAAAAgAGMAbwBsAG8AcgBzAHAAaQBu"
			"AG4AZQByAGQAbwBjAGsBAAAAAP////8AAAAcAP////sAAAASAEwAYQB5AGUAcgBMAG"
			"kAcwB0AQAAAYQAAAKgAAAAnAD////7AAAAGgBuAGEAdgBpAGcAYQB0AG8AcgBkAG8A"
			"YwBrAAAAAAD/////AAAAGwD///8AAAACAAAFGAAAALj8AQAAAAH8AAABYgAABRgAAA"
			"FlAP////oAAAAAAQAAAAL7AAAAEABUAGkAbQBlAGwAaQBuAGUBAAAAAP////8AAAE8"
			"AP////sAAAAUAG8AbgBpAG8AbgBzAGsAaQBuAHMBAAAAAP////8AAAFlAP///wAABR"
			"gAAAMpAAAAAQAAAAIAAAABAAAAAvwAAAABAAAAAgAAAAMAAAAYAGYAaQBsAGUAdABv"
			"AG8AbABzAGIAYQByAQAAAAD/////AAAAAAAAAAAAAAAYAGUAZABpAHQAdABvAG8AbA"
			"BzAGIAYQByAQAAAJT/////AAAAAAAAAAAAAAAYAGQAcgBhAHcAdABvAG8AbABzAGIA"
			"YQByAQAAAUn/////AAAAAAAAAAA="),
		&predefs.defaultAnimationLayout);
	predefs.compatibilityMappings.insert(
		QByteArray::fromBase64(
			"AAAA/wAAAAD9AAAAAwAAAAEAAAHyAAAD5/wCAAAAAfsAAAAaAG4AYQB2AGkAZwBhAH"
			"QAbwByAGQAbwBjAGsAAAAAAP////8AAAAbAP///wAAAAIAAAeAAAABPPwBAAAAAfsA"
			"AAAUAG8AbgBpAG8AbgBzAGsAaQBuAHMAAAAAAAAAB4AAAAFlAP///wAAAAMAAAeAAA"
			"ABPPwBAAAABPwAAAAAAAABJwAAAN4A////+gAAAAACAAAAA/sAAAAgAGMAbwBsAG8A"
			"cgBzAHAAaQBuAG4AZQByAGQAbwBjAGsBAAADSwAAANkAAAAcAP////sAAAAeAGMAbw"
			"BsAG8AcgBzAGwAaQBkAGUAcgBkAG8AYwBrAQAAAAD/////AAAA2QD////7AAAAIABj"
			"AG8AbABvAHIAcABhAGwAZQB0AHQAZQBkAG8AYwBrAQAAAAD/////AAAAfQD////7AA"
			"AAGABUAG8AbwBsAFMAZQB0AHQAaQBuAGcAcwEAAAEtAAABkgAAANIA/////AAAAsUA"
			"AAKtAAAAfQD////8AgAAAAL7AAAAGABCAHIAdQBzAGgAUABhAGwAZQB0AHQAZQEAAA"
			"LoAAABPAAAAGMA////+wAAABAAVABpAG0AZQBsAGkAbgBlAAAAA5YAAACOAAAARgD/"
			"///7AAAAEgBMAGEAeQBlAHIATABpAHMAdAEAAAV4AAACCAAAAJcA////AAAHgAAAAq"
			"UAAAABAAAAAgAAAAEAAAAC/AAAAAEAAAACAAAAAwAAABgAZgBpAGwAZQB0AG8AbwBs"
			"AHMAYgBhAHIBAAAAAP////8AAAAAAAAAAAAAABgAZQBkAGkAdAB0AG8AbwBsAHMAYg"
			"BhAHIBAAAAlP////8AAAAAAAAAAAAAABgAZAByAGEAdwB0AG8AbwBsAHMAYgBhAHIB"
			"AAABSf////8AAAAAAAAAAA=="),
		&predefs.horizontalLayout);
	predefs.compatibilityMappings.insert(
		QByteArray::fromBase64(
			"AAAA/wAAAAD9AAAAAgAAAAAAAAEvAAAD5/wCAAAABPsAAAAaAG4AYQB2AGkAZwBhAH"
			"QAbwByAGQAbwBjAGsBAAAAFgAAAKMAAAAbAP////sAAAASAEwAYQB5AGUAcgBMAGkA"
			"cwB0AQAAAL8AAADjAAAAnAD////8AAABqAAAAVkAAAFZAQAAHPoAAAAAAQAAAAL7AA"
			"AAGABUAG8AbwBsAFMAZQB0AHQAaQBuAGcAcwEAAAAA/////wAAANIA////+wAAABgA"
			"QgByAHUAcwBoAFAAYQBsAGUAdAB0AGUBAAAAAAAAAS8AAAB9AP////wAAAMHAAAA9g"
			"AAAPYBAAAc+gAAAAABAAAAA/sAAAAgAGMAbwBsAG8AcgBzAHAAaQBuAG4AZQByAGQA"
			"bwBjAGsBAAAAAAAAAS8AAABVAP////sAAAAeAGMAbwBsAG8AcgBzAGwAaQBkAGUAcg"
			"BkAG8AYwBrAQAAAAD/////AAAAXgD////7AAAAIABjAG8AbABvAHIAcABhAGwAZQB0"
			"AHQAZQBkAG8AYwBrAQAAAAD/////AAAA3gD///8AAAACAAAFEAAAALj8AQAAAAH8AA"
			"ABagAABRAAAAAAAP////r/////AQAAAAL7AAAAFABvAG4AaQBvAG4AcwBrAGkAbgBz"
			"AAAAAAD/////AAABZQD////7AAAAEABUAGkAbQBlAGwAaQBuAGUAAAAAAP////8AAA"
			"E8AP///wAABksAAAPnAAAAAQAAAAIAAAABAAAAAvwAAAABAAAAAwAAAAMAAAAYAGUA"
			"ZABpAHQAdABvAG8AbABzAGIAYQByAQAAAAD/////AAAAAAAAAAAAAAAYAGYAaQBsAG"
			"UAdABvAG8AbABzAGIAYQByAQAAALUAAAMOAAAAAAAAAAAAAAAYAGQAcgBhAHcAdABv"
			"AG8AbABzAGIAYQByAQAAA8MAAAO9AAAAAAAAAAA="),
		&predefs.hyenaLayout);
	predefs.compatibilityMappings.insert(
		QByteArray::fromBase64(
			"AAAA/wAAAAD9AAAAAwAAAAAAAAFTAAAD5/wCAAAAAfsAAAASAGMAbwBsAG8AcgBkAG"
			"8AYwBrAQAAAD0AAADaAAAAAAAAAAAAAAABAAABSQAAA+f8AgAAAAf8AAAAPQAAAVkA"
			"AAE/AQAAHPoAAAAAAQAAAAL7AAAAIABjAG8AbABvAHIAcwBwAGkAbgBuAGUAcgBkAG"
			"8AYwBrAQAABjcAAAFJAAAAVQD////7AAAAGABUAG8AbwBsAFMAZQB0AHQAaQBuAGcA"
			"cwEAAAAA/////wAAAMYA////+wAAABIATABhAHkAZQByAEwAaQBzAHQBAAABnAAAAP"
			"cAAACcAP////sAAAAYAEIAcgB1AHMAaABQAGEAbABlAHQAdABlAQAAApkAAAGLAAAA"
			"YwD////8AAAAPQAAA+cAAAAAAP////oAAAAAAQAAAAH7AAAAGgBJAG4AcAB1AHQAUw"
			"BlAHQAdABpAG4AZwBzAQAAAAD/////AAAAAAAAAAD7AAAAGgBuAGEAdgBpAGcAYQB0"
			"AG8AcgBkAG8AYwBrAAAAAD0AAAPnAAAAGwD////7AAAAIABjAG8AbABvAHIAcABhAG"
			"wAZQB0AHQAZQBkAG8AYwBrAAAAAD0AAAPnAAAAfQD////7AAAAHgBjAG8AbABvAHIA"
			"cwBsAGkAZABlAHIAZABvAGMAawAAAAAWAAAEDgAAANkA////AAAAAgAABgkAAAEi/A"
			"EAAAAC+wAAABAAVABpAG0AZQBsAGkAbgBlAAAAAXwAAAKxAAABPAD////7AAAAFABv"
			"AG4AaQBvAG4AcwBrAGkAbgBzAAAAAWoAAAXuAAABZQD///8AAAYJAAAD5wAAAAEAAA"
			"ACAAAAAQAAAAL8AAAAAgAAAAAAAAABAAAAGABkAHIAYQB3AHQAbwBvAGwAcwBiAGEA"
			"cgMAAAAA/////wAAAAAAAAAAAAAAAgAAAAIAAAAYAGYAaQBsAGUAdABvAG8AbABzAG"
			"IAYQByAQAAAAD/////AAAAAAAAAAAAAAAYAGUAZABpAHQAdABvAG8AbABzAGIAYQBy"
			"AQAAAJQAAAbsAAAAAAAAAAA="),
		&predefs.mediBangEsqueLayout);
	predefs.compatibilityMappings.insert(
		QByteArray::fromBase64(
			"AAAA/wAAAAD9AAAAAgAAAAAAAAIFAAAD5/wCAAAAAvwAAAA9AAABUQAAAPYA/////A"
			"EAAAAC+wAAABoAbgBhAHYAaQBnAGEAdABvAHIAZABvAGMAawEAAAAAAAAA+gAAAGwA"
			"/////AAAAQAAAAEFAAAA3gD////6AAAAAAIAAAAD+wAAACAAYwBvAGwAbwByAHMAcA"
			"BpAG4AbgBlAHIAZABvAGMAawEAAAA9AAABOwAAABwA////+wAAACAAYwBvAGwAbwBy"
			"AHAAYQBsAGUAdAB0AGUAZABvAGMAawEAAAAA/////wAAAH0A////+wAAAB4AYwBvAG"
			"wAbwByAHMAbABpAGQAZQByAGQAbwBjAGsBAAAAAP////8AAADZAP////wAAAGUAAAC"
			"kAAAAYsA/////AEAAAAC+wAAABIATABhAHkAZQByAEwAaQBzAHQBAAAAAAAAAPoAAA"
			"CXAP////wAAAEAAAABBQAAAMYA/////AIAAAAC+wAAABgAQgByAHUAcwBoAFAAYQBs"
			"AGUAdAB0AGUBAAABlAAAARQAAABjAP////sAAAAYAFQAbwBvAGwAUwBlAHQAdABpAG"
			"4AZwBzAQAAAq4AAAF2AAABIgD///8AAAACAAAFEAAAALj8AQAAAAH8AAABagAABRAA"
			"AAAAAP////r/////AQAAAAL7AAAAFABvAG4AaQBvAG4AcwBrAGkAbgBzAAAAAAD///"
			"//AAABZQD////7AAAAEABUAGkAbQBlAGwAaQBuAGUAAAAAAP////8AAAE8AP///wAA"
			"BXUAAAPnAAAAAQAAAAIAAAABAAAAAvwAAAABAAAAAgAAAAMAAAAYAGYAaQBsAGUAdA"
			"BvAG8AbABzAGIAYQByAQAAAAD/////AAAAAAAAAAAAAAAYAGUAZABpAHQAdABvAG8A"
			"bABzAGIAYQByAQAAAJT/////AAAAAAAAAAAAAAAYAGQAcgBhAHcAdABvAG8AbABzAG"
			"IAYQByAQAAAUn/////AAAAAAAAAAA="),
		&predefs.saiEsqueLayout);
	predefs.compatibilityMappings.insert(
		QByteArray::fromBase64(
			"AAAA/wAAAAD9AAAAAwAAAAAAAAEvAAAD5/wCAAAAAfsAAAAYAEIAcgB1AHMAaABQAG"
			"EAbABlAHQAdABlAQAAAD0AAAPnAAAAYwD///8AAAABAAABPgAAA+f8AgAAAAT7AAAA"
			"GABUAG8AbwBsAFMAZQB0AHQAaQBuAGcAcwEAAAA9AAABQgAAATwA/////AAAAYUAAA"
			"ExAAAA9gEAABz6AAAAAgIAAAAD+wAAACAAYwBvAGwAbwByAHAAYQBsAGUAdAB0AGUA"
			"ZABvAGMAawEAAAAA/////wAAAH0A////+wAAAB4AYwBvAGwAbwByAHMAbABpAGQAZQ"
			"ByAGQAbwBjAGsBAAAAAP////8AAADZAP////sAAAAgAGMAbwBsAG8AcgBzAHAAaQBu"
			"AG4AZQByAGQAbwBjAGsBAAAAAP////8AAAAcAP////sAAAASAEwAYQB5AGUAcgBMAG"
			"kAcwB0AQAAArwAAAFoAAAAnAD////7AAAAGgBuAGEAdgBpAGcAYQB0AG8AcgBkAG8A"
			"YwBrAAAAAAD/////AAAAGwD///8AAAACAAAFEAAAALj8AQAAAAH8AAABagAABRAAAA"
			"AAAP////r/////AQAAAAL7AAAAFABvAG4AaQBvAG4AcwBrAGkAbgBzAAAAAAD/////"
			"AAABZQD////7AAAAEABUAGkAbQBlAGwAaQBuAGUAAAAAAP////8AAAE8AP///wAABQ"
			"cAAAPnAAAAAQAAAAIAAAABAAAAAvwAAAABAAAAAgAAAAMAAAAYAGYAaQBsAGUAdABv"
			"AG8AbABzAGIAYQByAQAAAAD/////AAAAAAAAAAAAAAAYAGUAZABpAHQAdABvAG8AbA"
			"BzAGIAYQByAQAAAJT/////AAAAAAAAAAAAAAAYAGQAcgBhAHcAdABvAG8AbABzAGIA"
			"YQByAQAAAUn/////AAAAAAAAAAA="),
		&predefs.spiderLayout);
	return predefs;
}

}

Q_DECLARE_METATYPE(dialogs::LayoutsDialog::Layout *)
