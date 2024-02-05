// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SESSIONFILTERPROXYMODEL_H
#define SESSIONFILTERPROXYMODEL_H
#include <QSortFilterProxyModel>

class SessionFilterProxyModel : public QSortFilterProxyModel {
	Q_OBJECT
public:
	SessionFilterProxyModel(QObject *parent = nullptr);

	bool showNsfw() const { return m_showNsfm; }
	bool showPassworded() const { return m_showPassworded; }
	bool showClosed() const { return m_showClosed; }
	bool showDuplicates() const { return m_showDuplicates; }

	void refreshDuplicates();

public slots:
	void setShowNsfm(bool show);
	void setShowPassworded(bool show);
	void setShowClosed(bool show);
	void setShowInactive(bool show);
	void setShowDuplicates(bool show);

protected:
	bool filterAcceptsRow(
		int sourceRow, const QModelIndex &sourceParent) const override;

	virtual int nsfmRole() const = 0;
	virtual int passwordedRole() const = 0;
	virtual int closedRole() const = 0;
	virtual int inactiveRole() const = 0;

	virtual bool isDuplicate(const QModelIndex &index) const = 0;

	virtual bool alwaysShowTopLevel() const = 0;

private:
	bool m_showPassworded = true;
	bool m_showNsfm = true;
	bool m_showClosed = true;
	bool m_showInactive = true;
	bool m_showDuplicates = true;
};

class ListingSessionFilterProxyModel final : public SessionFilterProxyModel {
	Q_OBJECT
public:
	ListingSessionFilterProxyModel(QObject *parent = nullptr);

protected:
	int nsfmRole() const override;
	int passwordedRole() const override;
	int closedRole() const override;
	int inactiveRole() const override;
	bool isDuplicate(const QModelIndex &index) const override;
	bool alwaysShowTopLevel() const override;
};

class LoginSessionFilterProxyModel final : public SessionFilterProxyModel {
	Q_OBJECT
public:
	LoginSessionFilterProxyModel(QObject *parent = nullptr);

protected:
	int nsfmRole() const override;
	int passwordedRole() const override;
	int closedRole() const override;
	int inactiveRole() const override;
	bool isDuplicate(const QModelIndex &index) const override;
	bool alwaysShowTopLevel() const override;
};

#endif
