/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2017 Calle Laakkonen

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

#include "annotationsettings.h"
#include "tools/toolcontroller.h"
#include "tools/toolproperties.h"
#include "tools/annotation.h"
#include "canvas/canvasmodel.h"
#include "canvas/userlist.h"
#include "net/client.h"
#include "net/commands.h"

#include "../shared/net/annotation.h"
#include "../shared/net/undo.h"

#include "widgets/groupedtoolbutton.h"
#include "widgets/colorbutton.h"
using widgets::ColorButton;
using widgets::GroupedToolButton;

#include "ui_textsettings.h"

#include <QTimer>
#include <QTextBlock>

namespace tools {

AnnotationSettings::AnnotationSettings(QString name, QString title, ToolController *ctrl)
	: QObject(), ToolSettings(name, title, "draw-text", ctrl), _ui(nullptr), _noupdate(false)
{
}

AnnotationSettings::~AnnotationSettings()
{
	delete _ui;
}

QWidget *AnnotationSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	_ui = new Ui_TextSettings;
	_ui->setupUi(widget);

	_updatetimer = new QTimer(this);
	_updatetimer->setInterval(500);
	_updatetimer->setSingleShot(true);

	// Editor events
	connect(_ui->content, SIGNAL(textChanged()), this, SLOT(applyChanges()));
	connect(_ui->content, SIGNAL(cursorPositionChanged()), this, SLOT(updateStyleButtons()));

	connect(_ui->btnBackground, SIGNAL(colorChanged(const QColor&)),
			this, SLOT(setEditorBackgroundColor(const QColor &)));
	connect(_ui->btnBackground, SIGNAL(colorChanged(const QColor&)),
			this, SLOT(applyChanges()));

	connect(_ui->btnRemove, SIGNAL(clicked()), this, SLOT(removeAnnotation()));
	connect(_ui->btnBake, SIGNAL(clicked()), this, SLOT(bake()));

	connect(_ui->font, SIGNAL(currentIndexChanged(int)), this, SLOT(updateFontIfUniform()));
	connect(_ui->size, SIGNAL(valueChanged(double)), this, SLOT(updateFontIfUniform()));
	connect(_ui->btnTextColor, SIGNAL(colorChanged(QColor)), this, SLOT(updateFontIfUniform()));

	// Intra-editor connections that couldn't be made in the UI designer
	connect(_ui->left, SIGNAL(clicked()), this, SLOT(changeAlignment()));
	connect(_ui->center, SIGNAL(clicked()), this, SLOT(changeAlignment()));
	connect(_ui->justify, SIGNAL(clicked()), this, SLOT(changeAlignment()));
	connect(_ui->right, SIGNAL(clicked()), this, SLOT(changeAlignment()));
	connect(_ui->bold, SIGNAL(toggled(bool)), this, SLOT(toggleBold(bool)));
	connect(_ui->strikethrough, SIGNAL(toggled(bool)), this, SLOT(toggleStrikethrough(bool)));

	connect(_updatetimer, SIGNAL(timeout()), this, SLOT(saveChanges()));

	// Select a nice default font
	QStringList defaultFonts;
	defaultFonts << "Arial" << "Helvetica" << "Sans Serif";
	for(const QString &df : defaultFonts) {
		int i = _ui->font->findText(df, Qt::MatchFixedString);
		if(i>=0) {
			_ui->font->setCurrentIndex(i);
			break;
		}
	}

	setUiEnabled(false);

	return widget;
}

void AnnotationSettings::setUiEnabled(bool enabled)
{
	_ui->content->setEnabled(enabled);
	_ui->btnBake->setEnabled(enabled);
	_ui->btnRemove->setEnabled(enabled);
}

void AnnotationSettings::setEditorBackgroundColor(const QColor &color)
{
	// Blend transparent colors with white
	const QColor c = QColor::fromRgbF(
		color.redF() * color.alphaF() + (1-color.alphaF()),
		color.greenF() * color.alphaF() + (1-color.alphaF()),
		color.blueF() * color.alphaF() + (1-color.alphaF())
	);

	// We need to use the stylesheet because native styles ignore the palette.
	_ui->content->setStyleSheet("background-color: " + c.name());
}

void AnnotationSettings::updateStyleButtons()
{
	QTextBlockFormat bf = _ui->content->textCursor().blockFormat();
	switch(bf.alignment()) {
	case Qt::AlignLeft: _ui->left->setChecked(true); break;
	case Qt::AlignCenter: _ui->center->setChecked(true); break;
	case Qt::AlignJustify: _ui->justify->setChecked(true); break;
	case Qt::AlignRight: _ui->right->setChecked(true); break;
	default: break;
	}
	QTextCharFormat cf = _ui->content->textCursor().charFormat();
	_ui->btnTextColor->setColor(cf.foreground().color());

	_ui->size->blockSignals(true);
	if(cf.fontPointSize() < 1)
		_ui->size->setValue(12);
	else
		_ui->size->setValue(cf.fontPointSize());
	_ui->size->blockSignals(false);

	_ui->font->blockSignals(true);
	_ui->font->setCurrentFont(cf.font());
	_ui->font->blockSignals(false);

	_ui->italic->setChecked(cf.fontItalic());
	_ui->bold->setChecked(cf.fontWeight() > QFont::Normal);
	_ui->underline->setChecked(cf.fontUnderline());
	_ui->strikethrough->setChecked(cf.font().strikeOut());
}

void AnnotationSettings::toggleBold(bool bold)
{
	_ui->content->setFontWeight(bold ? QFont::Bold : QFont::Normal);
}

void AnnotationSettings::toggleStrikethrough(bool strike)
{
	QFont font = _ui->content->currentFont();
	font.setStrikeOut(strike);
	_ui->content->setCurrentFont(font);
}

void AnnotationSettings::changeAlignment()
{
	Qt::Alignment a = Qt::AlignLeft;
	if(_ui->left->isChecked())
		a = Qt::AlignLeft;
	else if(_ui->center->isChecked())
		a = Qt::AlignCenter;
	else if(_ui->justify->isChecked())
		a = Qt::AlignJustify;
	else if(_ui->right->isChecked())
		a = Qt::AlignRight;

	_ui->content->setAlignment(a);
}

void AnnotationSettings::updateFontIfUniform()
{
	bool uniformFontFamily = true;
	bool uniformSize = true;
	bool uniformColor = true;

	QTextDocument *doc = _ui->content->document();

	QTextBlock b = doc->firstBlock();
	QTextCharFormat fmt1;
	bool first=true;

	// Check all character formats in all blocks. If they are the same,
	// we can reset the font for the wole document.
	while(b.isValid()) {
		for(const QTextLayout::FormatRange &fr : b.textFormats()) {

			if(first) {
				fmt1 = fr.format;
				first = false;

			} else {
				uniformFontFamily &= fr.format.fontFamily() == fmt1.fontFamily();
				uniformSize &= fr.format.fontPointSize() == fmt1.fontPointSize();
				uniformColor &= fr.format.foreground() == fmt1.foreground();
			}
		}
		b = b.next();
	}

	resetContentFont(uniformFontFamily, uniformSize, uniformColor);
}

void AnnotationSettings::resetContentFont(bool resetFamily, bool resetSize, bool resetColor)
{
	if(!(resetFamily|resetSize|resetColor))
		return;

	QTextCursor cursor(_ui->content->document());
	cursor.select(QTextCursor::Document);
	QTextCharFormat fmt;

	if(resetFamily)
		fmt.setFontFamily(_ui->font->currentText());

	if(resetSize)
		fmt.setFontPointSize(_ui->size->value());

	if(resetColor)
		fmt.setForeground(_ui->btnTextColor->color());

	cursor.mergeCharFormat(fmt);
}

void AnnotationSettings::setSelectionId(int id)
{
	_noupdate = true;
	setUiEnabled(id>0);

	m_selectionId = id;
	if(id) {
		const paintcore::Annotation *a = controller()->model()->layerStack()->annotations()->getById(id);
		Q_ASSERT(a);
		if(!a)
			return;

		_ui->content->setHtml(a->text);
		_ui->btnBackground->setColor(a->background);
		setEditorBackgroundColor(a->background);
		if(a->text.isEmpty())
			resetContentFont(true, true, true);
		_ui->ownerLabel->setText(QString("(%1)").arg(
			controller()->model()->userlist()->getUsername((a->id & 0xff00) >> 8)));
	}
	_noupdate = false;
}

void AnnotationSettings::setFocusAt(int cursorPos)
{
	_ui->content->setFocus();
	if(cursorPos>=0) {
		QTextCursor c = _ui->content->textCursor();
		c.setPosition(cursorPos);
		_ui->content->setTextCursor(c);
	}
}

void AnnotationSettings::setFocus()
{
	setFocusAt(-1);
}

void AnnotationSettings::applyChanges()
{
	if(_noupdate || !selected())
		return;
	_updatetimer->start();
}

void AnnotationSettings::saveChanges()
{
	if(!selected())
		return;

	if(selected()) {
		controller()->client()->sendMessage(protocol::MessagePtr(new protocol::AnnotationEdit(
			controller()->client()->myId(),
			selected(),
			_ui->btnBackground->color().rgba(),
			_ui->content->toHtml()
		)));
	}
}

void AnnotationSettings::removeAnnotation()
{
	Q_ASSERT(selected());
	const uint8_t contextId = controller()->client()->myId();
	QList<protocol::MessagePtr> msgs;
	msgs << protocol::MessagePtr(new protocol::UndoPoint(contextId));
	msgs << protocol::MessagePtr(new protocol::AnnotationDelete(contextId, selected()));
	controller()->client()->sendMessages(msgs);
}

void AnnotationSettings::bake()
{
	Q_ASSERT(selected());

	const paintcore::Annotation *a = controller()->model()->layerStack()->annotations()->getById(selected());
	Q_ASSERT(a);

	const QImage img = a->toImage();

	const uint8_t contextId = controller()->client()->myId();
	const int layer = controller()->activeLayer();

	QList<protocol::MessagePtr> msgs;
	msgs << protocol::MessagePtr(new protocol::UndoPoint(contextId));
	msgs << net::command::putQImage(contextId, layer, a->rect.x(), a->rect.y(), img, paintcore::BlendMode::MODE_NORMAL);
	msgs << protocol::MessagePtr(new protocol::AnnotationDelete(contextId, selected()));
	controller()->client()->sendMessages(msgs);
}

}

