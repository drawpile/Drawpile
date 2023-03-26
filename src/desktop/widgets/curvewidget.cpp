// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/widgets/curvewidget.h"
#include "desktop/dialogs/curvepresetdialog.h"
#include "desktop/widgets/kis_curve_widget.h"
#include "desktop/widgets/toolmessage.h"
#include "libclient/utils/icon.h"
#include <QClipboard>
#include <QGridLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace widgets {

CurveWidget::CurveWidget(bool linear, QWidget *parent)
	: QWidget{parent}
{
	QGridLayout *grid = new QGridLayout;
	setLayout(grid);
	grid->setContentsMargins(0, 0, 0, 0);

	m_yMaxLabel = new QLabel{"1.00", this};
	grid->addWidget(m_yMaxLabel, 0, 0, Qt::AlignRight | Qt::AlignTop);
	m_yMinLabel = new QLabel{"0.00", this};
	grid->addWidget(m_yMinLabel, 1, 0, Qt::AlignRight | Qt::AlignBottom);

	m_curve = new KisCurveWidget{this};
	grid->addWidget(m_curve, 0, 1, 2, 2);
	m_curve->setLinear(linear);
	m_curve->setFixedSize(300, 300);
	connect(
		m_curve, &KisCurveWidget::curveChanged, this,
		&CurveWidget::curveChanged);

	m_xMaxLabel = new QLabel{"1.00", this};
	grid->addWidget(m_xMaxLabel, 2, 2, Qt::AlignRight | Qt::AlignTop);
	m_xMinLabel = new QLabel{"0.00", this};
	grid->addWidget(m_xMinLabel, 2, 1, Qt::AlignLeft | Qt::AlignTop);

	m_buttonLayout = new QVBoxLayout;
	grid->addLayout(m_buttonLayout, 0, 3, 1, 3);

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

	grid->setColumnStretch(3, 1);
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
	m_buttonLayout->addWidget(button);
}

void CurveWidget::setAxisLabels(
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
