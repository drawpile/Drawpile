/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2009 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#include <QItemSelection>
#include <QListView>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>

#include "layerlistwidget.h"
#include "layerlistdelegate.h"
#include "board.h"
#include "ui_layerbox.h"
#include "core/layerstack.h"
#include "core/layer.h"

namespace widgets {

LayerList::LayerList(QWidget *parent)
	: QDockWidget(tr("Layers"), parent), locksel_(false)
{
	ui_ = new Ui_LayerBox;
	QWidget *w = new QWidget(this);
	setWidget(w);
	ui_->setupUi(w);

	//model_ = new LayerListModel(this);
	//ui_->layers->setModel(model_);
	LayerListDelegate *del = new LayerListDelegate(this);
	ui_->layers->setItemDelegate(del);
	connect(del, SIGNAL(newLayer()), this, SLOT(newLayer()));
	connect(del, SIGNAL(deleteLayer(const dpcore::Layer*)), this,
			SLOT(deleteLayer(const dpcore::Layer*)));
	connect(del, SIGNAL(layerToggleHidden(int)), this,
			SIGNAL(layerToggleHidden(int)));
	connect(ui_->opacity, SIGNAL(valueChanged(int)), this,
			SLOT(opacityChanged(int)));
}

LayerList::~LayerList()
{
}

void LayerList::setBoard(drawingboard::Board *board)
{
	ui_->layers->setModel(board->layers());
	connect(ui_->layers->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)), this, SLOT(selected(const QItemSelection&, const QItemSelection&)));
}

/**
 * This is called to synchronize the UI with changes that have happened
 * due to things like layer deletion and network events.
 * @param id layer ID
 */
void LayerList::selectLayer(int id)
{
	dpcore::LayerStack *layers = static_cast<dpcore::LayerStack*>(ui_->layers->model());
	int row = layers->layers() - layers->id2index(id);
	QModelIndexList sel = ui_->layers->selectionModel()->selectedIndexes();
	if(sel.isEmpty() || sel.first().row() != row)
		ui_->layers->selectionModel()->select(layers->index(row,0),
				QItemSelectionModel::Select);
}

/**
 * A layer was selected
 */
void LayerList::selected(const QItemSelection& selection, const QItemSelection& prev)
{
	if(!locksel_) {
		locksel_ = true;
		if(selection.indexes().isEmpty()) {
			dpcore::LayerStack *layers = static_cast<dpcore::LayerStack*>(ui_->layers->model());
			// A layer must always be selected
			if(prev.indexes().isEmpty())
				ui_->layers->selectionModel()->select(layers->index(layers->layers(),0),
					QItemSelectionModel::Select);
			else
				ui_->layers->selectionModel()->select(prev.indexes().first(),
						QItemSelectionModel::Select);
		} else {
			const dpcore::Layer *layer = selection.indexes().first().data().value<dpcore::Layer*>();
			emit selected(layer->id());

			// Update the UI controls
			ui_->opacity->setValue(layer->opacity());
		}
		locksel_ = false;
	}
}

void LayerList::opacityChanged(int opacity)
{
	if(!locksel_) {
		const dpcore::Layer *layer = ui_->layers->selectionModel()->selection().indexes().first().data().value<dpcore::Layer*>();
		emit opacityChange(layer->id(), opacity);
	}
}

void LayerList::newLayer()
{
	bool ok;
	QString name = QInputDialog::getText(this, tr("Add a new layer"),
			tr("Layer name:"), QLineEdit::Normal, "", &ok);
	if(ok) {
		if(name.isEmpty())
			name = tr("Unnamed layer");
		emit newLayer(name);
	}
}

void LayerList::deleteLayer(const dpcore::Layer *layer)
{
	QMessageBox box(QMessageBox::Question, tr("Delete layer"),
			tr("Really delete \"%1\"?").arg(layer->name()),
			QMessageBox::NoButton, this);
	box.setInformativeText(tr("Press merge down to merge the layer with the first visible layer below instead of deleting."));
	box.addButton(tr("Delete"), QMessageBox::DestructiveRole);
	QPushButton *merge = box.addButton(tr("Merge down"), QMessageBox::DestructiveRole);
	QPushButton *cancel = box.addButton(tr("Cancel"), QMessageBox::RejectRole);
	box.setDefaultButton(cancel);
	box.exec();
	QAbstractButton *choice = box.clickedButton();
	if(choice != cancel)
		emit deleteLayer(layer->id(), choice==merge);
}

}

