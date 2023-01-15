/*
 *  Drawpile - a collaborative drawing program.
 *
 *  Copyright (C) 2023 askmeaboutloom
 *
 *  Drawpile is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Drawpile is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "onionskins.h"
#include "main.h"
#include "titlewidget.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QToolTip>
#include <QVBoxLayout>
#include <QVector>
#include <QtColorWidgets/ColorDialog>
#include <QtColorWidgets/ColorPreview>

using color_widgets::ColorDialog;
using color_widgets::ColorPreview;

namespace docks {

namespace {

constexpr int FRAME_COUNT_MIN = 1;
constexpr int FRAME_COUNT_MAX = 30;
constexpr int FRAME_COUNT_DEFAULT = 8;
constexpr QRgb TINT_BELOW_DEFAULT = 0xffff3333u;
constexpr QRgb TINT_ABOVE_DEFAULT = 0xff3333ffu;
constexpr int DEBOUNCE_DELAY_MS = 500;

class EqualizerSlider : public QSlider {
public:
	EqualizerSlider(QWidget *parent) : QSlider{Qt::Vertical, parent}
	{
		setRange(0, 100);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	}

	virtual ~EqualizerSlider() {}

protected:
	void paintEvent(QPaintEvent *e) override
	{
		int w = width();
		int h = height();
		double val = value();
		double max = maximum();
		double ratio = val / max;
		int bar = h * ratio + 0.5;
		QPainter painter(this);
		painter.setPen(Qt::transparent);
		painter.setBrush(palette().base());
		painter.drawRect(0, 0, w, h);
		painter.setBrush(palette().highlight());
		painter.drawRect(0, h - bar, w, bar);
		e->accept();
	}

	void mousePressEvent(QMouseEvent *e) override
	{
		if(e->button() == Qt::LeftButton) {
			m_leftButtonDown = true;
			moveSliderTo(e->y());
			e->accept();
		} else {
			QSlider::mousePressEvent(e);
		}
	}

	void mouseReleaseEvent(QMouseEvent *e) override
	{
		if(e->button() == Qt::LeftButton) {
			m_leftButtonDown = false;
			e->accept();
		} else {
			QSlider::mouseReleaseEvent(e);
		}
	}

	void mouseMoveEvent(QMouseEvent *e) override
	{
		if(m_leftButtonDown) {
			moveSliderTo(e->y());
			e->accept();
		} else {
			QSlider::mouseMoveEvent(e);
		}
	}

private:
	bool m_leftButtonDown = false;

	void moveSliderTo(int y)
	{
		int h = height();
		int min = minimum();
		int max = maximum();
		setValue(min + ((max - min) * (h - y) / h));
	}
};

}

struct OnionSkinsDock::Private {
	int debounceTimerId = 0;
	int frameCount = FRAME_COUNT_MIN - 1;
	QSpinBox *frameCountSpinner = nullptr;
	ColorPreview *belowPreview = nullptr;
	ColorPreview *abovePreview = nullptr;
	QVector<QSlider *> skinsAboveSliders = {};
	QVector<QSlider *> skinsBelowSliders = {};
};

OnionSkinsDock::OnionSkinsDock(const QString &title, QWidget *parent)
	: QDockWidget{title, parent}, d{new Private}
{
	TitleWidget *titlebar = new TitleWidget{this};
	setTitleBarWidget(titlebar);

	titlebar->addStretch();
	QLabel *belowLabel = new QLabel{titlebar};
	belowLabel->setText(tr("Tint Below: "));
	titlebar->addCustomWidget(belowLabel);
	d->belowPreview = new ColorPreview{titlebar};
	d->belowPreview->setDisplayMode(ColorPreview::DisplayMode::AllAlpha);
	titlebar->addCustomWidget(d->belowPreview);
	connect(d->belowPreview, &ColorPreview::colorChanged, [this](QColor color) {
		onTintColorChange(QStringLiteral("tintbelow"), color);
	});
	connect(d->belowPreview, &ColorPreview::clicked, [this]() {
		showColorPicker(d->belowPreview->color(), [this](QColor color) {
			d->belowPreview->setColor(color);
		});
	});
	titlebar->addSpace(24);

	QLabel *frameCountLabel = new QLabel{titlebar};
	frameCountLabel->setText(tr("Frames: "));
	titlebar->addCustomWidget(frameCountLabel);
	d->frameCountSpinner = new QSpinBox{titlebar};
	d->frameCountSpinner->setRange(FRAME_COUNT_MIN, FRAME_COUNT_MAX);
	connect(
		d->frameCountSpinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&OnionSkinsDock::frameCountChanged);
	titlebar->addCustomWidget(d->frameCountSpinner);

	titlebar->addSpace(24);
	QLabel *aboveLabel = new QLabel{titlebar};
	aboveLabel->setText(tr("Tint Above: "));
	titlebar->addCustomWidget(aboveLabel);
	d->abovePreview = new ColorPreview{titlebar};
	d->abovePreview->setDisplayMode(ColorPreview::DisplayMode::AllAlpha);
	titlebar->addCustomWidget(d->abovePreview);
	connect(d->abovePreview, &ColorPreview::colorChanged, [this](QColor color) {
		onTintColorChange(QStringLiteral("tintabove"), color);
	});
	connect(d->abovePreview, &ColorPreview::clicked, [this]() {
		showColorPicker(d->abovePreview->color(), [this](QColor color) {
			d->abovePreview->setColor(color);
		});
	});
	titlebar->addStretch();

	connect(
		static_cast<DrawpileApp *>(qApp), &DrawpileApp::settingsChanged, this,
		&OnionSkinsDock::updateSettings);
	updateSettings();
}

OnionSkinsDock::~OnionSkinsDock()
{
	delete d;
}

void OnionSkinsDock::triggerUpdate()
{
	killTimer(d->debounceTimerId);
	d->debounceTimerId = 0;

	QColor tintBelow = d->belowPreview->color();
	QVector<QPair<float, QColor>> skinsBelow;
	for(QSlider *slider : d->skinsBelowSliders) {
		int opacity = slider->value();
		if(opacity != 0 || !skinsBelow.isEmpty()) {
			skinsBelow.append({opacity / 100.0f, tintBelow});
		}
	}

	QColor tintAbove = d->abovePreview->color();
	QVector<QPair<float, QColor>> skinsAbove;
	auto rend = d->skinsAboveSliders.rend();
	for(auto it = d->skinsAboveSliders.rbegin(); it != rend; ++it) {
		int opacity = (*it)->value();
		if(opacity != 0 || !skinsAbove.isEmpty()) {
			skinsAbove.prepend({opacity / 100.0f, tintAbove});
		}
	}

	emit onionSkinsChanged(skinsBelow, skinsAbove);
}

void OnionSkinsDock::timerEvent(QTimerEvent *)
{
	triggerUpdate();
}

void OnionSkinsDock::updateSettings()
{
	QSettings settings;
	settings.beginGroup("onionskins");
	d->frameCountSpinner->setValue(
		settings.value("framecount", FRAME_COUNT_DEFAULT).toInt());
	d->belowPreview->setColor(
		settings.value("tintbelow", QColor::fromRgba(TINT_BELOW_DEFAULT))
			.value<QColor>());
	d->abovePreview->setColor(
		settings.value("tintabove", QColor::fromRgba(TINT_ABOVE_DEFAULT))
			.value<QColor>());
}

void OnionSkinsDock::frameCountChanged(int value)
{
	if(value != d->frameCount) {
		QSettings settings;
		settings.beginGroup("onionskins");
		d->frameCount = value;
		settings.setValue("framecount", value);
		buildWidget(settings);
	}
}

int OnionSkinsDock::getSliderDefault(int frameNumber)
{
	switch(frameNumber) {
	case -1:
	case 1:
		return 50;
	case -2:
	case 2:
		return 20;
	case -3:
	case 3:
		return 10;
	default:
		return 0;
	}
}

void OnionSkinsDock::buildWidget(QSettings &settings)
{
	QWidget *widget = new QWidget{this};
	QHBoxLayout *slidersLayout = new QHBoxLayout{widget};
	d->skinsAboveSliders.clear();
	d->skinsBelowSliders.clear();

	for(int i = 0; i < d->frameCount * 2; ++i) {
		if(i == d->frameCount) {
			QFrame *line = new QFrame{widget};
			line->setFrameShape(QFrame::VLine);
			line->setFrameShadow(QFrame::Sunken);
			slidersLayout->addWidget(line);
		}

		int frameNumber =
			i < d->frameCount ? -(d->frameCount - i) : i - d->frameCount + 1;

		QLabel *label = new QLabel{widget};
		label->setAlignment(Qt::AlignCenter);
		if(frameNumber < 0) {
			label->setText(tr("-%1").arg(-frameNumber));
		} else {
			label->setText(tr("+%1").arg(frameNumber));
		}

		QSlider *slider = new EqualizerSlider{widget};
		QString settingsKey = QStringLiteral("frame%1").arg(frameNumber);
		slider->setValue(
			settings.value(settingsKey, getSliderDefault(frameNumber)).toInt());
		connect(slider, &QSlider::valueChanged, [=](int value) {
			onSliderValueChange(settingsKey, value);
		});

		if(frameNumber < 0) {
			d->skinsBelowSliders.append(slider);
		} else {
			d->skinsAboveSliders.append(slider);
		}

		QVBoxLayout *layout = new QVBoxLayout{};
		layout->addWidget(label);
		layout->addWidget(slider);
		slidersLayout->addLayout(layout);
	}

	setWidget(widget);
}

void OnionSkinsDock::onSliderValueChange(const QString &settingsKey, int value)
{
	QSettings settings;
	settings.beginGroup("onionskins");
	settings.setValue(settingsKey, value);
	if(d->debounceTimerId != 0) {
		killTimer(d->debounceTimerId);
	}
	d->debounceTimerId = startTimer(DEBOUNCE_DELAY_MS);
	showSliderValue(value);
}

void OnionSkinsDock::onTintColorChange(
	const QString &settingsKey, const QColor &color)
{
	QSettings settings;
	settings.beginGroup("onionskins");
	settings.setValue(settingsKey, color);
	triggerUpdate();
}

void OnionSkinsDock::showSliderValue(int value)
{
	QToolTip::showText(QCursor::pos(), tr("Opacity: %1%").arg(value), nullptr);
}

void OnionSkinsDock::showColorPicker(
	const QColor &currentColor, std::function<void(QColor)> onColorSelected)
{
	ColorDialog *dlg = new ColorDialog{this};
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setColor(currentColor);
	connect(dlg, &ColorDialog::colorSelected, onColorSelected);
	dlg->show();
}

}
