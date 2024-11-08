// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/onionskins.h"
#include "desktop/dialogs/colordialog.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/main.h"
#include "desktop/utils/qtguicompat.h"
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QToolTip>
#include <QVBoxLayout>
#include <QVector>
#include <QtColorWidgets/ColorPreview>

using color_widgets::ColorDialog;
using color_widgets::ColorPreview;

namespace docks {

namespace {

constexpr int FRAME_COUNT_MIN = 1;
constexpr int FRAME_COUNT_MAX = 30;

class EqualizerSlider final : public QSlider {
public:
	EqualizerSlider(QWidget *parent)
		: QSlider{Qt::Vertical, parent}
	{
		setRange(0, 100);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	}

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
			moveSliderTo(compat::mousePos(*e).y());
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
			moveSliderTo(compat::mousePos(*e).y());
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
	QCheckBox *wrapCheckBox = nullptr;
	ColorPreview *belowPreview = nullptr;
	ColorPreview *abovePreview = nullptr;
	QVector<QSlider *> skinsAboveSliders = {};
	QVector<QSlider *> skinsBelowSliders = {};
};

OnionSkinsDock::OnionSkinsDock(QWidget *parent)
	: DockBase(tr("Onion Skins"), QString(), parent)
	, d{new Private}
{
	TitleWidget *titlebar = new TitleWidget{this};
	setTitleBarWidget(titlebar);

	titlebar->addStretch();
	d->belowPreview = new ColorPreview{titlebar};
	d->belowPreview->setDisplayMode(ColorPreview::DisplayMode::AllAlpha);
	d->belowPreview->setMinimumWidth(24);
	d->belowPreview->setToolTip(tr("Tint Below"));
	titlebar->addCustomWidget(d->belowPreview);
	connect(d->belowPreview, &ColorPreview::clicked, [this]() {
		showColorPicker(d->belowPreview->color(), [this](QColor color) {
			d->belowPreview->setColor(color);
		});
	});
	titlebar->addStretch();

	QLabel *frameCountLabel = new QLabel{titlebar};
	frameCountLabel->setText(tr("Frames: "));
	titlebar->addCustomWidget(frameCountLabel);
	d->frameCountSpinner = new QSpinBox{titlebar};
	d->frameCountSpinner->setRange(FRAME_COUNT_MIN, FRAME_COUNT_MAX);
	titlebar->addCustomWidget(d->frameCountSpinner);
	titlebar->addStretch();
	d->wrapCheckBox = new QCheckBox(titlebar);
	d->wrapCheckBox->setText(tr("Wrap"));
	titlebar->addCustomWidget(d->wrapCheckBox);

	titlebar->addStretch();
	d->abovePreview = new ColorPreview{titlebar};
	d->abovePreview->setDisplayMode(ColorPreview::DisplayMode::AllAlpha);
	d->abovePreview->setMinimumWidth(24);
	d->abovePreview->setToolTip(tr("Tint Above"));
	titlebar->addCustomWidget(d->abovePreview);
	connect(d->abovePreview, &ColorPreview::clicked, [this]() {
		showColorPicker(d->abovePreview->color(), [this](QColor color) {
			d->abovePreview->setColor(color);
		});
	});
	titlebar->addStretch();

	auto &settings = dpApp().settings();
	settings.bindOnionSkinsWrap(d->wrapCheckBox);
	settings.bindOnionSkinsWrap(this, &OnionSkinsDock::triggerUpdate);

	settings.bindOnionSkinsFrameCount(d->frameCountSpinner);
	settings.bindOnionSkinsFrameCount(this, &OnionSkinsDock::frameCountChanged);

	settings.bindOnionSkinsTintBelow(
		d->belowPreview, &ColorPreview::setColor, &ColorPreview::colorChanged);
	settings.bindOnionSkinsTintBelow(this, &OnionSkinsDock::triggerUpdate);

	settings.bindOnionSkinsTintAbove(
		d->abovePreview, &ColorPreview::setColor, &ColorPreview::colorChanged);
	settings.bindOnionSkinsTintAbove(this, &OnionSkinsDock::triggerUpdate);
}

OnionSkinsDock::~OnionSkinsDock()
{
	delete d;
}

void OnionSkinsDock::triggerUpdate()
{
	killTimer(d->debounceTimerId);
	d->debounceTimerId = 0;

	bool wrap = d->wrapCheckBox->isChecked();

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

	emit onionSkinsChanged(wrap, skinsBelow, skinsAbove);
}

void OnionSkinsDock::timerEvent(QTimerEvent *)
{
	triggerUpdate();
}

void OnionSkinsDock::frameCountChanged(int value)
{
	d->frameCount = value;
	buildWidget();
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

void OnionSkinsDock::buildWidget()
{
	QWidget *widget = new QWidget{this};
	QHBoxLayout *slidersLayout = new QHBoxLayout{widget};
	d->skinsAboveSliders.clear();
	d->skinsBelowSliders.clear();

	auto savedFrames = dpApp().settings().onionSkinsFrames();
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

		slider->setValue(
			savedFrames.value(frameNumber, getSliderDefault(frameNumber)));
		connect(slider, &QSlider::valueChanged, [=](int value) {
			onSliderValueChange(frameNumber, value);
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

void OnionSkinsDock::onSliderValueChange(int frameNumber, int value)
{
	auto &settings = dpApp().settings();
	auto savedFrames = settings.onionSkinsFrames();
	savedFrames[frameNumber] = value;
	settings.setOnionSkinsFrames(savedFrames);

	if(d->debounceTimerId != 0) {
		killTimer(d->debounceTimerId);
	}
	d->debounceTimerId = startTimer(settings.debounceDelayMs());
	showSliderValue(value);
}

void OnionSkinsDock::showSliderValue(int value)
{
	QToolTip::showText(QCursor::pos(), tr("Opacity: %1%").arg(value), nullptr);
}

void OnionSkinsDock::showColorPicker(
	const QColor &currentColor, std::function<void(QColor)> onColorSelected)
{
	ColorDialog *dlg = dialogs::newDeleteOnCloseColorDialog(currentColor, this);
	connect(dlg, &ColorDialog::colorSelected, onColorSelected);
	dlg->show();
}

}
