// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/avatarimport.h"
#include "desktop/filewrangler.h"
#include "libclient/utils/avatarlistmodel.h"

#include "ui_avatarimport.h"

#include <QImageReader>
#include <QMessageBox>

namespace dialogs {

AvatarImport::AvatarImport(const QImage &source, QWidget *parent)
	: QDialog(parent), m_ui(new Ui_AvatarImport), m_originalImage(source)
{
	const int maxSize = qMin(source.width(), source.height());

	m_ui->setupUi(this);
	m_ui->resizer->setImage(source);
	m_ui->resizer->setOriginalSize(source.size());
	m_ui->resizer->setTargetSize(QSize { maxSize, maxSize });

	m_ui->sizeSlider->setMaximum(maxSize);
	m_ui->sizeSlider->setValue(maxSize);

	connect(m_ui->sizeSlider, &QSlider::valueChanged, this, [this](int value) {
		m_ui->resizer->setTargetSize(QSize { value, value });
	});
}

AvatarImport::~AvatarImport()
{
	delete m_ui;
}

QImage AvatarImport::croppedAvatar() const
{
	const QPoint offset = m_ui->resizer->offset();
	const QSize size = m_ui->resizer->targetSize();

	return m_originalImage.copy(
		-offset.x(),
		-offset.y(),
		size.width(),
		size.height()
	);
}

void AvatarImport::importAvatar(
	AvatarListModel *avatarList, QPointer<QWidget> parentWindow)
{
	FileWrangler::ImageOpenFn imageOpenCompleted = [avatarList, parentWindow](
								 QImage &img) {
		if(img.isNull()) {
			QMessageBox::warning(
				parentWindow, tr("Import Avatar"), tr("Couldn't read image"));
			return;
		}

		if(img.width() < Size || img.height() < Size) {
			QMessageBox::warning(
				parentWindow, tr("Import Avatar"), tr("Picture is too small"));
			return;
		}

		if(img.width() != Size || img.height() != Size ||
		   img.width() != img.height()) {
			// Not square format: needs cropping
			auto *dlg = new dialogs::AvatarImport(img, parentWindow);
			dlg->setModal(true);
			dlg->setAttribute(Qt::WA_DeleteOnClose);
			connect(
				dlg, &QDialog::accepted, avatarList,
				[parentWindow, dlg, avatarList]() {
					avatarList->addAvatar(
						QPixmap::fromImage(dlg->croppedAvatar().scaled(
							Size, Size, Qt::IgnoreAspectRatio,
							Qt::SmoothTransformation)));
				});

			dlg->show();

		} else {
			avatarList->addAvatar(QPixmap::fromImage(img.scaled(
				Size, Size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
		}
	};
	FileWrangler(parentWindow).openAvatar(imageOpenCompleted);
}

}
