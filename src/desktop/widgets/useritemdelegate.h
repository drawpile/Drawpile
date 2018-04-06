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

private slots:
	void toggleOpMode(bool op);
	void toggleLock(bool lock);
	void toggleMute(bool mute);
	void kickUser();
	void banUser();

private:
	QMenu *m_userMenu;
	canvas::CanvasModel *m_canvas;

	QPixmap m_lockIcon;
	QPixmap m_muteIcon;

	QAction *m_opAction;
	//QAction *m_trustAction;
	QAction *m_lockAction;
	QAction *m_muteAction;
	QAction *m_kickAction;
	QAction *m_banAction;

	int m_menuId;
};

}

#endif
