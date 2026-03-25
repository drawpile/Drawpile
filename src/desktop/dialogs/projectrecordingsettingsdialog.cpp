// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/projectrecordingsettingsdialog.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libshared/util/paths.h"
#include <QDialogButtonBox>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>
#include <cmath>

namespace dialogs {

ProjectRecordingSettingsDialog::ProjectRecordingSettingsDialog(
	size_t lastReportedSizeInBytes, size_t sizeLimitInBytes, QWidget *parent)
	: QDialog(parent)
{
	utils::makeModal(this);
	setWindowTitle(tr("Manage Autorecovery"));
	resize(400, 200);

	QVBoxLayout *dlgLayout = new QVBoxLayout;
	setLayout(dlgLayout);

	double lastReportedSizeInGiB =
		double(lastReportedSizeInBytes) / 1024.0 / 1024.0 / 1024.0;
	double minimumInGiB =
		qMax(0.5, std::floor((lastReportedSizeInGiB * 10.0) + 1.0) / 10.0);

	m_sizeLimitSlider = new KisDoubleSliderSpinBox;
	initSizeLimitSlider(m_sizeLimitSlider, minimumInGiB);
	if(sizeLimitInBytes == 0) {
		m_sizeLimitSlider->setValue(m_sizeLimitSlider->maximum());
	} else {
		double sizeLimitInGiB =
			double(sizeLimitInBytes) / 1024.0 / 1024.0 / 1024.0;
		m_sizeLimitSlider->setValue(sizeLimitInGiB);
	}
	dlgLayout->addWidget(m_sizeLimitSlider);

	QLabel *explanationLabel = new QLabel;
	explanationLabel->setWordWrap(true);
	explanationLabel->setTextFormat(Qt::RichText);
	dlgLayout->addWidget(explanationLabel);

	QString currentSizeText =
		//: %1 is a file size, like "1 GB".
		tr("The current autorecovery file size is %1.")
			.arg(utils::paths::formatFileSize(lastReportedSizeInBytes));

	QString limitSizeText;
	if(sizeLimitInBytes == 0) {
		limitSizeText = tr(" There is no size limit set.");
	} else {
		int percent = qRound(
			double(lastReportedSizeInGiB) / double(sizeLimitInBytes) * 100.0);
		//: The %1% becomes a percentage, like "50%". Don't remove the second %!
		//: %2 is a file size, like "5GB".
		limitSizeText = tr(" This is %1% of the current %2 limit.")
							.arg(
								QString::number(percent),
								utils::paths::formatFileSize(sizeLimitInBytes));
	}
	explanationLabel->setText(QStringLiteral("<p>%1%2</p>")
								  .arg(
									  currentSizeText.toHtmlEscaped(),
									  limitSizeText.toHtmlEscaped()));

	QLabel *disableLabel = new QLabel;
	disableLabel->setWordWrap(true);
	disableLabel->setTextFormat(Qt::RichText);
	dlgLayout->addWidget(disableLabel);

	QString disableText =
		//: The stuff in [] will turn into a link. Don't remove the [] or
		//: replace them with different symbols!
		tr("You can also [disable autorecovery for this session].");
	disableLabel->setText(
		utils::toHtmlWithLink(disableText, QStringLiteral("#")));

	connect(
		disableLabel, &QLabel::linkActivated, this,
		&ProjectRecordingSettingsDialog::stopRequested);

	dlgLayout->addStretch();

	QDialogButtonBox *buttons =
		new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	dlgLayout->addWidget(buttons);
	connect(
		buttons, &QDialogButtonBox::accepted, this,
		&ProjectRecordingSettingsDialog::accept);
	connect(
		buttons, &QDialogButtonBox::rejected, this,
		&ProjectRecordingSettingsDialog::reject);
}

void ProjectRecordingSettingsDialog::accept()
{
	size_t sizeLimitInBytes;
	double value = m_sizeLimitSlider->value();
	if(value < m_sizeLimitSlider->maximum()) {
		sizeLimitInBytes = size_t(value * 1024.0 * 1024.0 * 1024.0);
	} else {
		sizeLimitInBytes = 0;
	}
	Q_EMIT setSizeLimitInBytesRequested(sizeLimitInBytes);
	QDialog::accept();
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

}
