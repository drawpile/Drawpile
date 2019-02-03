/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018-2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef USERITEMDELEGATE_H
#define USERITEMDELEGATE_H

#include <QAbstractItemDelegate>

class QMenu;

namespace canvas {
    class CanvasModel;
}

namespace protocol {
    class MessagePtr;
}

namespace widgets {

class UserItemDelegate : public QAbstractItemDelegate
{
	Q_OBJECT
public:
	UserItemDelegate(QObject *parent=nullptr);
	~UserItemDelegate();

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
	bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;

	void setCanvas(canvas::CanvasModel *canvas) { m_canvas = canvas; }

signals:
	void opCommand(protocol::MessagePtr msg);
	void requestPrivateChat(int userId);

private slots:
	void toggleOpMode(bool op);
	void toggleTrusted(bool trust);
	void toggleLock(bool lock);
	void toggleMute(bool mute);
	void kickUser();
	void banUser();
	void pmUser();

private:
	void showContextMenu(const QModelIndex &index, const QPoint &pos);

	QMenu *m_opMenu;
	QMenu *m_userMenu;

	canvas::CanvasModel *m_canvas;

	QPixmap m_lockIcon;
	QPixmap m_muteIcon;

	QAction *m_opAction;
	QAction *m_trustAction;
	QAction *m_lockAction;
	QAction *m_muteAction;
	QAction *m_kickAction;
	QAction *m_banAction;
	QAction *m_chatAction;

	int m_menuId;
};

}

#endif
