// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_PROPORTIONALTABLEVIEW_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_PROPORTIONALTABLEVIEW_H

#include <QTableView>
#include <array>

class QAbstractItemModel;
class QLineEdit;

namespace dialogs {
namespace settingsdialog {

class ProportionalTableView final : public QTableView {
public:
	using Stretches = std::array<int, 4>;

	static ProportionalTableView *
	make(QLineEdit *filter, QAbstractItemModel *model);

	ProportionalTableView(QWidget *parent = nullptr);

	const Stretches &columnStretches() const { return m_stretches; }
	void setColumnStretches(const Stretches &stretches);

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	Stretches m_stretches = {1, 1, 1, 1};
	int m_stretchSum = 4;
};

} // namespace settingsdialog
} // namespace dialogs

#endif
