// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/host/categories.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {
namespace host {

Categories::Categories(
	const QString &text, bool loadAnnouncements, bool loadAuth, bool loadBans,
	QWidget *parent)
	: QWidget(parent)
{
	setContentsMargins(0, 0, 0, 0);
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	m_label = new QLabel(text);
	m_label->setTextFormat(Qt::PlainText);
	m_label->setWordWrap(true);
	layout->addWidget(m_label);

	QCheckBox *sessionBox = new QCheckBox(tr("Session"));
	layout->addWidget(sessionBox);
	m_boxes[int(Session)] = sessionBox;

	QCheckBox *listingBox = new QCheckBox(tr("Listing"));
	layout->addWidget(listingBox);
	m_boxes[int(Listing)] = listingBox;

	int indent = 2 * style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
	if(indent <= 0) {
		indent = 24;
	}

	if(loadAnnouncements) {
		QHBoxLayout *replaceAnnouncementsLayout = new QHBoxLayout;
		replaceAnnouncementsLayout->setContentsMargins(indent, 0, 0, 0);
		layout->addLayout(replaceAnnouncementsLayout);
		m_replaceAnnouncementsBox = new QCheckBox(tr("Replace announcements"));
		m_replaceAnnouncementsBox->setChecked(true);
		replaceAnnouncementsLayout->addWidget(m_replaceAnnouncementsBox);
	} else {
		m_replaceAnnouncementsBox = nullptr;
	}

	QCheckBox *permissionsBox = new QCheckBox(tr("Permissions"));
	layout->addWidget(permissionsBox);
	m_boxes[int(Permissions)] = permissionsBox;

	QCheckBox *rolesBox = new QCheckBox(tr("Roles"));
	layout->addWidget(rolesBox);
	m_boxes[int(Roles)] = rolesBox;

	if(loadAuth) {
		QHBoxLayout *replaceAuthLayout = new QHBoxLayout;
		replaceAuthLayout->setContentsMargins(indent, 0, 0, 0);
		layout->addLayout(replaceAuthLayout);
		m_replaceAuthBox = new QCheckBox(tr("Replace role list"));
		m_replaceAuthBox->setChecked(true);
		replaceAuthLayout->addWidget(m_replaceAuthBox);
	} else {
		m_replaceAuthBox = nullptr;
	}

	QCheckBox *bansBox = new QCheckBox(tr("Bans"));
	layout->addWidget(bansBox);
	m_boxes[int(Bans)] = bansBox;

	if(loadBans) {
		QHBoxLayout *replaceBansLayout = new QHBoxLayout;
		replaceBansLayout->setContentsMargins(indent, 0, 0, 0);
		layout->addLayout(replaceBansLayout);
		m_replaceBansBox = new QCheckBox(tr("Replace ban list"));
		m_replaceBansBox->setChecked(true);
		replaceBansLayout->addWidget(m_replaceBansBox);
	} else {
		m_replaceBansBox = nullptr;
	}
}

bool Categories::isCategoryChecked(int category) const
{
	Q_ASSERT(category >= 0);
	Q_ASSERT(category < int(Count));
	return isChecked(m_boxes[category]);
}

bool Categories::isAnyCategoryChecked() const
{
	for(const QCheckBox *box : m_boxes) {
		if(isChecked(box)) {
			return true;
		}
	}
	return false;
}

bool Categories::isReplaceAnnouncementsChecked() const
{
	return isChecked(m_replaceAnnouncementsBox);
}

bool Categories::isReplaceAuthChecked() const
{
	return isChecked(m_replaceAuthBox);
}

bool Categories::isReplaceBansChecked() const
{
	return isChecked(m_replaceBansBox);
}

QString Categories::getCategoryName(int category)
{
	switch(Category(category)) {
	case Session:
		return QStringLiteral("session");
	case Listing:
		return QStringLiteral("listing");
	case Permissions:
		return QStringLiteral("permissions");
	case Roles:
		return QStringLiteral("roles");
	case Bans:
		return QStringLiteral("bans");
	case Count:
		break;
	}
	qWarning("Unknown category %d", category);
	return QString();
}

bool Categories::isChecked(const QCheckBox *box)
{
	return box && box->isEnabled() && box->isChecked();
}

}
}
}
