// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/widgets/recentscroll.h"
#include "cmake-config/config.h"
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

RecentScrollEntry *RecentScrollEntry::ofFile(const utils::Recents::File &file)
{
	RecentScrollEntry *entry = new RecentScrollEntry;
	// Small is too small, large is too large, so we pick the middle.
	QStyle *s = entry->style();
	entry->initDeleteButton(
		(s->pixelMetric(QStyle::PM_SmallIconSize) +
		 s->pixelMetric(QStyle::PM_LargeIconSize)) /
		2);
	entry->setText(
		QStringLiteral("<u style=\"font-size:large;\">%1</u>")
			.arg(makeWrappable(utils::paths::extractBasename(file.path))
					 .toHtmlEscaped()));
	entry->setToolTip(QFileInfo{file.path}.absoluteFilePath());
	entry->setCursor(Qt::PointingHandCursor);
	return entry;
}

RecentScrollEntry *RecentScrollEntry::ofNoHosts()
{
	RecentScrollEntry *entry = new RecentScrollEntry;
	entry->initOpacity(0.8);
	entry->setText(tr("No recent hosts.").toHtmlEscaped());
	return entry;
}

RecentScrollEntry *RecentScrollEntry::ofHost(const utils::Recents::Host &rh)
{
	RecentScrollEntry *entry = new RecentScrollEntry;
	entry->initDeleteButton(
		entry->style()->pixelMetric(QStyle::PM_ButtonIconSize));
	QString text = rh.port == cmake_config::proto::port()
					   ? rh.host
					   : QStringLiteral("%1:%2").arg(rh.host).arg(rh.port);
	entry->setText(QStringLiteral("<u>%1</u>").arg(text.toHtmlEscaped()));
	entry->setCursor(Qt::PointingHandCursor);
	return entry;
}

void RecentScrollEntry::mousePressEvent(QMouseEvent *event)
{
	QWidget::mousePressEvent(event);
	if(!event->isAccepted() && event->button() == Qt::LeftButton) {
		emit clicked();
	}
}

void RecentScrollEntry::mouseDoubleClickEvent(QMouseEvent *event)
{
	QWidget::mouseDoubleClickEvent(event);
	if(!event->isAccepted() && event->button() == Qt::LeftButton) {
		emit doubleClicked();
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

void RecentScrollEntry::initDeleteButton(int iconSize)
{
	m_deleteButton = new QToolButton;
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
	setWidgetResizable(true);
	utils::bindKineticScrollingWith(
		this, Qt::ScrollBarAlwaysOff, Qt::ScrollBarAsNeeded);

	m_content = new QWidget;
	setWidget(m_content);

	m_layout = new QVBoxLayout;
	m_layout->addStretch();
	m_content->setLayout(m_layout);

	if(mode == Mode::Files) {
		utils::Recents &recents = dpApp().recents();
		connect(
			&recents, &utils::Recents::recentFilesChanged, this,
			&RecentScroll::updateFiles);
		updateFiles();
	} else if(mode == Mode::Join) {
		utils::Recents &recents = dpApp().recents();
		connect(
			&recents, &utils::Recents::recentHostsChanged, this,
			&RecentScroll::updateHosts);
		updateHosts();
	} else {
		qWarning("Unknown recent scroll mode %d", int(mode));
	}
}

void RecentScroll::updateFiles()
{
	utils::ScopedUpdateDisabler disabler{this};
	utils::Recents &recents = dpApp().recents();
	QVector<utils::Recents::File> files = recents.getFiles();
	clearEntries();
	if(files.isEmpty()) {
		addEntry(RecentScrollEntry::ofNoFiles());
	} else {
		for(const utils::Recents::File &file : files) {
			RecentScrollEntry *entry = RecentScrollEntry::ofFile(file);
			connect(
				entry, &RecentScrollEntry::clicked, this,
				[this, path = file.path] {
					emit clicked(path);
				});
			connect(
				entry, &RecentScrollEntry::deleteRequested, this,
				[&recents, id = file.id] {
					recents.removeFileById(id);
				});
			addEntry(entry);
		}
	}
}

void RecentScroll::updateHosts()
{
	utils::ScopedUpdateDisabler disabler{this};
	utils::Recents &recents = dpApp().recents();
	QVector<utils::Recents::Host> rhs = recents.getHosts();
	clearEntries();
	if(rhs.isEmpty()) {
		addEntry(RecentScrollEntry::ofNoHosts());
	} else {
		for(const utils::Recents::Host &rh : rhs) {
			RecentScrollEntry *entry = RecentScrollEntry::ofHost(rh);
			QString value = rh.toString();
			connect(entry, &RecentScrollEntry::clicked, this, [this, value] {
				emit clicked(value);
			});
			connect(
				entry, &RecentScrollEntry::doubleClicked, this, [this, value] {
					emit doubleClicked(value);
				});
			connect(
				entry, &RecentScrollEntry::deleteRequested, this,
				[&recents, id = rh.id] {
					recents.removeHostById(id);
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
		entry->setParent(nullptr);
		entry->deleteLater();
	}
	m_entries.clear();
}

}
