/*
 * Drawpile - a collaborative drawing program.
 *
 * Copyright (C) 2023 askmeaboutloom
 *
 * Drawpile is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Drawpile is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "layoutsdialog.h"
#include "ui_layoutsdialog.h"
#include <QByteArray>
#include <QInputDialog>
#include <QSettings>
#include <QTimer>
#include <QVector>
#include <algorithm>

Q_DECLARE_METATYPE(dialogs::LayoutsDialog::Layout *);

namespace dialogs {

struct LayoutsDialog::Layout {
	QString title;
	QByteArray state;
	QString originalTitle;
	bool wasTransient;
	bool transient;
	bool deleted;

	Layout(QString title, QByteArray state)
		: title{title}
		, state{state}
		, originalTitle{}
		, wasTransient{false}
		, transient{false}
		, deleted{false}
	{
	}

	explicit Layout(QByteArray state)
		: title{}
		, state{state}
		, originalTitle{}
		, wasTransient{true}
		, transient{true}
		, deleted{false}
	{
	}
};

struct LayoutsDialog::Private {
	Ui_LayoutsDialog ui;
	QVector<Layout> layouts;
	QByteArray originalState;
	bool updateInProgress;

	// Defined at the bottom of the file because it's annoyingly large.
	void createDefaultLayouts();

	Private(const QByteArray &currentState)
		: ui{}
		, layouts{}
		, originalState{currentState}
		, updateInProgress{false}
	{
		layouts.append(Layout{currentState});
		QSettings settings;
		int count = settings.beginReadArray("layouts");
		if(count == 0) {
			createDefaultLayouts();
		} else {
			for(int i = 0; i < count; ++i) {
				settings.setArrayIndex(i);
				layouts.append(Layout{
					settings.value("title").toString(),
					settings.value("state").toByteArray()});
			}
		}
	}

	void saveLayouts()
	{
		QSettings settings;
		settings.beginWriteArray("layouts");
		int i = 0;
		for(const Layout &layout : layouts) {
			if(!layout.transient && !layout.deleted) {
				settings.setArrayIndex(i++);
				settings.setValue("title", layout.title);
				settings.setValue("state", layout.state);
			}
		}
		settings.endArray();
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
		d->ui.saveButton, &QPushButton::pressed, this, &LayoutsDialog::save);
	connect(
		d->ui.renameButton, &QPushButton::pressed, this,
		&LayoutsDialog::rename);
	connect(
		d->ui.deleteButton, &QPushButton::pressed, this,
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
	QString title;
	if(promptTitle(layout, title)) {
		layout->title = title;
		layout->transient = false;
		d->updateList(layout);
		updateButtons();
	}
}

void LayoutsDialog::rename()
{
	Layout *layout = d->selectedLayout();
	QString title;
	if(promptTitle(layout, title)) {
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
	}
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

bool LayoutsDialog::promptTitle(Layout *layout, QString &outTitle)
{
	if(layout) {
		bool ok;
		QString title = QInputDialog::getText(
							this, d->ui.saveButton->text(), tr("Layout Name:"),
							QLineEdit::Normal, layout->title, &ok)
							.trimmed();
		if(ok && !title.isEmpty() && title != layout->title) {
			outTitle = title;
			return true;
		}
	}
	return false;
}


void LayoutsDialog::Private::createDefaultLayouts()
{
	layouts.append(LayoutsDialog::Layout{
		tr("Default"),
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
			"YQByAQAAAUn/////AAAAAAAAAAA=")});
	layouts.append(LayoutsDialog::Layout{
		tr("Default Animation"),
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
			"YQByAQAAAUn/////AAAAAAAAAAA=")});
	layouts.append(LayoutsDialog::Layout{
		tr("FireAlpaca-esque"),
		QByteArray::fromBase64(
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
			"AA")});
	layouts.append(LayoutsDialog::Layout{
		tr("Horizontal"),
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
			"AAABSf////8AAAAAAAAAAA==")});
	layouts.append(LayoutsDialog::Layout{
		tr("Hyena"),
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
			"AG8AbABzAGIAYQByAQAAA8MAAAO9AAAAAAAAAAA=")});
	layouts.append(LayoutsDialog::Layout{
		tr("Krita-esque"),
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
			"AQAAAJQAAAbsAAAAAAAAAAA=")});
	layouts.append(LayoutsDialog::Layout{
		tr("MediBang-esque"),
		QByteArray::fromBase64(
			"AAAA/wAAAAD9AAAAAwAAAAAAAAExAAAD5/wCAAAAAvwAAAA9AAABMwAAAPYBAAAc+g"
			"AAAAACAAAAA/sAAAAgAGMAbwBsAG8AcgBzAHAAaQBuAG4AZQByAGQAbwBjAGsBAAAA"
			"PQAAANkAAAAcAP////sAAAAeAGMAbwBsAG8AcgBzAGwAaQBkAGUAcgBkAG8AYwBrAQ"
			"AAAAD/////AAAA2QD////7AAAAIABjAG8AbABvAHIAcABhAGwAZQB0AHQAZQBkAG8A"
			"YwBrAQAAAAD/////AAAAfQD////7AAAAGABUAG8AbwBsAFMAZQB0AHQAaQBuAGcAcw"
			"EAAAF2AAACrgAAATwA////AAAAAQAAASQAAAPn/AIAAAAD+wAAABoAbgBhAHYAaQBn"
			"AGEAdABvAHIAZABvAGMAawEAAAA9AAAApAAAABsA////+wAAABIATABhAHkAZQByAE"
			"wAaQBzAHQBAAAA5wAAASgAAACcAP////sAAAAYAEIAcgB1AHMAaABQAGEAbABlAHQA"
			"dABlAQAAAhUAAAIPAAAAYwD///8AAAACAAAFEAAAALj8AQAAAAH8AAABagAABRAAAA"
			"AAAP////r/////AQAAAAL7AAAAFABvAG4AaQBvAG4AcwBrAGkAbgBzAAAAAAD/////"
			"AAABZQD////7AAAAEABUAGkAbQBlAGwAaQBuAGUAAAAAAP////8AAAE8AP///wAABP"
			"cAAAPnAAAAAQAAAAIAAAABAAAAAvwAAAACAAAAAAAAAAEAAAAYAGQAcgBhAHcAdABv"
			"AG8AbABzAGIAYQByAwAAAAD/////AAAAAAAAAAAAAAACAAAAAgAAABgAZgBpAGwAZQ"
			"B0AG8AbwBsAHMAYgBhAHIBAAAAAP////8AAAAAAAAAAAAAABgAZQBkAGkAdAB0AG8A"
			"bwBsAHMAYgBhAHIBAAAAlAAABuwAAAAAAAAAAA==")});
	layouts.append(LayoutsDialog::Layout{
		tr("Mink"),
		QByteArray::fromBase64(
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
			"AAAAAAA=")});
	layouts.append(LayoutsDialog::Layout{
		tr("Paint.NET-esque"),
		QByteArray::fromBase64(
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
			"YQByAQAAAUn/////AAAAAAAAAAA=")});
	layouts.append(LayoutsDialog::Layout{
		tr("SAI-esque"),
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
			"IAYQByAQAAAUn/////AAAAAAAAAAA=")});
	layouts.append(LayoutsDialog::Layout{
		tr("Spider"),
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
			"YQByAQAAAUn/////AAAAAAAAAAA=")});
}

}
