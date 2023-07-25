// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/widgets/recentscroll.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "libshared/util/paths.h"
#include <QColor>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QRegularExpression>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace widgets {

RecentScrollEntry *RecentScrollEntry::ofNoFiles()
{
	RecentScrollEntry *entry = new RecentScrollEntry;
	entry->initOpacity(0.8);
	entry->setText(QStringLiteral("<em style=\"font-size:large;\">%1</em>")
					   .arg(tr("No recent files.").toHtmlEscaped()));
	return entry;
}

RecentScrollEntry *
RecentScrollEntry::ofFile(const utils::RecentFiles::File &file)
{
	RecentScrollEntry *entry = new RecentScrollEntry;
	entry->initDeleteButton();
	entry->setText(
		QStringLiteral("<u style=\"font-size:large;\">%1</u>")
			.arg(makeWrappable(utils::paths::extractBasename(file.path))
					 .toHtmlEscaped()));
	entry->setToolTip(QFileInfo{file.path}.absoluteFilePath());
	entry->setCursor(Qt::PointingHandCursor);
	return entry;
}

void RecentScrollEntry::mousePressEvent(QMouseEvent *event)
{
	QWidget::mousePressEvent(event);
	if(!event->isAccepted() && event->button() == Qt::LeftButton) {
		emit selected();
	}
}

RecentScrollEntry::RecentScrollEntry()
	: QWidget{}
	, m_layout{new QHBoxLayout}
	, m_text{new QLabel}
	, m_deleteButton{nullptr}
{
	m_layout->setContentsMargins(0, 0, 0, 0);
	setLayout(m_layout);

	m_text->setTextFormat(Qt::RichText);
	m_text->setWordWrap(true);
	m_layout->addWidget(m_text);
}

void RecentScrollEntry::initOpacity(double opacity)
{
	QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect;
	effect->setOpacity(opacity);
	m_text->setGraphicsEffect(effect);
}

void RecentScrollEntry::initDeleteButton()
{
	m_deleteButton = new QToolButton;
	// Small is too small, large is too large, so we pick the middle.
	QStyle *s = style();
	int iconSize = (s->pixelMetric(QStyle::PM_SmallIconSize) +
					s->pixelMetric(QStyle::PM_LargeIconSize)) /
				   2;
	m_deleteButton->setIconSize(QSize{iconSize, iconSize});
	m_deleteButton->setIcon(QIcon::fromTheme("trash-empty"));
	m_deleteButton->setToolTip(tr("Remove"));
	m_deleteButton->setAutoRaise(true);
	m_deleteButton->setCursor(Qt::ArrowCursor);
	m_layout->addWidget(m_deleteButton);
	connect(
		m_deleteButton, &QAbstractButton::clicked, this,
		&RecentScrollEntry::deleteRequested);
}

void RecentScrollEntry::setText(const QString &text)
{
	m_text->setText(text);
}

QString RecentScrollEntry::makeWrappable(const QString &s)
{
	// QLabels don't support text wrapping anywhere, only word wrapping. So
	// we have to do this stupid hack of splitting the text into grapheme
	// clusters ("characters" as humans see them) and joining them with
	// zero-width spaces that the label will wrap them at.
	static QRegularExpression graphemeClusterRe{"\\X"};
	QStringList graphemeClusters;
	QRegularExpressionMatchIterator it = graphemeClusterRe.globalMatch(s);
	while(it.hasNext()) {
		graphemeClusters.append(it.next().captured());
	}
	static constexpr char zeroWidthSpace[] = "\u200b";
	return graphemeClusters.join(zeroWidthSpace);
}


RecentScroll::RecentScroll(Mode mode, QWidget *parent)
	: QScrollArea{parent}
	, m_mode{mode}
{
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setWidgetResizable(true);

	m_content = new QWidget;
	setWidget(m_content);

	m_layout = new QVBoxLayout;
	m_layout->addStretch();
	m_content->setLayout(m_layout);

	if(mode == Mode::Files) {
		utils::RecentFiles &recentFiles = dpApp().recentFiles();
		connect(
			&recentFiles, &utils::RecentFiles::recentFilesChanged, this,
			&RecentScroll::updateFiles);
		updateFiles();
	} else {
		qWarning("Unknown recent scroll mode %d", int(mode));
	}
}

void RecentScroll::updateFiles()
{
	utils::ScopedUpdateDisabler disabler{this};
	utils::RecentFiles &recentFiles = dpApp().recentFiles();
	QVector<utils::RecentFiles::File> files = recentFiles.getFiles();
	clearEntries();
	if(files.isEmpty()) {
		addEntry(RecentScrollEntry::ofNoFiles());
	} else {
		for(const utils::RecentFiles::File &file : files) {
			RecentScrollEntry *entry = RecentScrollEntry::ofFile(file);
			connect(
				entry, &RecentScrollEntry::selected, this,
				[this, path = file.path] {
					emit selected(path);
				});
			connect(
				entry, &RecentScrollEntry::deleteRequested, this,
				[&recentFiles, id = file.id] {
					recentFiles.removeFileById(id);
				});
			addEntry(entry);
		}
	}
}

void RecentScroll::resizeEvent(QResizeEvent *event)
{
	QScrollArea::resizeEvent(event);
	m_content->setFixedWidth(viewport()->width());
}

void RecentScroll::addEntry(RecentScrollEntry *entry)
{
	m_layout->insertWidget(m_entries.size(), entry);
	m_entries.append(entry);
}

void RecentScroll::clearEntries()
{
	for(RecentScrollEntry *entry : m_entries) {
		delete entry;
	}
	m_entries.clear();
}

}
