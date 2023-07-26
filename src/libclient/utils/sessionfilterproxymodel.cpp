// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/sessionfilterproxymodel.h"
#include "libclient/net/loginsessions.h"
#include "libclient/net/sessionlistingmodel.h"

SessionFilterProxyModel::SessionFilterProxyModel(QObject *parent)
	: QSortFilterProxyModel{parent}
	, m_showPassworded{true}
	, m_showNsfw{true}
	, m_showClosed{true}
	, m_showDuplicates{true}
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

void SessionFilterProxyModel::setShowNsfw(bool show)
{
	if(m_showNsfw != show) {
		m_showNsfw = show;
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
	int nsfwRole, pwRole, closedRole;
	if(sourceModel()->inherits(
		   SessionListingModel::staticMetaObject.className())) {
		// Always show the top level items (listing servers)
		if(!sourceParent.isValid()) {
			return true;
		}
		nsfwRole = SessionListingModel::IsNsfwRole;
		pwRole = SessionListingModel::IsPasswordedRole;
		closedRole = SessionListingModel::IsClosedRole;
	} else if(sourceModel()->inherits(
				  net::LoginSessionModel::staticMetaObject.className())) {
		nsfwRole = net::LoginSessionModel::NsfmRole;
		pwRole = net::LoginSessionModel::NeedPasswordRole;
		closedRole = net::LoginSessionModel::ClosedRole;
	} else {
		qWarning("Unknown session filter source model");
		return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
	}

	const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

	if(!m_showNsfw && index.data(nsfwRole).toBool()) {
		return false;
	}

	if(!m_showPassworded && index.data(pwRole).toBool()) {
		return false;
	}

	if(!m_showClosed && index.data(closedRole).toBool()) {
		return false;
	}

	if(!m_showDuplicates && isDuplicate(index)) {
		return false;
	}

	return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

bool SessionFilterProxyModel::isDuplicate(const QModelIndex &index) const
{
	SessionListingModel *sessionListingModel =
		qobject_cast<SessionListingModel *>(sourceModel());
	if(sessionListingModel) {
		QModelIndex primaryIndex = sessionListingModel->primaryIndexOfUrl(
			index.data(SessionListingModel::UrlRole).toUrl());
		return primaryIndex.isValid() && primaryIndex != index;
	} else {
		return false;
	}
}
