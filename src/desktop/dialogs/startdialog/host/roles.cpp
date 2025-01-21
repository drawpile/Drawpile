// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/host/roles.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/net/authlistmodel.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {
namespace host {

Roles::Roles(QWidget *parent)
	: QWidget(parent)
{
	setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	QFormLayout *form = new QFormLayout;
	form->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(form);

	m_operatorPasswordEdit = new QLineEdit;
	m_operatorPasswordEdit->setPlaceholderText(
		tr("Define a password to let anyone become operator"));
	form->addRow(tr("Operator password:"), m_operatorPasswordEdit);

	utils::addFormSeparator(layout);

	m_noAuthListLabel = new QLabel(tr("No roles imported."));
	m_noAuthListLabel->setAlignment(Qt::AlignCenter);
	m_noAuthListLabel->setTextFormat(Qt::PlainText);
	m_noAuthListLabel->setWordWrap(true);
	layout->addWidget(m_noAuthListLabel, 1);

	m_authListWidget = new QWidget;
	m_authListWidget->setContentsMargins(0, 0, 0, 0);
	m_authListWidget->setEnabled(false);
	m_authListWidget->hide();
	layout->addWidget(m_authListWidget);

	QVBoxLayout *authListLayout = new QVBoxLayout(m_authListWidget);
	authListLayout->setContentsMargins(0, 0, 0, 0);

	m_authListModel = new net::AuthListModel(this);
	m_authListModel->setIsOperator(true);

	m_authListProxyModel = new QSortFilterProxyModel(this);
	m_authListProxyModel->setDynamicSortFilter(true);
	m_authListProxyModel->setSourceModel(m_authListModel);
	m_authListProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
	m_authListProxyModel->sort(0);

	m_authListTable = new QTableView;
	m_authListTable->setModel(m_authListProxyModel);
	m_authListTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_authListTable->setSelectionMode(QAbstractItemView::SingleSelection);
	authListLayout->addWidget(m_authListTable);

	QHeaderView *authListHeader = m_authListTable->horizontalHeader();
	authListHeader->setSectionResizeMode(0, QHeaderView::Stretch);
	authListHeader->setSectionResizeMode(1, QHeaderView::Stretch);

	connect(
		m_authListModel, &net::AuthListModel::updateApplied,
		m_authListTable->viewport(), QOverload<>::of(&QWidget::update));
	connect(
		m_authListTable->selectionModel(),
		&QItemSelectionModel::selectionChanged, this,
		&Roles::updateAuthListCheckboxes);

	QHBoxLayout *checkBoxLayout = new QHBoxLayout;
	checkBoxLayout->setContentsMargins(0, 0, 0, 0);
	checkBoxLayout->addStretch();
	authListLayout->addLayout(checkBoxLayout);

	m_opBox = new QCheckBox(tr("Operator"));
	checkBoxLayout->addWidget(m_opBox);
	connect(
		m_opBox, COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox), this,
		&Roles::opBoxChanged);

	m_trustedBox = new QCheckBox(tr("Trusted"));
	checkBoxLayout->addWidget(m_trustedBox);
	connect(
		m_trustedBox, COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox), this,
		&Roles::trustedBoxChanged);

	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindLastSessionOpPassword(m_operatorPasswordEdit);

	QJsonDocument doc = QJsonDocument::fromJson(settings.lastSessionAuthList());
	if(doc.isArray()) {
		m_authListModel->load(doc.array());
	}

	updateAuthListVisibility();
}

void Roles::reset(bool replaceAuth)
{
	desktop::settings::Settings &settings = dpApp().settings();
	settings.setLastSessionOpPassword(QString());
	if(replaceAuth) {
		m_authListModel->clear();
	}
	updateAuthListVisibility();
	saveAuthList();
}

void Roles::load(const QJsonObject &json, bool replaceAuth)
{
	desktop::settings::Settings &settings = dpApp().settings();
	if(json.contains(QStringLiteral("opword"))) {
		settings.setLastSessionOpPassword(
			json[QStringLiteral("opword")].toString());
	}
	if(json.contains(QStringLiteral("auth"))) {
		if(replaceAuth) {
			m_authListModel->clear();
		}
		m_authListModel->load(json[QStringLiteral("auth")].toArray());
	}
	updateAuthListVisibility();
	saveAuthList();
}

QJsonObject Roles::save() const
{
	QJsonObject json;
	json[QStringLiteral("opword")] = m_operatorPasswordEdit->text().trimmed();
	json[QStringLiteral("auth")] = authListToJson();
	return json;
}

void Roles::host(QString &outOperatorPassword, QJsonArray &outAuth)
{
	outOperatorPassword = m_operatorPasswordEdit->text().trimmed();
	outAuth = authListToJson();
}

void Roles::opBoxChanged(compat::CheckBoxState state)
{
	QModelIndex idx = getSelectedAuthListEntry();
	if(idx.isValid()) {
		m_authListModel->setIndexIsOp(idx, state != Qt::Unchecked);
		saveAuthList();
	}
}

void Roles::trustedBoxChanged(compat::CheckBoxState state)
{
	QModelIndex idx = getSelectedAuthListEntry();
	if(idx.isValid()) {
		m_authListModel->setIndexIsTrusted(idx, state != Qt::Unchecked);
		saveAuthList();
	}
}

void Roles::updateAuthListCheckboxes()
{
	bool enableOp = false;
	bool enableTrusted = false;
	bool checkOp = false;
	bool checkTrusted = false;
	QModelIndex idx = getSelectedAuthListEntry();
	if(idx.isValid() && !idx.data(net::AuthListModel::IsModRole).toBool()) {
		enableOp = !idx.data(net::AuthListModel::IsOwnRole).toBool();
		enableTrusted = true;
		checkOp = idx.data(net::AuthListModel::IsOpRole).toBool();
		checkTrusted = idx.data(net::AuthListModel::IsTrustedRole).toBool();
	}
	QSignalBlocker authOpBlocker(m_opBox);
	QSignalBlocker authTrustedBlocker(m_trustedBox);
	m_opBox->setEnabled(enableOp);
	m_trustedBox->setEnabled(enableTrusted);
	m_opBox->setChecked(checkOp);
	m_trustedBox->setChecked(checkTrusted);
}

QModelIndex Roles::getSelectedAuthListEntry()
{
	if(m_authListTable->isEnabled()) {
		QModelIndexList selected =
			m_authListTable->selectionModel()->selectedRows();
		if(selected.size() == 1) {
			return m_authListProxyModel->mapToSource(selected.at(0));
		}
	}
	return QModelIndex();
}

void Roles::updateAuthListVisibility()
{
	if(m_authListModel->isEmpty()) {
		if(m_authListWidget->isEnabled()) {
			m_authListWidget->setEnabled(false);
			m_authListWidget->hide();
			m_noAuthListLabel->show();
		}
	} else {
		if(!m_authListWidget->isEnabled()) {
			m_noAuthListLabel->hide();
			m_authListWidget->setEnabled(true);
			m_authListWidget->show();
			updateAuthListCheckboxes();
		}
	}
}

void Roles::saveAuthList()
{
	dpApp().settings().setLastSessionAuthList(
		QJsonDocument(authListToJson()).toJson(QJsonDocument::Compact));
}

QJsonArray Roles::authListToJson() const
{
	QJsonArray auth;
	for(const net::AuthListEntry &entry : m_authListModel->entries()) {
		if(!entry.authId.isEmpty() && (entry.op || entry.trusted)) {
			QJsonObject o;
			o[QStringLiteral("a")] = entry.authId;
			o[QStringLiteral("u")] = entry.username;
			if(entry.op) {
				o[QStringLiteral("o")] = true;
			}
			if(entry.trusted) {
				o[QStringLiteral("t")] = true;
			}
			auth.append(o);
		}
	}
	return auth;
}

}
}
}
