// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/curvewidget.h"
#include "desktop/dialogs/curvepresetdialog.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_curve_widget.h"
#include "desktop/widgets/toolmessage.h"
#include <QClipboard>
#include <QGridLayout>
#include <QGuiApplication>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QStyle>
#include <QVBoxLayout>

namespace widgets {

enum {
	FirstRow = 0,
	FirstCol = 0,

	YMaxLabelRow = FirstRow,
	YTitleLabelRow = YMaxLabelRow + 1,
	YMinLabelRow = YTitleLabelRow + 1,
	XLabelRow = YMinLabelRow + 1,
	LastRow = XLabelRow,

	YLabelCol = FirstCol,
	XMinLabelCol = YLabelCol + 1,
	XTitleLabelCol = XMinLabelCol + 1,
	XMaxLabelCol = XTitleLabelCol + 1,

	CurveFirstRow = YMaxLabelRow,
	CurveLastRow = YMinLabelRow,
	CurveFirstCol = XMinLabelCol,
	CurveLastCol = XMaxLabelCol,

	ButtonSpaceCol = CurveLastCol + 1,
	ButtonCol = ButtonSpaceCol + 1,

	LastCol = ButtonCol,
	ButtonFirstRow = FirstRow,
	ButtonLastRow = LastRow,
};

class CurveWidget::SideLabel final : public QLabel {
public:
	explicit SideLabel(const QString &text, QWidget *parent = nullptr)
		: QLabel{text, parent}
	{
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	}

protected:
	void paintEvent(QPaintEvent *) override final
	{
		QPainter painter{this};
		painter.translate(0, height());
		painter.rotate(-90.0);
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

CurveWidget::CurveWidget(QWidget *parent)
	: CurveWidget{QString{}, QString{}, false, parent}
{
}

CurveWidget::CurveWidget(
	const QString &xTitle, const QString &yTitle, bool linear, QWidget *parent)
	: QWidget{parent}
{
	QGridLayout *grid = new QGridLayout{this};

	const QLocale locale;
	const auto zero = locale.toString(0., 'f', 1);
	const auto one = locale.toString(1., 'f', 1);

	m_yMaxLabel = new QLabel{one};
	m_yTitleLabel = new SideLabel{yTitle};
	m_yMinLabel = new QLabel{zero};

	m_curve = new KisCurveWidget{this};
	m_curve->setLinear(linear);
	connect(
		m_curve, &KisCurveWidget::curveChanged, this,
		&CurveWidget::curveChanged);

	m_xMaxLabel = new QLabel{one};
	m_xTitleLabel = new QLabel{xTitle};
	m_xMinLabel = new QLabel{zero};

	m_buttonLayout = new QVBoxLayout;

	m_copyButton = new QPushButton{tr("Copy")};
	m_copyButton->setAutoDefault(false);
	m_buttonLayout->addWidget(m_copyButton);
	m_copyButton->setToolTip(
		tr("Copies this curve to the clipboard. You can paste it into another "
		   "setting or into the chat to share it with other users."));
	connect(m_copyButton, &QPushButton::clicked, this, &CurveWidget::copyCurve);

	m_pasteButton = new QPushButton{tr("Paste")};
	m_pasteButton->setAutoDefault(false);
	m_buttonLayout->addWidget(m_pasteButton);
	m_pasteButton->setToolTip(tr("Pastes a copied curve from the clipboard, "
								 "overwriting what is there currently."));
	connect(
		m_pasteButton, &QPushButton::clicked, this, &CurveWidget::pasteCurve);

	m_presetsButton = new QPushButton{tr("Presets…")};
	m_presetsButton->setAutoDefault(false);
	m_buttonLayout->addWidget(m_presetsButton);
	m_presetsButton->setToolTip(tr("Save and load curve presets."));
	connect(
		m_presetsButton, &QPushButton::clicked, this, &CurveWidget::loadCurve);

	m_buttonLayout->addStretch();

	grid->setContentsMargins(0, 0, 0, 0);
	grid->setSpacing(3);
	grid->addWidget(
		m_yMaxLabel, YMaxLabelRow, YLabelCol, Qt::AlignRight | Qt::AlignTop);
	grid->addWidget(
		m_yTitleLabel, YTitleLabelRow, YLabelCol,
		Qt::AlignRight | Qt::AlignVCenter);
	grid->addWidget(
		m_yMinLabel, YMinLabelRow, YLabelCol, Qt::AlignRight | Qt::AlignBottom);
	grid->addWidget(
		m_curve, CurveFirstRow, CurveFirstCol, CurveLastRow - CurveFirstRow + 1,
		CurveLastCol - CurveFirstCol + 1);
	grid->addWidget(
		m_xMinLabel, XLabelRow, XMinLabelCol, Qt::AlignLeft | Qt::AlignTop);
	grid->addWidget(
		m_xTitleLabel, XLabelRow, XTitleLabelCol,
		Qt::AlignCenter | Qt::AlignTop);
	grid->addWidget(
		m_xMaxLabel, XLabelRow, XMaxLabelCol, Qt::AlignRight | Qt::AlignTop);
	grid->addLayout(
		m_buttonLayout, ButtonFirstRow, ButtonCol,
		ButtonLastRow - ButtonFirstRow + 1, 1, Qt::AlignLeading);
	grid->setRowStretch(YTitleLabelRow, 1);
	grid->setColumnMinimumWidth(ButtonSpaceCol, 6);
	grid->setColumnStretch(LastCol + 1, 1);

	setCurveSize(300, 300);
}

CurveWidget::~CurveWidget() {}

void CurveWidget::setCurveSize(int width, int height)
{
	m_curve->setFixedSize(width, height);
}

KisCubicCurve CurveWidget::curve() const
{
	return m_curve->curve();
}

void CurveWidget::setCurve(const KisCubicCurve &curve)
{
	m_curve->setCurve(curve);
}

void CurveWidget::setCurveFromString(const QString &curveString)
{
	KisCubicCurve curve;
	curve.fromString(curveString);
	setCurve(curve);
}

void CurveWidget::addVerticalSpacingLabel(const QString &text)
{
	QLabel *label = new QLabel{text, this};
	static_cast<QGridLayout *>(layout())->addWidget(label, 3, 0);
	utils::setWidgetRetainSizeWhenHidden(label, true);
	label->setVisible(false);
}

void CurveWidget::addButton(QAbstractButton *button)
{
	button->setParent(this);
	m_buttonLayout->insertWidget(m_buttonLayout->count() - 2, button);
}

void CurveWidget::setAxisTitleLabels(
	const QString &xTitle, const QString &yTitle)
{
	m_xTitleLabel->setText(xTitle);
	m_yTitleLabel->setText(yTitle);
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

void CurveWidget::loadCurve()
{
	dialogs::CurvePresetDialog *dlg = new dialogs::CurvePresetDialog(
		m_curve->curve(), m_curve->linear(), this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(dlg, &dialogs::CurvePresetDialog::accepted, this, [this, dlg] {
		m_curve->setCurve(dlg->curve());
	});
	dlg->show();
}

}
