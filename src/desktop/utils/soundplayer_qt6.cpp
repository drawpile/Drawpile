// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/soundplayer.h"
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QUrl>

struct SoundPlayer::Private {
	QMediaPlayer *player = nullptr;
	QAudioOutput *output = nullptr;
};

SoundPlayer::SoundPlayer()
	: d(new Private)
{
}

SoundPlayer::~SoundPlayer()
{
	delete d->output;
	delete d->player;
	delete d;
}

void SoundPlayer::playSound(const QString &path, int volume)
{
	if(!path.isEmpty()) {
		QUrl url = QUrl::fromLocalFile(path);
		if(url.isValid()) {
			if(!d->player) {
				d->player = new QMediaPlayer;
				d->output = new QAudioOutput(d->player);
				d->player->setAudioOutput(d->output);
			}
			d->player->stop();
			d->player->setSource(url);
			d->output->setVolume(qreal(volume) / 100.0);
			d->player->setPosition(0);
			d->player->play();
		}
	}
}

bool SoundPlayer::isPlaying() const
{
	return d->player &&
		   d->player->playbackState() == QMediaPlayer::PlayingState;
}
