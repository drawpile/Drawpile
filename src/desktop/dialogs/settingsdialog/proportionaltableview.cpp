// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/proportionaltableview.h"
#include "desktop/dialogs/settingsdialog/shortcutfilterinput.h"
#include <QHeaderView>
#include <QSortFilterProxyModel>

namespace dialogs {
namespace settingsdialog {

ProportionalTableView::ProportionalTableView(QWidget *parent)
	: QTableView(parent)
{
	horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void ProportionalTableView::setColumnStretches(const Stretches &stretches)
{
	m_stretches = stretches;
	m_stretchSum = std::accumulate(m_stretches.begin(), m_stretches.end(), 0);
	update();
}

void ProportionalTableView::resizeEvent(QResizeEvent *event)
{
	QTableView::resizeEvent(event);
	const auto width = viewport()->width();
	const auto size = m_stretches.size();
	auto *header = horizontalHeader();
	auto remainder = width;
	for(auto i = 1UL; i < size; ++i) {
		const auto columnSize = width * m_stretches[i] / m_stretchSum;
		remainder -= columnSize;
		header->resizeSection(i, columnSize);
	}
	header->resizeSection(0, remainder);
}

ProportionalTableView *ProportionalTableView::make(
	ShortcutFilterInput *filter, int filterRole, QAbstractItemModel *model,
	QSortFilterProxyModel *filterModel, bool connectFilter)
{
	auto *view = new ProportionalTableView;
	view->setCornerButtonEnabled(false);
	view->setSelectionMode(QAbstractItemView::SingleSelection);
	view->setSelectionBehavior(QAbstractItemView::SelectRows);
	view->setFocusPolicy(Qt::StrongFocus);
	view->setAlternatingRowColors(true);
	view->setSortingEnabled(true);
	view->setWordWrap(false);
	view->setEditTriggers(QAbstractItemView::AllEditTriggers);

	if(filterModel) {
		filterModel->setParent(view);
	} else {
		filterModel = new QSortFilterProxyModel(view);
	}
	filterModel->setFilterRole(filterRole);
	filterModel->setSourceModel(model);
	filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	if(connectFilter) {
		QObject::connect(
			filter, &ShortcutFilterInput::filtered, filterModel,
			&QSortFilterProxyModel::setFilterFixedString);
	}
	view->setModel(filterModel);
	view->sortByColumn(0, Qt::AscendingOrder);

	auto *header = view->horizontalHeader();
	header->setSectionsMovable(false);
	header->setHighlightSections(false);
	view->verticalHeader()->hide();

	return view;
}

} // namespace settingsdialog
} // namespace dialogs
