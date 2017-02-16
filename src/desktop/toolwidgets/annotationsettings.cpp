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
#include "canvas/aclfilter.h"
#include "net/client.h"
#include "net/commands.h"
#include "utils/icon.h"

#include "../shared/net/annotation.h"
#include "../shared/net/undo.h"

#include "widgets/groupedtoolbutton.h"
#include "widgets/colorbutton.h"
using widgets::ColorButton;
using widgets::GroupedToolButton;

#include "ui_textsettings.h"

#include <QTimer>
#include <QTextBlock>
#include <QMenu>

namespace tools {

static const char *HALIGN_PROP = "HALIGN";
static const char *VALIGN_PROP = "VALIGN";

AnnotationSettings::AnnotationSettings(QString name, QString title, ToolController *ctrl)
	: QObject(), ToolSettings(name, title, "draw-text", ctrl), _ui(nullptr), m_selectionId(0), m_noupdate(false)
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

	m_updatetimer = new QTimer(this);
	m_updatetimer->setInterval(500);
	m_updatetimer->setSingleShot(true);

	// Horizontal alignment options
	QMenu *halignMenu = new QMenu;
	halignMenu->addAction(icon::fromTheme("format-justify-left"), tr("Left"))->setProperty(HALIGN_PROP, Qt::AlignLeft);
	halignMenu->addAction(icon::fromTheme("format-justify-center"), tr("Center"))->setProperty(HALIGN_PROP, Qt::AlignCenter);
	halignMenu->addAction(icon::fromTheme("format-justify-fill"), tr("Justify"))->setProperty(HALIGN_PROP, Qt::AlignJustify);
	halignMenu->addAction(icon::fromTheme("format-justify-right"), tr("Right"))->setProperty(HALIGN_PROP, Qt::AlignRight);
	_ui->halign->setIcon(halignMenu->actions().first()->icon());
	connect(halignMenu, &QMenu::triggered, this, &AnnotationSettings::changeAlignment);
	_ui->halign->setMenu(halignMenu);

	// Vertical alignment options
	QMenu *valignMenu = new QMenu;
	valignMenu->addAction(icon::fromTheme("format-align-vertical-top"), tr("Top"))->setProperty(VALIGN_PROP, 0);
	valignMenu->addAction(icon::fromTheme("format-align-vertical-center"), tr("Center"))->setProperty(VALIGN_PROP, protocol::AnnotationEdit::FLAG_VALIGN_CENTER);
	valignMenu->addAction(icon::fromTheme("format-align-vertical-bottom"), tr("Bottom"))->setProperty(VALIGN_PROP, protocol::AnnotationEdit::FLAG_VALIGN_BOTTOM);
	_ui->valign->setIcon(valignMenu->actions().first()->icon());
	connect(valignMenu, &QMenu::triggered, this, &AnnotationSettings::changeAlignment);
	_ui->valign->setMenu(valignMenu);

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
	connect(_ui->protect, &QCheckBox::clicked, this, &AnnotationSettings::saveChanges);

	// Intra-editor connections that couldn't be made in the UI designer
	connect(_ui->bold, SIGNAL(toggled(bool)), this, SLOT(toggleBold(bool)));
	connect(_ui->strikethrough, SIGNAL(toggled(bool)), this, SLOT(toggleStrikethrough(bool)));

	connect(m_updatetimer, &QTimer::timeout, this, &AnnotationSettings::saveChanges);

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
	QWidget *w[] = {
		_ui->content,
		_ui->btnBake,
		_ui->btnRemove,
		_ui->btnBackground,
		_ui->btnTextColor,
		_ui->protect,
		_ui->halign,
		_ui->valign,
		_ui->bold,
		_ui->italic,
		_ui->underline,
		_ui->strikethrough,
		_ui->font,
		_ui->size
	};
	for(unsigned int i=0;i<sizeof(w)/sizeof(*w);++i)
		w[i]->setEnabled(enabled);
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
	case Qt::AlignLeft: _ui->halign->setIcon(icon::fromTheme("format-justify-left")); break;
	case Qt::AlignCenter: _ui->halign->setIcon(icon::fromTheme("format-justify-center")); break;
	case Qt::AlignJustify: _ui->halign->setIcon(icon::fromTheme("format-justify-fill")); break;
	case Qt::AlignRight: _ui->halign->setIcon(icon::fromTheme("format-justify-right")); break;
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

void AnnotationSettings::changeAlignment(const QAction *action)
{
	if(action->property(HALIGN_PROP).isNull()) {
		_ui->valign->setIcon(action->icon());
		_ui->valign->setProperty(VALIGN_PROP, action->property(VALIGN_PROP));

		applyChanges();

	} else {
		Qt::Alignment a = Qt::Alignment(action->property(HALIGN_PROP).toInt());

		_ui->content->setAlignment(a);
		_ui->halign->setIcon(action->icon());
	}
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
	m_noupdate = true;
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

		switch(a->valign) {
		case 0: _ui->valign->setIcon(icon::fromTheme("format-align-vertical-top")); break;
		case protocol::AnnotationEdit::FLAG_VALIGN_BOTTOM: _ui->valign->setIcon(icon::fromTheme("format-align-vertical-bottom")); break;
		case protocol::AnnotationEdit::FLAG_VALIGN_CENTER: _ui->valign->setIcon(icon::fromTheme("format-align-vertical-center")); break;
		}
		_ui->valign->setProperty(VALIGN_PROP, a->valign);

		_ui->ownerLabel->setText(QString("(%1)").arg(
			controller()->model()->userlist()->getUsername(a->userId())));
		_ui->protect->setChecked(a->protect);

		const bool opOrOwner = controller()->model()->aclFilter()->isLocalUserOperator() || a->userId() == controller()->client()->myId();
		if(a->protect && !opOrOwner)
			setUiEnabled(false);
		else if(!opOrOwner)
			_ui->protect->setEnabled(false);
	}
	m_noupdate = false;
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
	if(m_noupdate || !selected())
		return;
	m_updatetimer->start();
}

void AnnotationSettings::saveChanges()
{
	if(!selected())
		return;

	m_updatetimer->stop();

	if(selected()) {
		controller()->client()->sendMessage(protocol::MessagePtr(new protocol::AnnotationEdit(
			controller()->client()->myId(),
			selected(),
			_ui->btnBackground->color().rgba(),
			(_ui->protect->isChecked() ? protocol::AnnotationEdit::FLAG_PROTECT : 0)
			| uint8_t(_ui->valign->property(VALIGN_PROP).toInt()),
			0,
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
	if(!a) {
		qWarning("Selected annotation %d not found!", selected());
		return;
	}
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

