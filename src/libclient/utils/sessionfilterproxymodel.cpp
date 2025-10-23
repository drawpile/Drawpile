// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/sessionfilterproxymodel.h"
#include "libclient/net/loginsessions.h"
#include "libclient/net/sessionlistingmodel.h"
#include "libshared/util/qtcompat.h"

SessionFilterProxyModel::SessionFilterProxyModel(QObject *parent)
	: QSortFilterProxyModel(parent)
{
}

void SessionFilterProxyModel::refreshDuplicates()
{
	if(!m_showDuplicates) {
		// Before Qt 6.10, this used to just be a call to invalidateFilter(),
		// now it looks kind of weird. Hope this doesn't break.
		COMPAT_SORT_FILTER_PROXY_MODEL_BEGIN_FILTER_CHANGE();
		COMPAT_SORT_FILTER_PROXY_MODEL_END_FILTER_CHANGE();
	}
}

void SessionFilterProxyModel::setShowPassworded(bool show)
{
	if(m_showPassworded != show) {
		COMPAT_SORT_FILTER_PROXY_MODEL_BEGIN_FILTER_CHANGE();
		m_showPassworded = show;
		COMPAT_SORT_FILTER_PROXY_MODEL_END_FILTER_CHANGE();
	}
}

void SessionFilterProxyModel::setShowNsfm(bool show)
{
	if(m_showNsfm != show) {
		COMPAT_SORT_FILTER_PROXY_MODEL_BEGIN_FILTER_CHANGE();
		m_showNsfm = show;
		COMPAT_SORT_FILTER_PROXY_MODEL_END_FILTER_CHANGE();
	}
}

void SessionFilterProxyModel::setShowClosed(bool show)
{
	if(m_showClosed != show) {
		COMPAT_SORT_FILTER_PROXY_MODEL_BEGIN_FILTER_CHANGE();
		m_showClosed = show;
		COMPAT_SORT_FILTER_PROXY_MODEL_END_FILTER_CHANGE();
	}
}

void SessionFilterProxyModel::setShowInactive(bool show)
{
	if(m_showInactive != show) {
		COMPAT_SORT_FILTER_PROXY_MODEL_BEGIN_FILTER_CHANGE();
		m_showInactive = show;
		COMPAT_SORT_FILTER_PROXY_MODEL_END_FILTER_CHANGE();
	}
}

void SessionFilterProxyModel::setShowDuplicates(bool show)
{
	if(m_showDuplicates != show) {
		COMPAT_SORT_FILTER_PROXY_MODEL_BEGIN_FILTER_CHANGE();
		m_showDuplicates = show;
		COMPAT_SORT_FILTER_PROXY_MODEL_END_FILTER_CHANGE();
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
