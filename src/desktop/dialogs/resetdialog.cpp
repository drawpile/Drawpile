// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/resetdialog.h"
#include "desktop/filewrangler.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/utils/images.h"
#include "libshared/util/qtcompat.h"
#include "ui_resetsession.h"
#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScopedPointer>
#include <QVector>
#include <functional>

using std::placeholders::_1;
using std::placeholders::_2;

namespace {

enum class Type { Current, Past, External };

struct ResetPoint {
	drawdance::CanvasState canvasState;
	QPixmap thumbnail;
	Type type;
};

QVector<ResetPoint> makeResetPoints(const canvas::PaintEngine *pe)
{
	QVector<ResetPoint> resetPoints;
	pe->snapshotQueue().getSnapshotsWith(
		[&](size_t count, drawdance::SnapshotQueue::SnapshotAtFn at) {
			resetPoints.reserve(compat::castSize(count + 1));
			for(size_t i = 0; i < count; ++i) {
				DP_Snapshot *s = at(i);
				resetPoints.append(ResetPoint{
					drawdance::CanvasState::inc(
						DP_snapshot_canvas_state_noinc(s)),
					QPixmap{},
					Type::Past,
				});
			}
		});

	drawdance::CanvasState currentCanvasState = pe->historyCanvasState();
	int lastIndex = resetPoints.count() - 1;
	// Don't repeat last reset point if the canvas state hasn't changed since.
	if(lastIndex < 0 ||
	   currentCanvasState.get() != resetPoints[lastIndex].canvasState.get()) {
		resetPoints.append(ResetPoint{
			currentCanvasState,
			QPixmap{},
			Type::Current,
		});
	} else {
		resetPoints[lastIndex].type = Type::Current;
	}

	return resetPoints;
}

}

namespace dialogs {

struct ResetDialog::Private {
	QScopedPointer<Ui_ResetDialog> ui;
	const canvas::PaintEngine *paintEngine;
	QPushButton *resetButton;
	QVector<ResetPoint> resetPoints;
	compat::sizetype selection;

	Private(const canvas::PaintEngine *pe)
		: ui{new Ui_ResetDialog}
		, paintEngine{pe}
		, resetPoints{makeResetPoints(pe)}
		, selection{resetPoints.size() - 1}
	{
	}

	~Private() = default;

	static void drawCheckerBackground(QImage &image)
	{
		const int TS = 16;
		const QBrush checker[] = {QColor(128, 128, 128), QColor(Qt::white)};
		QPainter painter(&image);
		painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
		for(int y = 0; y < image.height(); y += TS) {
			for(int x = 0; x < image.width(); x += TS * 2) {
				const int z = (y / TS + x) % 2 == 0;
				painter.fillRect(x, y, TS, TS, checker[z]);
				painter.fillRect(x + TS, y, TS, TS, checker[1 - z]);
			}
		}
	}

	void updateSelection()
	{
		Q_ASSERT(!resetPoints.isEmpty());
		Q_ASSERT(selection >= 0 && selection < resetPoints.size());

		ResetPoint &rp = resetPoints[selection];
		if(rp.thumbnail.isNull()) {
			const drawdance::CanvasState &canvasState = rp.canvasState;
			QImage img = canvasState.toFlatImage();
			if(!img.isNull()) {
				img = img.scaled(256, 256, Qt::KeepAspectRatio);
				drawCheckerBackground(img);
				rp.thumbnail = QPixmap::fromImage(img);
			}
		}

		ui->preview->setPixmap(rp.thumbnail);
	}
};

ResetDialog::ResetDialog(
	const canvas::PaintEngine *pe, bool singleSession, QWidget *parent)
	: QDialog(parent)
	, d(new Private(pe))
{
	d->ui->setupUi(this);

	d->resetButton = d->ui->buttonBox->addButton(
		tr("Reset Session"), QDialogButtonBox::DestructiveRole);
	d->resetButton->setIcon(QIcon::fromTheme("edit-undo"));
	connect(
		d->resetButton, &QPushButton::clicked, this,
		&ResetDialog::resetSelected);

#ifdef SINGLE_MAIN_WINDOW
	Q_UNUSED(singleSession);
#else
	// If we can't open a new window, this would obliterate the current session.
	// That's confusing and not terribly useful, so we don't offer this option.
	// Also, in single-session mode, we don't want to allow opening new windows.
	if(!singleSession) {
		QPushButton *newButton = d->ui->buttonBox->addButton(
			tr("New"), QDialogButtonBox::ActionRole);
		newButton->setIcon(QIcon::fromTheme("document-new"));
		connect(
			newButton, &QPushButton::clicked, this, &ResetDialog::newSelected);
	}
#endif

	QPushButton *openButton = d->ui->buttonBox->addButton(
		tr("Open..."), QDialogButtonBox::ActionRole);
	openButton->setIcon(QIcon::fromTheme("document-open"));
	connect(
		openButton, &QPushButton::clicked, this, &ResetDialog::onOpenClicked);

	d->ui->snapshotSlider->setMaximum(d->resetPoints.size());
	connect(
		d->ui->snapshotSlider, &QSlider::valueChanged, this,
		&ResetDialog::onSelectionChanged);

	d->updateSelection();

	setCanReset(true);
}

ResetDialog::~ResetDialog()
{
	delete d;
}

void ResetDialog::setCanReset(bool canReset)
{
	d->resetButton->setEnabled(canReset);
	d->ui->opLabel->setVisible(!canReset);
}

void ResetDialog::onSelectionChanged(int pos)
{
	int count = d->resetPoints.count();
	d->selection = qBound(0, count - pos, count - 1);
	d->updateSelection();
}

void ResetDialog::onOpenClicked()
{
	FileWrangler(this).openCanvasState(
		std::bind(&ResetDialog::onOpenBegin, this, _1),
		std::bind(&ResetDialog::onOpenSuccess, this, _1),
		std::bind(&ResetDialog::onOpenError, this, _1, _2));
}

void ResetDialog::onOpenBegin(const QString &fileName)
{
	Q_UNUSED(fileName);
	setEnabled(false);
	QApplication::setOverrideCursor(Qt::WaitCursor);
}

void ResetDialog::onOpenSuccess(const drawdance::CanvasState &canvasState)
{
	setEnabled(true);
	QApplication::restoreOverrideCursor();
	d->resetPoints.append({
		canvasState,
		QPixmap(),
		Type::External,
	});
	d->ui->snapshotSlider->setMaximum(d->resetPoints.size());
	d->ui->snapshotSlider->setValue(0);
	d->selection = d->resetPoints.size() - 1;
	d->updateSelection();
}

void ResetDialog::onOpenError(const QString &error, const QString &detail)
{
	setEnabled(true);
	QApplication::restoreOverrideCursor();
	utils::showWarning(
		this, tr("Reset"),
		detail.isEmpty()
			? tr("Error opening file: %1").arg(error)
			: tr("Error opening file: %1 (%2)").arg(error, detail));
}

net::MessageList ResetDialog::getResetImage(bool compatibilityMode) const
{
	net::MessageList resetImage;
	d->resetPoints[d->selection].canvasState.toResetImage(
		resetImage, 0, compatibilityMode);
	return resetImage;
}

QString ResetDialog::getResetImageType() const
{
	switch(d->resetPoints[d->selection].type) {
	case Type::Current:
		return QStringLiteral("current");
	case Type::Past:
		return QStringLiteral("past");
	case Type::External:
		return QStringLiteral("external");
	default:
		return QString();
	}
}

bool ResetDialog::isExternalResetImage() const
{
	return d->resetPoints[d->selection].type == Type::External;
}

}
