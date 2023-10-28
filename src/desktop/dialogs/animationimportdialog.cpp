// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/canvas_state.h>
}
#include "desktop/dialogs/animationimportdialog.h"
#include "desktop/filewrangler.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/import/animationimporter.h"
#include <QApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QThreadPool>
#include <QVBoxLayout>

namespace dialogs {

AnimationImportDialog::AnimationImportDialog(QWidget *parent)
	: QDialog(parent)
{
	setModal(true);
	setWindowTitle(tr("Import Animation"));
	resize(600, 200);

	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);

	QFormLayout *form = new QFormLayout;
	layout->addLayout(form);

	QHBoxLayout *pathLayout = new QHBoxLayout;
	form->addRow(tr("File to Import:"), pathLayout);

	m_pathEdit = new QLineEdit;
	pathLayout->addWidget(m_pathEdit);

	m_chooseButton = new QPushButton(tr("Choose"));
	pathLayout->addWidget(m_chooseButton);
	connect(
		m_chooseButton, &QAbstractButton::clicked, this,
		&AnimationImportDialog::chooseFile);

	m_holdTime = new QSpinBox;
	m_holdTime->setRange(1, 99);
	m_holdTime->setValue(1);
	//: How many frames each imported key frame gets in the timeline.
	form->addRow(tr("Key frame length:"), m_holdTime);
	updateHoldTimeSuffix(m_holdTime->value());
	connect(
		m_holdTime, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&AnimationImportDialog::updateHoldTimeSuffix);

	m_framerate = new QSpinBox;
	m_framerate->setRange(1, 999);
	m_framerate->setValue(24);
	form->addRow(tr("Framerate:"), m_framerate);

	layout->addStretch();

	m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
	layout->addWidget(m_buttons);
	connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	m_importButton =
		m_buttons->addButton(tr("Import"), QDialogButtonBox::ActionRole);
	m_importButton->setIcon(QIcon::fromTheme("document-import"));
	connect(
		m_buttons, &QDialogButtonBox::clicked, this,
		&AnimationImportDialog::buttonClicked);
	connect(
		m_pathEdit, &QLineEdit::textChanged, this,
		&AnimationImportDialog::updateImportButton);
	updateImportButton(m_pathEdit->text());
}

AnimationImportDialog::~AnimationImportDialog()
{
	if(!isEnabled()) {
		QApplication::restoreOverrideCursor();
	}
}

void AnimationImportDialog::chooseFile()
{
	QString path = FileWrangler(this).getOpenOraPath();
	if(!path.isEmpty()) {
		m_pathEdit->setText(path);
	}
}

void AnimationImportDialog::updateHoldTimeSuffix(int value)
{
	m_holdTime->setSuffix(tr(" frame(s)", "", value));
}

void AnimationImportDialog::updateImportButton(const QString &path)
{
	m_importButton->setEnabled(!path.trimmed().isEmpty());
}

void AnimationImportDialog::buttonClicked(QAbstractButton *button)
{
	if(button == m_importButton) {
		runImport();
	}
}

void AnimationImportDialog::importFinished(
	const drawdance::CanvasState &canvasState, const QString &error)
{
	if(canvasState.isNull()) {
		QApplication::restoreOverrideCursor();
		QMessageBox::critical(this, tr("Animation Import Error"), error);
		setEnabled(true);
	} else {
		emit canvasStateImported(canvasState);
	}
}

void AnimationImportDialog::runImport()
{
	if(isEnabled()) {
		impex::AnimationImporter *importer = new impex::AnimationImporter(
			m_pathEdit->text().trimmed(), m_holdTime->value(),
			m_framerate->value());
		connect(
			importer, &impex::AnimationImporter::finished, this,
			&AnimationImportDialog::importFinished);
		QThreadPool::globalInstance()->start(importer);
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		setEnabled(false);
	}
}

}
