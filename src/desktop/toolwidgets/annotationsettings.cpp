/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2022 Calle Laakkonen

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

#include "desktop/toolwidgets/annotationsettings.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/userlist.h"
#include "desktop/scene/canvasscene.h"
#include "desktop/scene/annotationitem.h"
#include "libclient/net/client.h"
#include "libclient/net/envelopebuilder.h"
#include "libclient/utils/icon.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/utils/qtguicompat.h"


#include "ui_textsettings.h"

#include <QActionGroup>
#include <QTimer>
#include <QTextBlock>
#include <QMenu>
#include <QLabel>

namespace tools {

static const char *HALIGN_PROP = "HALIGN";
static const char *VALIGN_PROP = "VALIGN";

AnnotationSettings::AnnotationSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent), m_ui(nullptr), m_headerWidget(nullptr),
	  m_selectionId(0), m_noupdate(false)
{
}

AnnotationSettings::~AnnotationSettings()
{
	delete m_ui;
}

QWidget *AnnotationSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	m_ui = new Ui_TextSettings;
	m_ui->setupUi(widget);

	// Set up the header widget
	m_headerWidget = new QWidget(parent);
	auto headerLayout = new QHBoxLayout;
	headerLayout->setSpacing(0);
	headerLayout->setContentsMargins(0, 0, 0, 0);
	m_headerWidget->setLayout(headerLayout);

	m_protectedAction = new QAction(icon::fromTheme("object-locked"), tr("Protect"), this);
	m_protectedAction->setCheckable(true);
	auto *protectedButton = new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft, m_headerWidget);
	protectedButton->setDefaultAction(m_protectedAction);
	headerLayout->addWidget(protectedButton);

	auto *mergeButton = new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter, m_headerWidget);
	QAction *mergeAction = new QAction(icon::fromTheme("arrow-down-double"), tr("Merge"), this);
	mergeButton->setDefaultAction(mergeAction);
	headerLayout->addWidget(mergeButton);

	auto *deleteButton = new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight, m_headerWidget);
	QAction *deleteAction = new QAction(icon::fromTheme("list-remove"), tr("Delete"), this);
	deleteButton->setDefaultAction(deleteAction);
	headerLayout->addWidget(deleteButton);

	headerLayout->addStretch(0);

	m_creatorLabel = new QLabel(m_headerWidget);
	m_creatorLabel->setAlignment(Qt::AlignRight);
	headerLayout->addWidget(m_creatorLabel);

	m_editActions = new QActionGroup(this);
	m_editActions->addAction(mergeAction);
	m_editActions->addAction(deleteAction);
	m_editActions->addAction(m_protectedAction);
	m_editActions->setExclusive(false);

	// Set up the dock
	m_updatetimer = new QTimer(this);
	m_updatetimer->setInterval(500);
	m_updatetimer->setSingleShot(true);

	// Horizontal alignment options
	QMenu *halignMenu = new QMenu;
	halignMenu->addAction(icon::fromTheme("format-justify-left"), tr("Left"))->setProperty(HALIGN_PROP, Qt::AlignLeft);
	halignMenu->addAction(icon::fromTheme("format-justify-center"), tr("Center"))->setProperty(HALIGN_PROP, Qt::AlignCenter);
	halignMenu->addAction(icon::fromTheme("format-justify-fill"), tr("Justify"))->setProperty(HALIGN_PROP, Qt::AlignJustify);
	halignMenu->addAction(icon::fromTheme("format-justify-right"), tr("Right"))->setProperty(HALIGN_PROP, Qt::AlignRight);
	m_ui->halign->setIcon(halignMenu->actions().constFirst()->icon());
	connect(halignMenu, &QMenu::triggered, this, &AnnotationSettings::changeAlignment);
	m_ui->halign->setMenu(halignMenu);

	// Vertical alignment options
	QMenu *valignMenu = new QMenu;
	valignMenu->addAction(icon::fromTheme("format-align-vertical-top"), tr("Top"))->setProperty(VALIGN_PROP, 0);
	valignMenu->addAction(icon::fromTheme("format-align-vertical-center"), tr("Center"))->setProperty(VALIGN_PROP, rustpile::AnnotationEditMessage_FLAGS_VALIGN_CENTER);
	valignMenu->addAction(icon::fromTheme("format-align-vertical-bottom"), tr("Bottom"))->setProperty(VALIGN_PROP, rustpile::AnnotationEditMessage_FLAGS_VALIGN_BOTTOM);
	m_ui->valign->setIcon(valignMenu->actions().constFirst()->icon());
	connect(valignMenu, &QMenu::triggered, this, &AnnotationSettings::changeAlignment);
	m_ui->valign->setMenu(valignMenu);

	// Editor events
	connect(m_ui->content, &QTextEdit::textChanged, this, &AnnotationSettings::applyChanges);
	connect(m_ui->content, &QTextEdit::cursorPositionChanged, this, &AnnotationSettings::updateStyleButtons);

	connect(m_ui->btnBackground, &widgets::ColorButton::colorChanged, this, &AnnotationSettings::setEditorBackgroundColor);
	connect(m_ui->btnBackground, &widgets::ColorButton::colorChanged, this, &AnnotationSettings::applyChanges);

	connect(deleteAction, &QAction::triggered, this, &AnnotationSettings::removeAnnotation);
	connect(mergeAction, &QAction::triggered, this, &AnnotationSettings::bake);
	connect(m_protectedAction, &QAction::triggered, this, &AnnotationSettings::saveChanges);

	connect(m_ui->font, SIGNAL(currentIndexChanged(int)), this, SLOT(updateFontIfUniform()));
	connect(m_ui->size, SIGNAL(valueChanged(double)), this, SLOT(updateFontIfUniform()));
	connect(m_ui->btnTextColor, &widgets::ColorButton::colorChanged, this, &AnnotationSettings::updateFontIfUniform);

	// Intra-editor connections that couldn't be made in the UI designer
	connect(m_ui->bold, &QToolButton::toggled, this, &AnnotationSettings::toggleBold);
	connect(m_ui->strikethrough, &QToolButton::toggled, this, &AnnotationSettings::toggleStrikethrough);

	connect(m_updatetimer, &QTimer::timeout, this, &AnnotationSettings::saveChanges);

	// Select a nice default font
	QStringList defaultFonts;
	defaultFonts << "Arial" << "Helvetica" << "Sans Serif";
	for(const QString &df : defaultFonts) {
		int i = m_ui->font->findText(df, Qt::MatchFixedString);
		if(i>=0) {
			m_ui->font->setCurrentIndex(i);
			break;
		}
	}

	// Set initial content format
	QTextCharFormat fmt;
	compat::setFontFamily(fmt, m_ui->font->currentText());
	fmt.setFontPointSize(m_ui->size->value());
	fmt.setForeground(m_ui->btnTextColor->color());
	m_ui->content->setCurrentCharFormat(fmt);

	setUiEnabled(false);

	return widget;
}

QWidget *AnnotationSettings::getHeaderWidget()
{
	return m_headerWidget;
}

void AnnotationSettings::setUiEnabled(bool enabled)
{
	QWidget *w[] = {
		m_ui->content,
		m_ui->btnBackground,
		m_ui->btnTextColor,
		m_ui->halign,
		m_ui->valign,
		m_ui->bold,
		m_ui->italic,
		m_ui->underline,
		m_ui->strikethrough,
		m_ui->font,
		m_ui->size
	};
	for(unsigned int i=0;i<sizeof(w)/sizeof(*w);++i)
		w[i]->setEnabled(enabled);
	m_editActions->setEnabled(enabled);
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
	m_ui->content->setStyleSheet("background-color: " + c.name());
}

void AnnotationSettings::updateStyleButtons()
{
	QTextBlockFormat bf = m_ui->content->textCursor().blockFormat();
	switch(bf.alignment()) {
	case Qt::AlignLeft: m_ui->halign->setIcon(icon::fromTheme("format-justify-left")); break;
	case Qt::AlignCenter: m_ui->halign->setIcon(icon::fromTheme("format-justify-center")); break;
	case Qt::AlignJustify: m_ui->halign->setIcon(icon::fromTheme("format-justify-fill")); break;
	case Qt::AlignRight: m_ui->halign->setIcon(icon::fromTheme("format-justify-right")); break;
	default: break;
	}

	QTextCharFormat cf = m_ui->content->textCursor().charFormat();
	m_ui->btnTextColor->setColor(cf.foreground().color());

	m_ui->size->blockSignals(true);
	if(cf.fontPointSize() < 1)
		m_ui->size->setValue(12);
	else
		m_ui->size->setValue(cf.fontPointSize());
	m_ui->size->blockSignals(false);

	m_ui->font->blockSignals(true);
	m_ui->font->setCurrentFont(cf.font());
	m_ui->font->blockSignals(false);

	m_ui->italic->setChecked(cf.fontItalic());
	m_ui->bold->setChecked(cf.fontWeight() > QFont::Normal);
	m_ui->underline->setChecked(cf.fontUnderline());
	m_ui->strikethrough->setChecked(cf.font().strikeOut());
}

void AnnotationSettings::toggleBold(bool bold)
{
	m_ui->content->setFontWeight(bold ? QFont::Bold : QFont::Normal);
}

void AnnotationSettings::toggleStrikethrough(bool strike)
{
	QFont font = m_ui->content->currentFont();
	font.setStrikeOut(strike);
	m_ui->content->setCurrentFont(font);
}

void AnnotationSettings::changeAlignment(const QAction *action)
{
	if(action->property(HALIGN_PROP).isNull()) {
		m_ui->valign->setIcon(action->icon());
		m_ui->valign->setProperty(VALIGN_PROP, action->property(VALIGN_PROP));

		applyChanges();

	} else {
		Qt::Alignment a = Qt::Alignment(action->property(HALIGN_PROP).toInt());

		m_ui->content->setAlignment(a);
		m_ui->halign->setIcon(action->icon());
	}
}

void AnnotationSettings::updateFontIfUniform()
{
	bool uniformFontFamily = true;
	bool uniformSize = true;
	bool uniformColor = true;

	QTextDocument *doc = m_ui->content->document();

	QTextBlock b = doc->firstBlock();
	QTextCharFormat fmt1;
	bool first=true;

	// Check all character formats in all blocks. If they are the same,
	// we can reset the font for the wole document.
	while(b.isValid()) {
		const auto textFormats = b.textFormats();
		for(const QTextLayout::FormatRange &fr : textFormats) {

			if(first) {
				fmt1 = fr.format;
				first = false;

			} else {
				uniformFontFamily &= compat::fontFamily(fr.format) == compat::fontFamily(fmt1);
				uniformSize &= qFuzzyCompare(fr.format.fontPointSize(), fmt1.fontPointSize());
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

	QTextCursor cursor(m_ui->content->document());
	cursor.select(QTextCursor::Document);
	QTextCharFormat fmt;

	if(resetFamily)
		compat::setFontFamily(fmt, m_ui->font->currentText());

	if(resetSize)
		fmt.setFontPointSize(m_ui->size->value());

	if(resetColor)
		fmt.setForeground(m_ui->btnTextColor->color());

	cursor.mergeCharFormat(fmt);
}

void AnnotationSettings::setSelectionId(uint16_t id)
{

	m_noupdate = true;
	setUiEnabled(id>0);

	m_selectionId = id;
	if(id) {
		const auto *a = m_scene->getAnnotationItem(id);
		if(!a)
			return;

		const QString text = a->text();
		m_ui->content->setHtml(text);
		m_ui->btnBackground->setColor(a->color());
		setEditorBackgroundColor(a->color());
		if(text.isEmpty())
			resetContentFont(true, true, true);

		int align = 0;
		switch(a->valign()) {
		case 0:
			m_ui->valign->setIcon(icon::fromTheme("format-align-vertical-top"));
			break;
		case 1:
			m_ui->valign->setIcon(icon::fromTheme("format-align-vertical-center"));
			align = rustpile::AnnotationEditMessage_FLAGS_VALIGN_CENTER;
			break;
		case 2:
			m_ui->valign->setIcon(icon::fromTheme("format-align-vertical-bottom"));
			align = rustpile::AnnotationEditMessage_FLAGS_VALIGN_BOTTOM;
			break;
		}
		m_ui->valign->setProperty(VALIGN_PROP, align);

		m_creatorLabel->setText(controller()->model()->userlist()->getUsername(a->userId()));
		m_protectedAction->setChecked(a->protect());

		const bool opOrOwner = controller()->model()->aclState()->amOperator() || (a->id() >> 8) == controller()->client()->myId();
		m_protectedAction->setEnabled(opOrOwner);

		if(a->protect() && !opOrOwner)
			setUiEnabled(false);
	}
	m_noupdate = false;
}

void AnnotationSettings::setFocusAt(int cursorPos)
{
	m_ui->content->setFocus();
	if(cursorPos>=0) {
		QTextCursor c = m_ui->content->textCursor();
		c.setPosition(cursorPos);
		m_ui->content->setTextCursor(c);
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
		QString content;
		if(m_ui->content->document()->isEmpty())
			content = QString();
		else
			content = m_ui->content->toHtml();

		net::EnvelopeBuilder eb;
		rustpile::write_editannotation(
			eb,
			controller()->client()->myId(),
			selected(),
			m_ui->btnBackground->color().rgba(),
			(m_protectedAction->isChecked() ? rustpile::AnnotationEditMessage_FLAGS_PROTECT : 0)
			| uint8_t(m_ui->valign->property(VALIGN_PROP).toInt()),
			0,
			reinterpret_cast<const uint16_t*>(content.constData()),
			content.length()
		);
		controller()->client()->sendEnvelope(eb.toEnvelope());
	}
}

void AnnotationSettings::removeAnnotation()
{
	Q_ASSERT(selected());
	const uint8_t contextId = controller()->client()->myId();
	net::EnvelopeBuilder eb;
	rustpile::write_undopoint(eb, contextId);
	rustpile::write_deleteannotation(eb, contextId, selected());
	controller()->client()->sendEnvelope(eb.toEnvelope());
}

void AnnotationSettings::bake()
{
	Q_ASSERT(selected());
	const auto *a = m_scene->getAnnotationItem(selected());
	if(!a) {
		qWarning("Selected annotation %d not found!", selected());
		return;
	}

	const QImage img = a->toImage();

	const uint8_t contextId = controller()->client()->myId();
	const int layer = controller()->activeLayer();

	const QRect rect = a->rect().toRect();

	net::EnvelopeBuilder eb;
	rustpile::write_undopoint(eb, contextId);
	rustpile::write_deleteannotation(eb, contextId, selected());
	eb.buildPutQImage(contextId, layer, rect.x(), rect.y(), img, rustpile::Blendmode::Normal);
	controller()->client()->sendEnvelope(eb.toEnvelope());
}

}

