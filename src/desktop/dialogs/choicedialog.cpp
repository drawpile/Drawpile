// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/choicedialog.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/commandlinkbutton.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>
#include <functional>

namespace dialogs {

ChoiceDialog::ChoiceDialog(
	const QString &title, const QString &description,
	const QVector<Choice> &choices, QWidget *parent)
	: QDialog(parent)
{
	utils::makeModal(this);
	setWindowTitle(title);

	QVBoxLayout *dlgLayout = new QVBoxLayout(this);

	if(!description.isEmpty()) {
		QLabel *descriptionLabel = new QLabel(description);
		descriptionLabel->setWordWrap(true);
		dlgLayout->addWidget(descriptionLabel);
	}

	for(const Choice &choice : choices) {
		widgets::CommandLinkButton *choiceButton =
			new widgets::CommandLinkButton(
				choice.icon, choice.title, choice.description);
		dlgLayout->addWidget(choiceButton);
		connect(
			choiceButton, &widgets::CommandLinkButton::clicked, this,
			std::bind(&ChoiceDialog::selectChoice, this, choice.id));
	}

	dlgLayout->addStretch();

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
	dlgLayout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, this, &ChoiceDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &ChoiceDialog::reject);

	dlgLayout->activate();
	resize(400, dlgLayout->minimumHeightForWidth(400));
}

void ChoiceDialog::selectChoice(int id)
{
	Q_EMIT choiceSelected(id);
	accept();
}

}
