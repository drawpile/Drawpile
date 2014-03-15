/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#ifndef PLAYBACKDIALOG_H
#define PLAYBACKDIALOG_H

#include <QDialog>
#include <QPointer>

#include "../shared/net/message.h"

namespace recording {
	class Reader;
	class IndexLoader;
	class IndexBuilder;
}

namespace drawingboard {
	class CanvasScene;
}

class VideoExporter;
class QTimer;
class QGraphicsScene;
class IndexPointerGraphicsItem;

class Ui_PlaybackDialog;

namespace dialogs {

class TinyPlayer;

class PlaybackDialog : public QDialog
{
	Q_OBJECT
public:
	explicit PlaybackDialog(drawingboard::CanvasScene *canvas, recording::Reader *reader, QWidget *parent = 0);
	~PlaybackDialog();

	static recording::Reader *openRecording(const QString &filename, QWidget *msgboxparent=0);

	bool isPlaying() const { return _play; }

	void centerOnParent();

signals:
	void commandRead(protocol::MessagePtr msg);
	void playbackToggled(bool play);

public slots:
	void togglePlay(bool play);
	void nextCommand();
	void nextSequence();

	void prevSnapshot();
	void nextSnapshot();

	void prevMarker();
	void nextMarker();

	virtual void done(int r);

protected:
	void closeEvent(QCloseEvent *);

private slots:
	void exportFrame(int count=1);
	void exportConfig();

	void exporterError(const QString &message);
	void exporterReady();
	void exporterFinished();

	void makeIndex();
	void indexMade(bool ok, const QString &msg);
	void filterRecording();

	void jumpTo(int pos);

	void addMarkerHere();

private:
	void createIndexView();
	void endOfFileReached();
	bool waitForExporter();
	void loadIndex();
	void jumptToSnapshot(int idx);
	void updateIndexPosition();

	bool exitCleanup();

	QString indexFileName() const;

	Ui_PlaybackDialog *_ui;

	recording::Reader *_reader;
	recording::IndexLoader *_index;
	QPointer<recording::IndexBuilder> _indexbuilder;

	QGraphicsScene *_indexscene;
	IndexPointerGraphicsItem *_indexpositem;

	drawingboard::CanvasScene *_canvas;	
	VideoExporter *_exporter;
	QTimer *_timer;
	float _speedfactor;

	TinyPlayer *_tinyPlayer;

	bool _play;
	bool _exporterReady;
	bool _waitedForExporter;
	bool _closing;
};

}

#endif // PLAYBACKDIALOG_H
