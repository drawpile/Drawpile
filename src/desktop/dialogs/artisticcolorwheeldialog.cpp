// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/artisticcolorwheeldialog.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/artisticcolorwheel.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libshared/util/paths.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace dialogs {

ArtisticColorWheelDialog::ArtisticColorWheelDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Color Circle Settings"));
	setWindowModality(Qt::WindowModal);
	resize(350, 600);

	QVBoxLayout *layout = new QVBoxLayout(this);
	const desktop::settings::Settings &settings = dpApp().settings();

	QFormLayout *form = new QFormLayout;
	layout->addLayout(form);

	bool hueLimit = settings.colorCircleHueLimit();
	m_continuousHueBox = new QCheckBox;
	m_continuousHueBox->setChecked(!hueLimit);
	form->addRow(m_continuousHueBox);
	connect(
		m_continuousHueBox, COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox),
		this, &ArtisticColorWheelDialog::updateContinuousHue);

	int hueCount = settings.colorCircleHueCount();
	m_hueStepsSlider = new KisSliderSpinBox;
	m_hueStepsSlider->setRange(
		widgets::ArtisticColorWheel::HUE_COUNT_MIN,
		widgets::ArtisticColorWheel::HUE_COUNT_MAX);
	m_hueStepsSlider->setValue(hueCount);
	m_hueStepsSlider->setEnabled(hueLimit);
	form->addRow(m_hueStepsSlider);
	connect(
		m_hueStepsSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &ArtisticColorWheelDialog::updateHueSteps);

	int hueAngle = settings.colorCircleHueAngle();
	m_hueAngleSlider = new KisSliderSpinBox;
	m_hueAngleSlider->setRange(0, 180);
	m_hueAngleSlider->setValue(hueAngle);
	m_hueAngleSlider->setEnabled(hueLimit);
	form->addRow(m_hueAngleSlider);
	connect(
		m_hueAngleSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &ArtisticColorWheelDialog::updateHueAngle);

	utils::addFormSpacer(form);

	bool saturationLimit = settings.colorCircleSaturationLimit();
	m_continuousSaturationBox = new QCheckBox;
	m_continuousSaturationBox->setChecked(!saturationLimit);
	form->addRow(m_continuousSaturationBox);
	connect(
		m_continuousSaturationBox,
		COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox), this,
		&ArtisticColorWheelDialog::updateContinuousSaturation);

	int saturationCount = settings.colorCircleSaturationCount();
	m_saturationStepsSlider = new KisSliderSpinBox;
	m_saturationStepsSlider->setRange(
		widgets::ArtisticColorWheel::SATURATION_COUNT_MIN,
		widgets::ArtisticColorWheel::SATURATION_COUNT_MAX);
	m_saturationStepsSlider->setValue(saturationCount);
	m_saturationStepsSlider->setEnabled(saturationLimit);
	form->addRow(m_saturationStepsSlider);
	connect(
		m_saturationStepsSlider,
		QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&ArtisticColorWheelDialog::updateSaturationSteps);

	utils::addFormSpacer(form);

	bool valueLimit = settings.colorCircleValueLimit();
	m_continuousValueBox = new QCheckBox;
	m_continuousValueBox->setChecked(!valueLimit);
	form->addRow(m_continuousValueBox);
	connect(
		m_continuousValueBox, COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox),
		this, &ArtisticColorWheelDialog::updateContinuousValue);

	int valueCount = settings.colorCircleValueCount();
	m_valueStepsSlider = new KisSliderSpinBox;
	m_valueStepsSlider->setRange(
		widgets::ArtisticColorWheel::VALUE_COUNT_MIN,
		widgets::ArtisticColorWheel::VALUE_COUNT_MAX);
	m_valueStepsSlider->setValue(valueCount);
	m_valueStepsSlider->setEnabled(valueLimit);
	form->addRow(m_valueStepsSlider);
	connect(
		m_valueStepsSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &ArtisticColorWheelDialog::updateValueSteps);

	m_continuousHueBox->setText(tr("Continuous hue"));
	m_hueStepsSlider->setPrefix(tr("Hue steps: "));
	m_hueAngleSlider->setPrefix(tr("Hue angle: "));
	//: Degree symbol. Unless your language uses a different one, keep as-is.
	m_hueAngleSlider->setSuffix(tr("°"));
	ColorSpace colorSpace = settings.colorWheelSpace();
	if(colorSpace == ColorSpace::ColorLCH) {
		m_continuousSaturationBox->setText(tr("Continuous chroma"));
		m_saturationStepsSlider->setPrefix(tr("Chroma steps: "));
		m_continuousValueBox->setText(tr("Continuous luminance"));
		m_saturationStepsSlider->setPrefix(tr("Luminance steps: "));
	} else {
		m_continuousSaturationBox->setText(tr("Continuous saturation"));
		m_saturationStepsSlider->setPrefix(tr("Saturation steps: "));
		if(colorSpace == ColorSpace::ColorHSL) {
			m_continuousValueBox->setText(tr("Continuous lightness"));
			m_valueStepsSlider->setPrefix(tr("Lightness steps: "));
		} else {
			m_continuousValueBox->setText(tr("Continuous value"));
			m_valueStepsSlider->setPrefix(tr("Value steps: "));
		}
	}

	utils::addFormSpacer(form);

	QString gamutMaskPath = settings.colorCircleGamutMaskPath();
	m_gamutMaskCombo = new QComboBox;
	m_gamutMaskCombo->addItem(tr("None"));
	loadGamutMasks();
	if(!gamutMaskPath.isEmpty()) {
		int count = m_gamutMaskCombo->count();
		for(int i = 1; i < count; ++i) {
			if(m_gamutMaskCombo->itemData(i).toString() == gamutMaskPath) {
				m_gamutMaskCombo->setCurrentIndex(i);
				break;
			}
		}
	}
	form->addRow(tr("Gamut mask:"), m_gamutMaskCombo);
	connect(
		m_gamutMaskCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &ArtisticColorWheelDialog::updateGamutMask);

	int gamutMaskAngle = settings.colorCircleGamutMaskAngle();
	m_gamutMaskAngleSlider = new KisSliderSpinBox;
	m_gamutMaskAngleSlider->setPrefix(tr("Mask angle: "));
	//: Degree symbol. Unless your language uses a different one, keep as-is.
	m_gamutMaskAngleSlider->setSuffix(tr("°"));
	m_gamutMaskAngleSlider->setRange(0, 359);
	m_gamutMaskAngleSlider->setValue(gamutMaskAngle);
	m_gamutMaskAngleSlider->setEnabled(
		!m_gamutMaskCombo->currentData().toString().isEmpty());
	form->addRow(m_gamutMaskAngleSlider);
	connect(
		m_gamutMaskAngleSlider,
		QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&ArtisticColorWheelDialog::updateGamutMaskAngle);

	qreal gamutMaskOpacity = settings.colorCircleGamutMaskOpacity();
	m_gamutMaskOpacitySlider = new KisSliderSpinBox;
	m_gamutMaskOpacitySlider->setPrefix(tr("Mask opacity: "));
	m_gamutMaskOpacitySlider->setSuffix(tr("%"));
	m_gamutMaskOpacitySlider->setRange(10, 100);
	m_gamutMaskOpacitySlider->setValue(qRound(gamutMaskOpacity * 100.0));
	m_gamutMaskOpacitySlider->setEnabled(
		!m_gamutMaskCombo->currentData().toString().isEmpty());
	form->addRow(m_gamutMaskOpacitySlider);
	connect(
		m_gamutMaskOpacitySlider,
		QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&ArtisticColorWheelDialog::updateGamutMaskOpacity);

	utils::addFormSpacer(form);

	m_wheel = new widgets::ArtisticColorWheel;
	m_wheel->setHueLimit(hueLimit);
	m_wheel->setHueCount(hueCount);
	m_wheel->setHueAngle(hueAngle);
	m_wheel->setSaturationLimit(saturationLimit);
	m_wheel->setSaturationCount(saturationCount);
	m_wheel->setValueLimit(valueLimit);
	m_wheel->setValueCount(valueCount);
	m_wheel->setGamutMaskPath(gamutMaskPath);
	m_wheel->setGamutMaskAngle(gamutMaskAngle);
	m_wheel->setGamutMaskOpacity(gamutMaskOpacity);
	m_wheel->setColorSpace(colorSpace);
	m_wheel->setColor(Qt::red);
	m_wheel->setMinimumSize(64, 64);
	layout->addWidget(m_wheel, 1);

	QDialogButtonBox *buttons = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Apply |
		QDialogButtonBox::Cancel);
	layout->addWidget(buttons);
	connect(
		buttons, &QDialogButtonBox::accepted, this,
		&ArtisticColorWheelDialog::accept);
	connect(
		buttons, &QDialogButtonBox::rejected, this,
		&ArtisticColorWheelDialog::reject);
	connect(
		buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
		&ArtisticColorWheelDialog::saveSettings);

	connect(
		this, &ArtisticColorWheelDialog::accepted, this,
		&ArtisticColorWheelDialog::saveSettings);
}

void ArtisticColorWheelDialog::loadGamutMasks()
{
	QHash<QString, QString> defaultGamutMaskTitles = {
		{
			QStringLiteral("Atmosphere with accent"),
			//: This is the name for a gamut mask, a shape on a color circle.
			tr("Atmosphere with accent"),
		},
		{
			QStringLiteral("Atmospheric triad"),
			//: This is the name for a gamut mask, a shape on a color circle.
			tr("Atmospheric triad"),
		},
		{
			QStringLiteral("Complementary"),
			//: This is the name for a gamut mask, a shape on a color circle.
			tr("Complementary"),
		},
		{
			QStringLiteral("Dominant hue with accent"),
			//: This is the name for a gamut mask, a shape on a color circle.
			tr("Dominant hue with accent"),
		},
		{
			QStringLiteral("Shifted triad"),
			//: This is the name for a gamut mask, a shape on a color circle.
			tr("Shifted triad"),
		},
		{
			QStringLiteral("Split"),
			//: This is the name for a gamut mask, a shape on a color circle.
			tr("Split"),
		},
		{
			QStringLiteral("Split complementary"),
			//: This is the name for a gamut mask, a shape on a color circle.
			tr("Split complementary"),
		},
		{
			QStringLiteral("Tetradic"),
			//: This is the name for a gamut mask, a shape on a color circle.
			tr("Tetradic"),
		},
	};

	QStringList datapaths = utils::paths::dataPaths();
	QVector<QPair<QString, QString>> entries;
	for(const QString &path : datapaths) {
		QDir dir(path + QStringLiteral("/gamutmasks/"));
		for(const QString &p :
			dir.entryList({QStringLiteral("*.svg")}, QDir::Files)) {
			QFile f(dir.filePath(p));
			if(!f.open(QIODevice::ReadOnly)) {
				qWarning(
					"Error opening gamut mask '%s': %s",
					qUtf8Printable(f.fileName()),
					qUtf8Printable(f.errorString()));
				continue;
			}

			QDomDocument doc;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			QDomDocument::ParseResult parseResult = doc.setContent(&f);
			bool parseOk = bool(parseResult);
			QString parseError = parseResult.errorMessage;
#else
			QString parseError;
			bool parseOk = doc.setContent(&f, &parseError);
#endif
			f.close();
			if(!parseOk) {
				qWarning(
					"Error parsing gamut mask '%s': %s",
					qUtf8Printable(f.fileName()), qUtf8Printable(parseError));
				continue;
			}

			QString title;
			QDomElement titleElement = doc.documentElement().firstChildElement(
				QStringLiteral("title"));
			if(!titleElement.isNull()) {
				title = titleElement.text().trimmed();
			}

			if(title.isEmpty()) {
				title = QFileInfo(f.fileName()).baseName();
			}

			entries.append(
				{defaultGamutMaskTitles.value(title, title), f.fileName()});
		}
	}

	std::sort(
		entries.begin(), entries.end(),
		[](const QPair<QString, QString> &a, const QPair<QString, QString> &b) {
			return a.first.compare(b.first, Qt::CaseInsensitive) < 0;
		});
	for(const QPair<QString, QString> &entry : entries) {
		m_gamutMaskCombo->addItem(entry.first, entry.second);
	}
}

void ArtisticColorWheelDialog::updateContinuousHue(compat::CheckBoxState state)
{
	bool limit = state == Qt::Unchecked;
	m_hueStepsSlider->setEnabled(limit);
	m_hueAngleSlider->setEnabled(limit);
	m_wheel->setHueLimit(limit);
}

void ArtisticColorWheelDialog::updateHueSteps(int steps)
{
	m_wheel->setHueCount(steps);
}

void ArtisticColorWheelDialog::updateHueAngle(int angle)
{
	m_wheel->setHueAngle(angle);
}

void ArtisticColorWheelDialog::updateContinuousSaturation(
	compat::CheckBoxState state)
{
	bool limit = state == Qt::Unchecked;
	m_saturationStepsSlider->setEnabled(limit);
	m_wheel->setSaturationLimit(limit);
}

void ArtisticColorWheelDialog::updateSaturationSteps(int steps)
{
	m_wheel->setSaturationCount(steps);
}

void ArtisticColorWheelDialog::updateContinuousValue(
	compat::CheckBoxState state)
{
	bool limit = state == Qt::Unchecked;
	m_valueStepsSlider->setEnabled(limit);
	m_wheel->setValueLimit(limit);
}

void ArtisticColorWheelDialog::updateValueSteps(int steps)
{
	m_wheel->setValueCount(steps);
}

void ArtisticColorWheelDialog::updateGamutMask(int index)
{
	QString path = m_gamutMaskCombo->itemData(index).toString();
	m_wheel->setGamutMaskPath(path);
	bool enabled = m_wheel->hasGamutMask();
	m_gamutMaskAngleSlider->setEnabled(enabled);
	m_gamutMaskOpacitySlider->setEnabled(enabled);
}

void ArtisticColorWheelDialog::updateGamutMaskAngle(int angle)
{
	m_wheel->setGamutMaskAngle(angle);
}

void ArtisticColorWheelDialog::updateGamutMaskOpacity(int opacity)
{
	m_wheel->setGamutMaskOpacity(opacity / 100.0);
}

void ArtisticColorWheelDialog::saveSettings()
{
	desktop::settings::Settings &settings = dpApp().settings();
	settings.setColorCircleHueLimit(!m_continuousHueBox->isChecked());
	settings.setColorCircleHueCount(m_hueStepsSlider->value());
	settings.setColorCircleHueAngle(m_hueAngleSlider->value());
	settings.setColorCircleSaturationLimit(
		!m_continuousSaturationBox->isChecked());
	settings.setColorCircleSaturationCount(m_saturationStepsSlider->value());
	settings.setColorCircleValueLimit(!m_continuousValueBox->isChecked());
	settings.setColorCircleValueCount(m_valueStepsSlider->value());
	settings.setColorCircleGamutMaskPath(
		m_gamutMaskCombo->currentData().toString());
	settings.setColorCircleGamutMaskAngle(m_gamutMaskAngleSlider->value());
	settings.setColorCircleGamutMaskOpacity(
		m_gamutMaskOpacitySlider->value() / 100.0);
}

}
