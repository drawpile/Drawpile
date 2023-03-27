// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/widgets/curvewidget.h"
#include "desktop/dialogs/curvepresetdialog.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_curve_widget.h"
#include "desktop/widgets/toolmessage.h"
#include "libclient/utils/icon.h"
#include <QClipboard>
#include <QGridLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QStyle>
#include <QVBoxLayout>

namespace widgets {

class CurveWidget::SideLabel final : public QLabel {
public:
	explicit SideLabel(const QString &text, QWidget *parent = nullptr)
		: QLabel{text, parent}
	{
	}

protected:
	void paintEvent(QPaintEvent *) override final
	{
		QPainter painter{this};
		painter.translate(sizeHint().width(), 0);
		painter.rotate(90.0);
		painter.drawText(0, 0, height(), width(), alignment(), text());
	}

	QSize sizeHint() const override final
	{
		return QLabel::sizeHint().transposed();
	}

	QSize minimumSizeHint() const override final
	{
		return QWidget::minimumSizeHint().transposed();
	}
};

CurveWidget::CurveWidget(
	const QString &xTitle, const QString &yTitle, bool linear, QWidget *parent)
	: QWidget{parent}
{
	m_yMaxLabel = new QLabel{"1.00", this};
	m_yMaxLabel->setAlignment(Qt::AlignRight);

	m_yTitleLabel = new SideLabel{yTitle, this};
	m_yTitleLabel->setAlignment(Qt::AlignCenter);
	m_yTitleLabel->setSizePolicy(
		QSizePolicy::Expanding, QSizePolicy::Expanding);

	m_yMinLabel = new QLabel{"0.00", this};
	m_yMinLabel->setAlignment(Qt::AlignRight);

	m_curve = new KisCurveWidget{this};
	m_curve->setLinear(linear);
	m_curve->setFixedSize(300, 300);
	connect(
		m_curve, &KisCurveWidget::curveChanged, this,
		&CurveWidget::curveChanged);

	m_xMaxLabel = new QLabel{"1.00", this};
	m_xMaxLabel->setAlignment(Qt::AlignRight);

	m_xTitleLabel = new QLabel{xTitle, this};
	m_xTitleLabel->setAlignment(Qt::AlignCenter);
	m_xTitleLabel->setSizePolicy(
		QSizePolicy::Expanding, QSizePolicy::Preferred);

	m_xMinLabel = new QLabel{"0.00", this};
	m_xMinLabel->setAlignment(Qt::AlignLeft);

	m_buttonLayout = new QVBoxLayout;

	m_copyButton =
		new QPushButton{icon::fromTheme("edit-copy"), tr("Copy"), this};
	m_buttonLayout->addWidget(m_copyButton);
	m_copyButton->setToolTip(
		tr("Copies this curve to the clipboard. You can paste it into another "
		   "setting or into the chat to share it with other users."));
	connect(m_copyButton, &QPushButton::pressed, this, &CurveWidget::copyCurve);

	m_pasteButton =
		new QPushButton{icon::fromTheme("edit-paste"), tr("Paste"), this};
	m_buttonLayout->addWidget(m_pasteButton);
	m_pasteButton->setToolTip(tr("Pastes a copied curve from the clipboard, "
								 "overwriting what is there currently."));
	connect(
		m_pasteButton, &QPushButton::pressed, this, &CurveWidget::pasteCurve);

	m_presetsButton = new QPushButton{
		icon::fromTheme("document-save"), tr("Presets..."), this};
	m_buttonLayout->addWidget(m_presetsButton);
	m_presetsButton->setToolTip(tr("Save and load curve presets."));
	connect(
		m_presetsButton, &QPushButton::pressed, this, &CurveWidget::loadCurve);

	m_buttonLayout->addSpacerItem(
		new QSpacerItem{0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding});

	QGridLayout *grid = new QGridLayout{this};
	grid->setContentsMargins(0, 0, 0, 0);
	grid->addWidget(m_yMaxLabel, 0, 0, Qt::AlignRight | Qt::AlignTop);
	grid->addWidget(m_yTitleLabel, 1, 0, Qt::AlignRight | Qt::AlignVCenter);
	grid->addWidget(m_yMinLabel, 2, 0, Qt::AlignRight | Qt::AlignBottom);
	grid->addWidget(m_curve, 0, 1, 3, 3);
	grid->addWidget(m_xMinLabel, 3, 1, Qt::AlignLeft | Qt::AlignTop);
	grid->addWidget(m_xTitleLabel, 3, 2, Qt::AlignCenter | Qt::AlignTop);
	grid->addWidget(m_xMaxLabel, 3, 3, Qt::AlignRight | Qt::AlignTop);
	grid->addLayout(m_buttonLayout, 0, 4, 4, 1);
	grid->setColumnStretch(4, 1);
}

CurveWidget::~CurveWidget() {}

KisCubicCurve CurveWidget::curve() const
{
	return m_curve->curve();
}

void CurveWidget::setCurve(const KisCubicCurve &curve)
{
	m_curve->setCurve(curve);
}

void CurveWidget::addVerticalSpacingLabel(const QString &text)
{
	QLabel *label = new QLabel{text, this};
	static_cast<QGridLayout *>(layout())->addWidget(label, 3, 0);
	QSizePolicy sizePolicy = label->sizePolicy();
	sizePolicy.setRetainSizeWhenHidden(true);
	label->setSizePolicy(sizePolicy);
	label->setVisible(false);
}

void CurveWidget::addButton(QAbstractButton *button)
{
	button->setParent(this);
	m_buttonLayout->insertWidget(m_buttonLayout->count() - 2, button);
}

void CurveWidget::setAxisValueLabels(
	const QString &xMin, const QString &xMax, const QString &yMin,
	const QString &yMax)
{
	m_xMinLabel->setText(xMin);
	m_xMaxLabel->setText(xMax);
	m_yMinLabel->setText(yMin);
	m_yMaxLabel->setText(yMax);
}

void CurveWidget::copyCurve()
{
	QString curveString = m_curve->curve().toString();
	QGuiApplication::clipboard()->setText(
		QStringLiteral("[%1:%2]")
			.arg(m_curve->linear() ? "L" : "C")
			.arg(curveString));
	ToolMessage::showText(tr("Curve copied to clipboard."));
}

void CurveWidget::pasteCurve()
{
	static QRegularExpression re{"\\[([LC]):([0-9\\.\\-,;]+)\\]"};
	QString curveString = QGuiApplication::clipboard()->text();
	QRegularExpressionMatch match = re.match(curveString);
	if(match.hasMatch()) {
		KisCubicCurve curve;
		curve.fromString(match.captured(2));
		m_curve->setCurve(curve);
		ToolMessage::showText(tr("Curve pasted from clipboard."));
	} else {
		ToolMessage::showText(
			tr("Clipboard does not appear to contain a curve."));
	}
}

void CurveWidget::saveCurve() {}

void CurveWidget::loadCurve()
{
	dialogs::CurvePresetDialog dlg{m_curve->curve(), m_curve->linear(), this};
	if(dlg.exec() == QDialog::Accepted) {
		m_curve->setCurve(dlg.curve());
	}
}

}
