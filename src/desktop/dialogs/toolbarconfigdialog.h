// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_TOOLBARCONFIGDIALOG_H
#define DESKTOP_DIALOGS_TOOLBARCONFIGDIALOG_H
#include <QAction>
#include <QDialog>
#include <QList>
#include <QVariantHash>
#include <functional>

class QListWidget;

namespace dialogs {

class ToolBarConfigDialog final : public QDialog {
	Q_OBJECT
public:
	using ReadConfigFn = std::function<void(QAction *action, bool hidden)>;

	explicit ToolBarConfigDialog(
		const QVariantHash &cfg, const QList<QAction *> &drawActions,
		QWidget *parent = nullptr);

	void updateConfig(QVariantHash &cfg) const;

	static void readConfig(
		const QVariantHash &cfg, const QString &toolBarName,
		QList<QAction *> actions, const ReadConfigFn &fn);

private:
	void loadDrawActions(const QVariantHash &cfg);
	void resetDrawActions();
	void addDrawAction(QAction *action, bool hidden);

	const QList<QAction *> m_drawActions;
	QListWidget *m_drawList;
};

}


#endif
