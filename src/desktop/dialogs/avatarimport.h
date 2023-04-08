// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AVATARIMPORT_H
#define AVATARIMPORT_H

#include <QDialog>
#include <QPointer>

class Ui_AvatarImport;

class QImage;
class AvatarListModel;

namespace dialogs {

class AvatarImport final : public QDialog
{
	Q_OBJECT
public:
	AvatarImport(const QImage &source, QWidget *parent=nullptr);
	~AvatarImport() override;

	// Size of the final avatar image
	static const int Size = 32;

	QImage croppedAvatar() const;

	static void importAvatar(AvatarListModel *avatarList, QPointer<QWidget> parentWindow=QPointer<QWidget>());

private:
	Ui_AvatarImport *m_ui;
	QImage m_originalImage;
};

}

#endif
