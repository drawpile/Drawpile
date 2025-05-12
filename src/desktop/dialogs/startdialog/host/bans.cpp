// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/host/bans.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {
namespace host {

Bans::Bans(QWidget *parent)
	: QWidget(parent)
{
	setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	m_noBansLabel = new QLabel(tr("No bans imported."));
	m_noBansLabel->setAlignment(Qt::AlignCenter);
	m_noBansLabel->setTextFormat(Qt::PlainText);
	m_noBansLabel->setWordWrap(true);
	layout->addWidget(m_noBansLabel, 1);

	m_banListWidget = new QWidget;
	m_banListWidget->setContentsMargins(0, 0, 0, 0);
	m_banListWidget->hide();
	layout->addWidget(m_banListWidget);

	QVBoxLayout *banListLayout = new QVBoxLayout(m_banListWidget);
	banListLayout->setContentsMargins(0, 0, 0, 0);

	m_banListView = new QListWidget;
	m_banListView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_banListView->setEnabled(false);
	banListLayout->addWidget(m_banListView);
	connect(
		m_banListView, &QListWidget::itemSelectionChanged, this,
		&Bans::updateButton);

	QHBoxLayout *buttonLayout = new QHBoxLayout;
	buttonLayout->setContentsMargins(0, 0, 0, 0);
	buttonLayout->addStretch();
	banListLayout->addLayout(buttonLayout);

	buttonLayout->addStretch();

	m_removeButton =
		new QPushButton(QIcon::fromTheme("list-remove"), tr("Remove"));
	buttonLayout->addWidget(m_removeButton);
	connect(
		m_removeButton, &QPushButton::clicked, this, &Bans::removeSelectedBans);

	QJsonDocument doc =
		QJsonDocument::fromJson(dpApp().settings().lastSessionBanList());
	if(doc.isArray()) {
		loadBanList(doc.array());
	} else {
		updateBanListVisibility();
	}
}

void Bans::reset(bool replaceBans)
{
	if(replaceBans) {
		m_banListView->clear();
		updateBanListVisibility();
		saveBanList();
	}
}

void Bans::load(const QJsonObject &json, bool replaceBans)
{
	if(replaceBans) {
		m_banListView->clear();
	}
	if(json.contains(QStringLiteral("list"))) {
		loadBanList(json.value(QStringLiteral("list")).toArray());
	}
	updateBanListVisibility();
	saveBanList();
}

QJsonObject Bans::save() const
{
	QJsonObject json;
	json[QStringLiteral("list")] = banListToJson();
	return json;
}

void Bans::host(QStringList &outBans)
{
	int count = m_banListView->count();
	for(int i = 0; i < count; ++i) {
		outBans.append(m_banListView->item(i)
						   ->data(Qt::UserRole)
						   .toJsonObject()
						   .value(QStringLiteral("data"))
						   .toString());
	}
}

void Bans::removeSelectedBans()
{
	for(QListWidgetItem *item : m_banListView->selectedItems()) {
		delete item;
	}
}

void Bans::updateButton()
{
	m_removeButton->setEnabled(!m_banListView->selectedItems().isEmpty());
}

void Bans::updateBanListVisibility()
{
	if(m_banListView->count() == 0) {
		if(m_banListView->isEnabled()) {
			m_banListView->setEnabled(false);
			m_banListWidget->hide();
			m_noBansLabel->show();
		}
	} else {
		if(!m_banListView->isEnabled()) {
			m_noBansLabel->hide();
			m_banListView->setEnabled(true);
			m_banListWidget->show();
			updateButton();
		}
	}
}

void Bans::loadBanList(const QJsonArray &bans)
{
	for(const QJsonValue &value : bans) {
		QJsonObject object = value.toObject();
		QString banName = object.value(QStringLiteral("name")).toString();
		QString banData = object.value(QStringLiteral("data")).toString();
		QJsonValue banEncrypted = object.value(QStringLiteral("encrypted"));
		if(!banName.isEmpty() && !banData.isEmpty() && banEncrypted.isBool()) {
			int count = m_banListView->count();
			QVector<QListWidgetItem *> itemsToDelete;
			for(int i = 0; i < count; ++i) {
				QListWidgetItem *item = m_banListView->item(i);
				if(item->data(Qt::UserRole)
					   .toJsonObject()[QStringLiteral("data")]
					   .toString() == banData) {
					itemsToDelete.append(item);
				}
			}

			for(QListWidgetItem *item : itemsToDelete) {
				delete item;
			}

			QListWidgetItem *item = new QListWidgetItem;
			item->setText(banName);
			item->setData(
				Qt::UserRole,
				QJsonObject({
					{QStringLiteral("name"), banName},
					{QStringLiteral("data"), banData},
					{QStringLiteral("encrypted"), banEncrypted.toBool()},
				}));
			m_banListView->addItem(item);
		}
	}
	m_banListView->sortItems();
	updateBanListVisibility();
}

void Bans::saveBanList()
{
	dpApp().settings().setLastSessionBanList(
		QJsonDocument(banListToJson()).toJson(QJsonDocument::Compact));
}

QJsonArray Bans::banListToJson() const
{
	QJsonArray bans;
	int count = m_banListView->count();
	for(int i = 0; i < count; ++i) {
		QListWidgetItem *item = m_banListView->item(i);
		bans.append(item->data(Qt::UserRole).toJsonObject());
	}
	return bans;
}

}
}
}
