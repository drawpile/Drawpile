// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/projectrecordingsettingsdialog.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libshared/util/paths.h"
#include <QAction>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <cmath>

namespace dialogs {

ProjectRecordingSettingsDialog::ProjectRecordingSettingsDialog(
	QAction *autoRecordAction, bool settingsOpen, QWidget *parent)
	: QDialog(parent)
	, m_autoRecordAction(autoRecordAction)
{
	utils::makeModal(this);
	setWindowTitle(tr("Manage Autorecovery"));
	resize(400, 400);

	QVBoxLayout *dlgLayout = new QVBoxLayout;
	setLayout(dlgLayout);

	if(!settingsOpen) {
		QFrame *settingsFrame = new QFrame;
		settingsFrame->setFrameShape(QFrame::StyledPanel);
		settingsFrame->setFrameShadow(QFrame::Sunken);
		dlgLayout->addWidget(settingsFrame);

		QHBoxLayout *settingsLayout = new QHBoxLayout(settingsFrame);

		settingsLayout->addWidget(
			utils::makeIconLabel(
				QIcon::fromTheme(QStringLiteral("backup")), settingsFrame));

		QLabel *settingsLabel = new QLabel(
			utils::toHtmlWithLink(
				//: The stuff in [] will turn into a link. Don't remove the []
				//: or replace them with different symbols!
				tr("These settings affect only the current session. You can "
				   "change the defaults [in the preferences]."),
				QStringLiteral("#")));
		settingsLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		settingsLabel->setTextFormat(Qt::RichText);
		settingsLabel->setWordWrap(true);
		settingsLayout->addWidget(settingsLabel, 1);
		connect(
			settingsLabel, &QLabel::linkActivated, this,
			&ProjectRecordingSettingsDialog::preferencesRequested);

		utils::addFormSpacer(dlgLayout);
	}

	m_enableCheckBox =
		new QCheckBox(tr("Enable autorecovery for the current session"));
	dlgLayout->addWidget(m_enableCheckBox);
	connect(
		m_enableCheckBox, &QCheckBox::clicked, autoRecordAction,
		&QAction::trigger);
	connect(
		autoRecordAction, &QAction::changed, this,
		&ProjectRecordingSettingsDialog::updateFromAutoRecordAction);

	utils::addFormSpacer(dlgLayout);

	m_stack = new QStackedWidget;
	dlgLayout->addWidget(m_stack, 1);

	m_disabledPage = new QWidget;
	m_disabledPage->setContentsMargins(0, 0, 0, 0);
	m_stack->addWidget(m_disabledPage);
	// Nothing on the disabled page currently.

	m_enabledPage = new QWidget;
	m_enabledPage->setContentsMargins(0, 0, 0, 0);
	m_stack->addWidget(m_enabledPage);

	QVBoxLayout *enabledLayout = new QVBoxLayout(m_enabledPage);
	enabledLayout->setContentsMargins(0, 0, 0, 0);

	QGroupBox *sizeLimitBox = new QGroupBox;
	sizeLimitBox->setTitle(tr("Size limit"));
	enabledLayout->addWidget(sizeLimitBox);

	QVBoxLayout *sizeLimitLayout = new QVBoxLayout;
	sizeLimitBox->setLayout(sizeLimitLayout);

	m_sizeLimitLabel = new QLabel;
	m_sizeLimitLabel->setWordWrap(true);
	m_sizeLimitLabel->setTextFormat(Qt::RichText);
	sizeLimitLayout->addWidget(m_sizeLimitLabel);

	QPushButton *sizeLimitButton = new QPushButton(tr("Change Size Limit"));
	sizeLimitLayout->addWidget(sizeLimitButton);
	connect(
		sizeLimitButton, &QPushButton::clicked, this,
		&ProjectRecordingSettingsDialog::showOrRaiseSizeLimitChangeDialog);

	enabledLayout->addStretch();

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
	dlgLayout->addWidget(buttons);
	connect(
		buttons, &QDialogButtonBox::accepted, this,
		&ProjectRecordingSettingsDialog::accept);
	connect(
		buttons, &QDialogButtonBox::rejected, this,
		&ProjectRecordingSettingsDialog::reject);

	updateSizeLimitLabelText();
	updateFromAutoRecordAction();
}

void ProjectRecordingSettingsDialog::updateSize(
	size_t lastReportedSizeInBytes, size_t sizeLimitInBytes)
{
	if(lastReportedSizeInBytes != m_lastReportedSizeInBytes ||
	   sizeLimitInBytes != m_sizeLimitInBytes) {
		m_lastReportedSizeInBytes = lastReportedSizeInBytes;
		m_sizeLimitInBytes = sizeLimitInBytes;
		updateSizeLimitLabelText();
	}
}

void ProjectRecordingSettingsDialog::initSizeLimitSlider(
	KisDoubleSliderSpinBox *slider, double minimumInGiB)
{
	slider->setRange(minimumInGiB, 100.0, 1);
	slider->setExponentRatio(3.0);
	utils::encapsulateDoubleSpinBoxPrefixSuffix(
		slider, tr("Size limit: %1 GB"));
	connect(
		slider, QOverload<double>::of(&KisDoubleSliderSpinBox::valueChanged),
		slider, [slider](double gib) {
			if(gib < slider->maximum()) {
				slider->setOverrideText(QString());
			} else {
				slider->setOverrideText(tr("No size limit"));
			}
		});
}

QString ProjectRecordingSettingsDialog::getAutorecordNoteText()
{
	//: The %1% is a percentage, like 75%. Don't remove the second %!
	return tr("You will receive a warning when the autorecovery file reaches "
			  "%1% of the size limit. Once it exceeds the limit, autorecovery "
			  "will terminate.")
		.arg(75);
}

QString ProjectRecordingSettingsDialog::getLimitText(
	size_t lastReportedSizeInBytes, size_t sizeLimitInBytes)
{
	if(sizeLimitInBytes == 0) {
		return tr(" There is no size limit set.");
	} else {
		int percent = qRound(
			bytesInGiB(lastReportedSizeInBytes) / double(sizeLimitInBytes) *
			100.0);
		//: The %1% becomes a percentage, like "50%". Don't remove the second %!
		//: %2 is a file size, like "5GB".
		return tr(" This is %1% of the current %2 limit.")
			.arg(
				QString::number(percent),
				utils::paths::formatFileSize(sizeLimitInBytes));
	}
}

void ProjectRecordingSettingsDialog::updateSizeLimitLabelText()
{
	QString currentSizeText =
		//: %1 is a file size, like "1 GB".
		tr("The current autorecovery file size is %1.")
			.arg(utils::paths::formatFileSize(m_lastReportedSizeInBytes));

	QString limitSizeText =
		getLimitText(m_lastReportedSizeInBytes, m_sizeLimitInBytes);
	m_sizeLimitLabel->setText(QStringLiteral("<p>%1%2</p>")
								  .arg(
									  currentSizeText.toHtmlEscaped(),
									  limitSizeText.toHtmlEscaped()));
}

void ProjectRecordingSettingsDialog::updateFromAutoRecordAction()
{
	bool checked = m_autoRecordAction->isChecked();
	m_enableCheckBox->setEnabled(m_autoRecordAction->isEnabled());
	m_enableCheckBox->setChecked(checked);
	m_stack->setCurrentWidget(checked ? m_enabledPage : m_disabledPage);
	if(!checked && m_sizeLimitChangeDialog) {
		m_sizeLimitChangeDialog->close();
		m_sizeLimitChangeDialog.clear();
	}
}

void ProjectRecordingSettingsDialog::showOrRaiseSizeLimitChangeDialog()
{
	if(m_sizeLimitChangeDialog) {
		m_sizeLimitChangeDialog->activateWindow();
		m_sizeLimitChangeDialog->raise();
	} else {
		QDialog *dlg = new QDialog(this);
		dlg->setAttribute(Qt::WA_DeleteOnClose);
		dlg->setWindowTitle(tr("Change Size Limit"));
		dlg->resize(300, 0);

		QVBoxLayout *dlgLayout = new QVBoxLayout(dlg);

		double minimumInGiB = qMax(
			0.5, std::floor((lastReportedSizeInGiB() * 10.0) + 1.0) / 10.0);

		KisDoubleSliderSpinBox *sizeLimitSlider = new KisDoubleSliderSpinBox;
		initSizeLimitSlider(sizeLimitSlider, minimumInGiB);
		if(m_sizeLimitInBytes == 0) {
			sizeLimitSlider->setValue(sizeLimitSlider->maximum());
		} else {
			sizeLimitSlider->setValue(sizeLimitInGiB());
		}
		dlgLayout->addWidget(sizeLimitSlider);

		QDialogButtonBox *buttons = new QDialogButtonBox(
			QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		dlgLayout->addWidget(buttons);

		connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
		connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
		connect(dlg, &QDialog::accepted, this, [this, sizeLimitSlider] {
			size_t sizeLimitInBytes;
			double value = sizeLimitSlider->value();
			if(value < sizeLimitSlider->maximum()) {
				sizeLimitInBytes = size_t(value * 1024.0 * 1024.0 * 1024.0);
			} else {
				sizeLimitInBytes = 0;
			}
			Q_EMIT setSizeLimitInBytesRequested(sizeLimitInBytes);
		});

		dlg->show();
		m_sizeLimitChangeDialog = dlg;
	}
}

}
