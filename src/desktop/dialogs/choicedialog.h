// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_CHOICEDIALOG_H
#define DESKTOP_DIALOGS_CHOICEDIALOG_H
#include <QDialog>
#include <QIcon>
#include <QString>
#include <QVector>

class QVboxLayout;

namespace dialogs {

class ChoiceDialog final : public QDialog {
	Q_OBJECT
public:
	struct Choice {
		int id;
		QIcon icon;
		QString title;
		QString description;
	};

	explicit ChoiceDialog(
		const QString &title, const QString &description,
		const QVector<Choice> &choices, QWidget *parent = nullptr);

Q_SIGNALS:
	void choiceSelected(int id);

private:
	void selectChoice(int id);
};

}

#endif
