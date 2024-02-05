// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/sessionfilterproxymodel.h"
#include "libclient/net/loginsessions.h"
#include "libclient/net/sessionlistingmodel.h"

SessionFilterProxyModel::SessionFilterProxyModel(QObject *parent)
	: QSortFilterProxyModel(parent)
{
}

void SessionFilterProxyModel::refreshDuplicates()
{
	if(!m_showDuplicates) {
		invalidateFilter();
	}
}

void SessionFilterProxyModel::setShowPassworded(bool show)
{
	if(m_showPassworded != show) {
		m_showPassworded = show;
		invalidateFilter();
	}
}

void SessionFilterProxyModel::setShowNsfm(bool show)
{
	if(m_showNsfm != show) {
		m_showNsfm = show;
		invalidateFilter();
	}
}

void SessionFilterProxyModel::setShowClosed(bool show)
{
	if(m_showClosed != show) {
		m_showClosed = show;
		invalidateFilter();
	}
}

void SessionFilterProxyModel::setShowInactive(bool show)
{
	if(m_showInactive != show) {
		m_showInactive = show;
		invalidateFilter();
	}
}

void SessionFilterProxyModel::setShowDuplicates(bool show)
{
	if(m_showDuplicates != show) {
		m_showDuplicates = show;
		invalidateFilter();
	}
}

bool SessionFilterProxyModel::filterAcceptsRow(
	int sourceRow, const QModelIndex &sourceParent) const
{
	if(alwaysShowTopLevel() && !sourceParent.isValid()) {
		return true;
	}

	const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

	if(!m_showNsfm && index.data(nsfmRole()).toBool()) {
		return false;
	}

	if(!m_showPassworded && index.data(passwordedRole()).toBool()) {
		return false;
	}

	if(!m_showClosed && index.data(closedRole()).toBool()) {
		return false;
	}

	if(!m_showInactive && index.data(inactiveRole()).toBool()) {
		return false;
	}

	if(!m_showDuplicates && isDuplicate(index)) {
		return false;
	}

	return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

bool SessionFilterProxyModel::isDuplicate(const QModelIndex &index) const
{
	Q_UNUSED(index);
	return false;
}


ListingSessionFilterProxyModel::ListingSessionFilterProxyModel(QObject *parent)
	: SessionFilterProxyModel(parent)
{
}

bool ListingSessionFilterProxyModel::lessThan(
	const QModelIndex &a, const QModelIndex &b) const
{
	QAbstractItemModel *model = sourceModel();
	if(a.model() == model && b.model() == model &&
	   !model->data(a, SessionListingModel::IsListingRole).toBool() &&
	   !model->data(b, SessionListingModel::IsListingRole).toBool()) {
		int ai = model->data(a, SessionListingModel::SortKeyRole).toInt();
		int bi = model->data(b, SessionListingModel::SortKeyRole).toInt();
		return sortOrder() == Qt::AscendingOrder ? ai < bi : ai > bi;
	}
	return QSortFilterProxyModel::lessThan(a, b);
}

int ListingSessionFilterProxyModel::nsfmRole() const
{
	return SessionListingModel::IsNsfwRole;
}

int ListingSessionFilterProxyModel::passwordedRole() const
{
	return SessionListingModel::IsPasswordedRole;
}

int ListingSessionFilterProxyModel::closedRole() const
{
	return SessionListingModel::IsClosedRole;
}

int ListingSessionFilterProxyModel::inactiveRole() const
{
	return SessionListingModel::IsInactiveRole;
}

bool ListingSessionFilterProxyModel::isDuplicate(const QModelIndex &index) const
{
	SessionListingModel *sessionListingModel =
		static_cast<SessionListingModel *>(sourceModel());
	QModelIndex primaryIndex = sessionListingModel->primaryIndexOfUrl(
		index.data(SessionListingModel::UrlRole).toUrl());
	return primaryIndex.isValid() && primaryIndex != index;
}

bool ListingSessionFilterProxyModel::alwaysShowTopLevel() const
{
	return true;
}


LoginSessionFilterProxyModel::LoginSessionFilterProxyModel(QObject *parent)
	: SessionFilterProxyModel(parent)
{
}

int LoginSessionFilterProxyModel::nsfmRole() const
{
	return net::LoginSessionModel::NsfmRole;
}

int LoginSessionFilterProxyModel::passwordedRole() const
{
	return net::LoginSessionModel::NeedPasswordRole;
}

int LoginSessionFilterProxyModel::closedRole() const
{
	return net::LoginSessionModel::ClosedRole;
}

int LoginSessionFilterProxyModel::inactiveRole() const
{
	return net::LoginSessionModel::InactiveRole;
}

bool LoginSessionFilterProxyModel::isDuplicate(const QModelIndex &index) const
{
	SessionListingModel *sessionListingModel =
		static_cast<SessionListingModel *>(sourceModel());
	QModelIndex primaryIndex = sessionListingModel->primaryIndexOfUrl(
		index.data(SessionListingModel::UrlRole).toUrl());
	return primaryIndex.isValid() && primaryIndex != index;
}

bool LoginSessionFilterProxyModel::alwaysShowTopLevel() const
{
	return false;
}
