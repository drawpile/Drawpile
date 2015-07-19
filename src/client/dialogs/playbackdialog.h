/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2015 Calle Laakkonen

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
#ifndef PLAYBACKDIALOG_H
#define PLAYBACKDIALOG_H

#include <QDialog>
#include <QPointer>

#include "../shared/net/message.h"

class QQuickView;

namespace recording {
	class Reader;
	class IndexLoader;
	class IndexBuilder;
	class PlaybackController;
}

namespace drawingboard {
	class CanvasScene;
}

namespace dialogs {

class PlaybackDialog : public QDialog
{
	Q_OBJECT
public:
	explicit PlaybackDialog(drawingboard::CanvasScene *canvas, recording::Reader *reader, QWidget *parent = 0);
	~PlaybackDialog();

	static recording::Reader *openRecording(const QString &filename, QWidget *msgboxparent=0);

	void centerOnParent();

	bool isPlaying() const;

public slots:
	void done(int r);

	void filterRecording();
	void configureVideoExport();
	void addMarker();

signals:
	void commandRead(protocol::MessagePtr msg);
	void playbackToggled(bool play);

protected:
	void closeEvent(QCloseEvent *);
	void keyPressEvent(QKeyEvent *);

private:
	bool exitCleanup();

	QPointer<QQuickView> m_view;
	recording::PlaybackController *m_ctrl;

	bool m_closing;
};

}

#endif // PLAYBACKDIALOG_H
