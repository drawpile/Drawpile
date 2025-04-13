// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpmsg/ids.h>
}
#include "desktop/scene/annotationitem.h"
#include "desktop/toolwidgets/annotationsettings.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/view/canvaswrapper.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/userlist.h"
#include "libclient/net/client.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/utils/annotations.h"
#include "ui_textsettings.h"
#include <QActionGroup>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QScopedValueRollback>
#include <QStackedWidget>
#include <QTextBlock>
#include <QTimer>
#include <QVBoxLayout>

namespace tools {

AnnotationSettings::AnnotationSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
{
}

AnnotationSettings::~AnnotationSettings()
{
	delete m_ui;
}

bool AnnotationSettings::isLocked()
{
	return !m_annotationsShown;
}

QWidget *AnnotationSettings::createUiWidget(QWidget *parent)
{
	m_stack = new QStackedWidget(parent);
	m_stack->setContentsMargins(0, 0, 0, 0);

	QWidget *uiWidget = new QWidget(parent);
	m_ui = new Ui_TextSettings;
	m_ui->setupUi(uiWidget);
	m_stack->addWidget(uiWidget);

	// Set up the header widget
	m_headerWidget = new QWidget(parent);
	m_ui->headerLayout->setParent(nullptr);
	m_ui->headerLayout->setContentsMargins(0, 0, 1, 0);
	m_headerWidget->setLayout(m_ui->headerLayout);

	m_protectedAction =
		new QAction(QIcon::fromTheme("object-locked"), tr("Protect"), this);
	m_protectedAction->setCheckable(true);
	m_ui->protectButton->setDefaultAction(m_protectedAction);

	QAction *mergeAction =
		new QAction(QIcon::fromTheme("arrow-down-double"), tr("Merge"), this);
	m_ui->mergeButton->setDefaultAction(mergeAction);

	QAction *deleteAction =
		new QAction(QIcon::fromTheme("list-remove"), tr("Delete"), this);
	m_ui->deleteButton->setDefaultAction(deleteAction);

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
	QMenu *halignMenu = new QMenu(parent);
	halignMenu->addAction(QIcon::fromTheme("format-justify-left"), tr("Left"))
		->setProperty(HALIGN_PROP, Qt::AlignLeft);
	halignMenu
		->addAction(QIcon::fromTheme("format-justify-center"), tr("Center"))
		->setProperty(HALIGN_PROP, Qt::AlignCenter);
	halignMenu
		->addAction(QIcon::fromTheme("format-justify-fill"), tr("Justify"))
		->setProperty(HALIGN_PROP, Qt::AlignJustify);
	halignMenu->addAction(QIcon::fromTheme("format-justify-right"), tr("Right"))
		->setProperty(HALIGN_PROP, Qt::AlignRight);
	m_ui->halign->setIcon(halignMenu->actions().constFirst()->icon());
	connect(
		halignMenu, &QMenu::triggered, this,
		&AnnotationSettings::changeAlignment);
	m_ui->halign->setMenu(halignMenu);

	// Vertical alignment options
	QMenu *valignMenu = new QMenu(parent);
	valignMenu
		->addAction(QIcon::fromTheme("format-align-vertical-top"), tr("Top"))
		->setProperty(VALIGN_PROP, 0);
	valignMenu
		->addAction(
			QIcon::fromTheme("format-align-vertical-center"), tr("Center"))
		->setProperty(VALIGN_PROP, DP_MSG_ANNOTATION_EDIT_FLAGS_VALIGN_CENTER);
	valignMenu
		->addAction(
			QIcon::fromTheme("format-align-vertical-bottom"), tr("Bottom"))
		->setProperty(VALIGN_PROP, DP_MSG_ANNOTATION_EDIT_FLAGS_VALIGN_BOTTOM);
	m_ui->valign->setIcon(valignMenu->actions().constFirst()->icon());
	connect(
		valignMenu, &QMenu::triggered, this,
		&AnnotationSettings::changeAlignment);
	m_ui->valign->setMenu(valignMenu);

	QMenu *renderMenu = new QMenu(parent);
	renderMenu->addAction(QIcon::fromTheme("draw-bezier-curves"), tr("Vector"))
		->setProperty(RENDER_PROP, 0);
	renderMenu->addAction(QIcon::fromTheme("drawpile_round"), tr("Smooth"))
		->setProperty(RENDER_PROP, DP_MSG_ANNOTATION_EDIT_FLAGS_RASTERIZE);
	renderMenu->addAction(QIcon::fromTheme("drawpile_pixelround"), tr("Pixel"))
		->setProperty(
			RENDER_PROP, DP_MSG_ANNOTATION_EDIT_FLAGS_ALIAS |
							 DP_MSG_ANNOTATION_EDIT_FLAGS_RASTERIZE);
	m_ui->render->setIcon(renderMenu->actions().constFirst()->icon());
	connect(
		renderMenu, &QMenu::triggered, this, &AnnotationSettings::changeRender);
	m_ui->render->setMenu(renderMenu);

	m_ui->btnTextColor->setColor(Qt::black);
	m_ui->btnBackground->setColor(Qt::transparent);

	// Editor events
	connect(
		m_ui->content, &QTextEdit::textChanged, this,
		&AnnotationSettings::applyChanges);
	connect(
		m_ui->content, &QTextEdit::cursorPositionChanged, this,
		&AnnotationSettings::updateStyleButtons);

	connect(
		m_ui->btnBackground, &widgets::ColorButton::colorChanged, this,
		&AnnotationSettings::setEditorBackgroundColor);
	connect(
		m_ui->btnBackground, &widgets::ColorButton::colorChanged, this,
		&AnnotationSettings::applyChanges);

	connect(
		deleteAction, &QAction::triggered, this,
		&AnnotationSettings::removeAnnotation);
	connect(mergeAction, &QAction::triggered, this, &AnnotationSettings::bake);
	connect(
		m_protectedAction, &QAction::triggered, this,
		&AnnotationSettings::saveChanges);

	connect(
		m_ui->font, SIGNAL(currentIndexChanged(int)), this,
		SLOT(updateFontIfUniform()));
	connect(
		m_ui->size, SIGNAL(valueChanged(double)), this,
		SLOT(updateFontIfUniform()));
	connect(
		m_ui->btnTextColor, &widgets::ColorButton::colorChanged, this,
		&AnnotationSettings::updateFontIfUniform);

	// Intra-editor connections that couldn't be made in the UI designer
	connect(
		m_ui->bold, &QToolButton::toggled, this,
		&AnnotationSettings::toggleBold);
	connect(
		m_ui->strikethrough, &QToolButton::toggled, this,
		&AnnotationSettings::toggleStrikethrough);

	connect(
		m_updatetimer, &QTimer::timeout, this,
		&AnnotationSettings::saveChanges);

	// Select a nice default font
	QStringList defaultFonts;
	defaultFonts << "Arial"
				 << "Helvetica"
				 << "Sans Serif";
	for(const QString &df : defaultFonts) {
		int i = m_ui->font->findText(df, Qt::MatchFixedString);
		if(i >= 0) {
			m_ui->font->setCurrentIndex(i);
			break;
		}
	}

	// Set initial content format
	resetContentFormat();
	setUiEnabled(false);

	QWidget *disabledWidget = new QWidget;
	QVBoxLayout *disabledLayout = new QVBoxLayout(disabledWidget);

	m_annotationsHiddenLabel =
		new QLabel(QStringLiteral("%1<a href=\"#\">%2</a>")
					   .arg(
						   //: This is part of the sentence "Annotations are
						   //: hidden. _Show_". The latter is a clickable link.
						   tr("Annotations are hidden. ").toHtmlEscaped(),
						   //: This is part of the sentence "Annotations are
						   //: hidden. _Show_". The latter is a clickable link.
						   tr("Show").toHtmlEscaped()));
	m_annotationsHiddenLabel->setTextFormat(Qt::RichText);
	m_annotationsHiddenLabel->setWordWrap(true);
	disabledLayout->addWidget(m_annotationsHiddenLabel);
	connect(
		m_annotationsHiddenLabel, &QLabel::linkActivated, this,
		&AnnotationSettings::showAnnotationsRequested);

	disabledLayout->addStretch();
	m_stack->addWidget(disabledWidget);

	updateWidgets();
	return m_stack;
}

QWidget *AnnotationSettings::getHeaderWidget()
{
	return m_headerWidget;
}

void AnnotationSettings::setAnnotationsShown(bool annotationsShown)
{
	if(!annotationsShown && m_annotationsShown) {
		controller()->getTool(Tool::ANNOTATION)->cancelMultipart();
	}
	m_annotationsShown = annotationsShown;
	updateWidgets();
}

void AnnotationSettings::setUiEnabled(bool enabled)
{
	QWidget *widgets[] = {
		m_ui->content, m_ui->btnBackground, m_ui->btnTextColor,
		m_ui->halign,  m_ui->valign,		m_ui->bold,
		m_ui->italic,  m_ui->underline,		m_ui->strikethrough,
		m_ui->font,	   m_ui->size,			m_ui->render};
	for(QWidget *w : widgets) {
		w->setEnabled(enabled);
	}
	m_editActions->setEnabled(enabled);
	if(!enabled) {
		m_ui->content->setText(QString());
		m_protectedAction->setChecked(false);
		m_ui->creatorLabel->setText(QString{});
	}
}

bool AnnotationSettings::shouldAlias() const
{
	return m_ui->render->property(RENDER_PROP).toInt() &
		   DP_MSG_ANNOTATION_EDIT_FLAGS_ALIAS;
}

void AnnotationSettings::setEditorBackgroundColor(const QColor &color)
{
	// Blend transparent colors with white
	const QColor c = QColor::fromRgbF(
		color.redF() * color.alphaF() + (1 - color.alphaF()),
		color.greenF() * color.alphaF() + (1 - color.alphaF()),
		color.blueF() * color.alphaF() + (1 - color.alphaF()));

	// We need to use the stylesheet because native styles ignore the palette.
	m_ui->content->setStyleSheet(
		QStringLiteral("QTextEdit { background-color: %1; }").arg(c.name()));
}

void AnnotationSettings::updateStyleButtons()
{
	if(m_ui->content->document()->isEmpty()) {
		// Qt inexplicably resets the content format to some goofy default
		// values when the user clears all text, so undo that silliness.
		resetContentFormat();
	} else {
		QTextBlockFormat bf = m_ui->content->textCursor().blockFormat();
		switch(bf.alignment()) {
		case Qt::AlignLeft:
			m_ui->halign->setIcon(QIcon::fromTheme("format-justify-left"));
			break;
		case Qt::AlignCenter:
			m_ui->halign->setIcon(QIcon::fromTheme("format-justify-center"));
			break;
		case Qt::AlignJustify:
			m_ui->halign->setIcon(QIcon::fromTheme("format-justify-fill"));
			break;
		case Qt::AlignRight:
			m_ui->halign->setIcon(QIcon::fromTheme("format-justify-right"));
			break;
		default:
			break;
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

void AnnotationSettings::changeRender(const QAction *action)
{
	m_ui->render->setIcon(action->icon());
	m_ui->render->setProperty(RENDER_PROP, action->property(RENDER_PROP));
	applyChanges();
}

void AnnotationSettings::updateFontIfUniform()
{
	QTextDocument *doc = m_ui->content->document();
	if(doc->isEmpty()) {
		resetContentFormat();
	} else {
		bool uniformFontFamily = true;
		bool uniformSize = true;
		bool uniformColor = true;
		QTextBlock b = doc->firstBlock();
		QTextCharFormat fmt1;
		bool first = true;

		// Check all character formats in all blocks. If they are the same,
		// we can reset the font for the wole document.
		while(b.isValid()) {
			const auto textFormats = b.textFormats();
			for(const QTextLayout::FormatRange &fr : textFormats) {

				if(first) {
					fmt1 = fr.format;
					first = false;

				} else {
					uniformFontFamily &= compat::fontFamily(fr.format) ==
										 compat::fontFamily(fmt1);
					uniformSize &= qFuzzyCompare(
						fr.format.fontPointSize(), fmt1.fontPointSize());
					uniformColor &= fr.format.foreground() == fmt1.foreground();
				}
			}
			b = b.next();
		}

		resetContentFont(uniformFontFamily, uniformSize, uniformColor);
	}
}

void AnnotationSettings::resetContentFormat()
{
	QTextCharFormat fmt;
	compat::setFontFamily(fmt, m_ui->font->currentText());
	fmt.setFontPointSize(m_ui->size->value());
	fmt.setForeground(m_ui->btnTextColor->color());
	fmt.setFontStyleStrategy(
		shouldAlias() ? QFont::NoAntialias : QFont::PreferDefault);
	m_ui->content->setCurrentCharFormat(fmt);
}

void AnnotationSettings::resetContentFont(
	bool resetFamily, bool resetSize, bool resetColor)
{
	if(!(resetFamily | resetSize | resetColor))
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

void AnnotationSettings::setFontFamily(QTextCharFormat &fmt)
{
	compat::setFontFamily(fmt, m_ui->font->currentText());
}

void AnnotationSettings::setSelectionId(int id)
{
	QScopedValueRollback<bool> rollback(m_noupdate, true);
	setUiEnabled(id > 0);

	m_selectionId = id;
	if(id) {
		const drawingboard::AnnotationItem *a =
			m_canvasView->getAnnotationItem(id);
		if(!a)
			return;

		const QString text = a->text();
		m_ui->content->setHtml(text);
		m_ui->btnBackground->setColor(a->color());
		setEditorBackgroundColor(a->color());
		if(text.isEmpty()) {
			resetContentFormat();
		}

		int valign;
		QIcon valignIcon;
		switch(a->valign()) {
		case 1:
			valign = DP_MSG_ANNOTATION_EDIT_FLAGS_VALIGN_CENTER;
			valignIcon = QIcon::fromTheme("format-align-vertical-center");
			break;
		case 2:
			valign = DP_MSG_ANNOTATION_EDIT_FLAGS_VALIGN_BOTTOM;
			valignIcon = QIcon::fromTheme("format-align-vertical-bottom");
			break;
		default:
			valign = 0;
			valignIcon = QIcon::fromTheme("format-align-vertical-top");
			break;
		}
		m_ui->valign->setProperty(VALIGN_PROP, valign);
		m_ui->valign->setIcon(valignIcon);

		int render;
		QIcon renderIcon;
		if(a->rasterize()) {
			if(a->alias()) {
				render = DP_MSG_ANNOTATION_EDIT_FLAGS_ALIAS |
						 DP_MSG_ANNOTATION_EDIT_FLAGS_RASTERIZE;
				renderIcon = QIcon::fromTheme("drawpile_pixelround");
			} else {
				render = DP_MSG_ANNOTATION_EDIT_FLAGS_RASTERIZE;
				renderIcon = QIcon::fromTheme("drawpile_round");
			}
		} else {
			render = 0;
			renderIcon = QIcon::fromTheme("draw-bezier-curves");
		}
		m_ui->render->setProperty(RENDER_PROP, render);
		m_ui->render->setIcon(renderIcon);

		m_ui->creatorLabel->setText(
			controller()->model()->userlist()->getUsername(a->userId()));
		m_protectedAction->setChecked(a->protect());

		const bool opOrOwner =
			controller()->model()->aclState()->amOperator() ||
			DP_annotation_id_owner(a->id(), controller()->client()->myId());
		m_protectedAction->setEnabled(opOrOwner);

		if(a->protect() && !opOrOwner)
			setUiEnabled(false);
	}

	emit selectionIdChanged(m_selectionId);
}

void AnnotationSettings::setFocusAt(int cursorPos)
{
	m_ui->content->setFocus();
	if(cursorPos >= 0) {
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
	if(!m_noupdate && selected()) {
		QScopedValueRollback<bool> rollback(m_noupdate, true);
		utils::setAliasedAnnotationFonts(
			m_ui->content->document(), shouldAlias());
		m_updatetimer->start();
	}
}

void AnnotationSettings::saveChanges()
{
	if(!selected())
		return;

	m_updatetimer->stop();

	if(selected()) {
		QString content;
		if(m_ui->content->document()->isEmpty()) {
			content = QString();
		} else {
			content = m_ui->content->toHtml();
		}

		net::Client *client = controller()->client();
		uint8_t contextId = client->myId();
		uint8_t flags = (m_protectedAction->isChecked()
							 ? DP_MSG_ANNOTATION_EDIT_FLAGS_PROTECT
							 : 0) |
						uint8_t(m_ui->valign->property(VALIGN_PROP).toInt()) |
						uint8_t(m_ui->render->property(RENDER_PROP).toInt());
		client->sendMessage(net::makeAnnotationEditMessage(
			contextId, selected(), m_ui->btnBackground->color().rgba(), flags,
			0, content));
	}
}

void AnnotationSettings::removeAnnotation()
{
	Q_ASSERT(selected());
	net::Client *client = controller()->client();
	uint8_t contextId = client->myId();
	net::Message messages[] = {
		net::makeUndoPointMessage(contextId),
		net::makeAnnotationDeleteMessage(contextId, selected()),
	};
	client->sendCommands(DP_ARRAY_LENGTH(messages), messages);
}

void AnnotationSettings::bake()
{
	int annotationId = selected();
	if(annotationId == 0) {
		qWarning("No annotation to bake selected");
		return;
	}

	const drawingboard::AnnotationItem *a =
		m_canvasView->getAnnotationItem(selected());
	if(!a) {
		qWarning("Selected annotation %d not found!", selected());
		return;
	}

	ToolController *ctrl = controller();
	canvas::CanvasModel *canvas = ctrl->model();
	if(canvas && !canvas->aclState()->canUseFeature(DP_FEATURE_PUT_IMAGE)) {
		emit ctrl->showMessageRequested(
			tr("You don't have permission to paste merged annotations."));
		return;
	}

	net::Client *client = ctrl->client();
	uint8_t contextId = client->myId();
	int layer = ctrl->activeLayer();
	QRect rect = a->rect().toRect();
	QImage img = a->toImage();
	net::MessageList msgs = {
		net::makeUndoPointMessage(contextId),
		net::makeAnnotationDeleteMessage(contextId, selected()),
	};
	net::makePutImageMessages(
		msgs, contextId, layer, DP_BLEND_MODE_NORMAL, rect.x(), rect.y(), img);
	client->sendCommands(msgs.count(), msgs.data());
}

void AnnotationSettings::updateWidgets()
{
	if(m_stack) {
		utils::ScopedUpdateDisabler disabler(m_stack);
		m_stack->setCurrentIndex(m_annotationsShown ? 0 : 1);
		m_ui->font->setVisible(m_annotationsShown);
		m_ui->size->setVisible(m_annotationsShown);
		m_annotationsHiddenLabel->setVisible(!m_annotationsShown);
	}
}

}
