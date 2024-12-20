// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/resizedialog.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/utils/images.h"
#include "ui_resizedialog.h"
#include <QAction>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <tuple>

namespace dialogs {

ResizeDialog::ResizeDialog(
	const QSize &oldsize, const QAction *expandUpAction,
	const QAction *expandLeftAction, const QAction *expandRightAction,
	const QAction *expandDownAction, QWidget *parent)
	: QDialog(parent)
	, m_oldsize(oldsize)
{
	m_ui = new Ui_ResizeDialog;
	m_ui->setupUi(this);

	m_ui->resizer->setOriginalSize(oldsize);
	m_ui->resizer->setTargetSize(oldsize);

	std::tuple<QToolButton *, const QAction *, QString> buttonActions[] = {
		{m_ui->expandUp, expandUpAction, tr("Expand up")},
		{m_ui->expandLeft, expandLeftAction, tr("Expand left")},
		{m_ui->expandRight, expandRightAction, tr("Expand right")},
		{m_ui->expandDown, expandDownAction, tr("Expand down")},
	};
	for(const auto &[button, action, text] : buttonActions) {
		m_ui->grid->setAlignment(button, Qt::AlignCenter);
		QAction *buttonAction = new QAction(action->icon(), text, this);
		buttonAction->setToolTip(
			utils::makeActionShortcutText(action->text(), action->shortcut()));
		buttonAction->setShortcuts(action->shortcuts());
		buttonAction->setAutoRepeat(true);
		button->setDefaultAction(buttonAction);
	}

	m_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Resize"));

	QPushButton *centerButton = new QPushButton(tr("Center"));
	m_ui->buttons->addButton(centerButton, QDialogButtonBox::ActionRole);

	m_ui->width->setValue(m_oldsize.width());
	m_ui->height->setValue(m_oldsize.height());

	connect(
		m_ui->width, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&ResizeDialog::widthChanged);
	connect(
		m_ui->height, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&ResizeDialog::heightChanged);
	connect(
		m_ui->keepaspect, &QCheckBox::toggled, this,
		&ResizeDialog::toggleAspectRatio);
	connect(
		m_ui->expandUp->defaultAction(), &QAction::triggered, this,
		&ResizeDialog::expandUp);
	connect(
		m_ui->expandLeft->defaultAction(), &QAction::triggered, this,
		&ResizeDialog::expandLeft);
	connect(
		m_ui->expandRight->defaultAction(), &QAction::triggered, this,
		&ResizeDialog::expandRight);
	connect(
		m_ui->expandDown->defaultAction(), &QAction::triggered, this,
		&ResizeDialog::expandDown);
	connect(
		centerButton, &QPushButton::clicked, m_ui->resizer,
		&widgets::ResizerWidget::center);
	connect(
		m_ui->buttons->button(QDialogButtonBox::Reset),
		&QAbstractButton::clicked, this, &ResizeDialog::reset);
}

ResizeDialog::~ResizeDialog()
{
	delete m_ui;
}

void ResizeDialog::setBackgroundColor(const QColor &bgColor)
{
	m_ui->resizer->setBackgroundColor(bgColor);
}

void ResizeDialog::setPreviewImage(const QImage &image)
{
	m_ui->resizer->setImage(image);
}

void ResizeDialog::setBounds(const QRect &rect, bool clamp)
{
	QRect rectIn =
		clamp ? rect.intersected({{0, 0}, m_ui->resizer->originalSize()})
			  : rect;

	m_ui->width->setValue(rectIn.width());
	m_ui->height->setValue(rectIn.height());

	m_ui->resizer->setOffset(-rectIn.topLeft());
	m_ui->resizer->setTargetSize(rectIn.size());
}

void ResizeDialog::initialExpand(int expandDirection)
{
	QToolButton *button;
	switch(expandDirection) {
	case int(ExpandDirection::Up):
		button = m_ui->expandUp;
		break;
	case int(ExpandDirection::Left):
		button = m_ui->expandLeft;
		break;
	case int(ExpandDirection::Right):
		button = m_ui->expandRight;
		break;
	case int(ExpandDirection::Down):
		button = m_ui->expandDown;
		break;
	default:
		button = nullptr;
		break;
	}
	if(button) {
		button->click();
		m_ui->buttons->button(QDialogButtonBox::Ok)->setFocus();
	}
}

void ResizeDialog::done(int r)
{
	if(r == QDialog::Accepted) {
		if(!utils::checkImageSize(newSize())) {
			QMessageBox::information(
				this, tr("Error"), tr("Size is too large"));
			return;
		}
	}

	QDialog::done(r);
}

void ResizeDialog::widthChanged(int newWidth)
{
	if(m_aspectratio) {
		QSignalBlocker blocker(m_ui->height);
		m_ui->height->setValue(newWidth / m_aspectratio);
	}
	m_ui->resizer->setTargetSize(QSize(newWidth, m_ui->height->value()));
	m_lastchanged = 0;
}

void ResizeDialog::heightChanged(int newHeight)
{
	if(m_aspectratio) {
		QSignalBlocker blocker(m_ui->width);
		m_ui->width->setValue(newHeight * m_aspectratio);
	}
	m_ui->resizer->setTargetSize(QSize(m_ui->width->value(), newHeight));
	m_lastchanged = 1;
}

void ResizeDialog::toggleAspectRatio(bool keep)
{
	if(keep) {
		m_aspectratio =
			qreal(m_ui->width->value()) / qreal(m_ui->height->value());
	} else {
		m_aspectratio = 0;
	}
}

void ResizeDialog::expandUp()
{
	m_ui->height->setValue(m_ui->height->value() + EXPAND);
	m_ui->resizer->changeOffsetBy(QPoint(0, EXPAND));
}

void ResizeDialog::expandLeft()
{
	m_ui->width->setValue(m_ui->width->value() + EXPAND);
	m_ui->resizer->changeOffsetBy(QPoint(EXPAND, 0));
}

void ResizeDialog::expandRight()
{
	m_ui->width->setValue(m_ui->width->value() + EXPAND);
}

void ResizeDialog::expandDown()
{
	m_ui->height->setValue(m_ui->height->value() + EXPAND);
}

void ResizeDialog::reset()
{
	m_ui->width->blockSignals(true);
	m_ui->height->blockSignals(true);
	m_ui->width->setValue(m_oldsize.width());
	m_ui->height->setValue(m_oldsize.height());
	m_ui->width->blockSignals(false);
	m_ui->height->blockSignals(false);
	m_ui->resizer->setTargetSize(m_oldsize);
}

QSize ResizeDialog::newSize() const
{
	return QSize(m_ui->width->value(), m_ui->height->value());
}

QPoint ResizeDialog::newOffset() const
{
	return m_ui->resizer->offset();
}

QRect ResizeDialog::newBounds() const
{
	return QRect(-newOffset(), newSize());
}

ResizeVector ResizeDialog::resizeVector() const
{
	ResizeVector rv;
	QSize s = newSize();
	QPoint o = newOffset();

	rv.top = o.y();
	rv.left = o.x();
	rv.right = s.width() - m_oldsize.width() - o.x();
	rv.bottom = s.height() - m_oldsize.height() - o.y();

	return rv;
}

}
