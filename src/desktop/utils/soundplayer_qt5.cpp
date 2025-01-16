// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/soundplayer.h"
#include <QAudioOutput>
#include <QMediaContent>
#include <QMediaPlayer>
#include <QUrl>

struct SoundPlayer::Private {
	QMediaPlayer *player = nullptr;
};

SoundPlayer::SoundPlayer()
	: d(new Private)
{
}

SoundPlayer::~SoundPlayer()
{
	delete d->player;
	delete d;
}

void SoundPlayer::playSound(const QString &path, int volume)
{
	if(!path.isEmpty()) {
		QMediaContent media(QUrl::fromLocalFile(path));
		if(!media.isNull()) {
			if(!d->player) {
				d->player = new QMediaPlayer;
			}
			d->player->stop();
			d->player->setMedia(media);
			d->player->setVolume(volume);
			d->player->setPosition(0);
			d->player->play();
		}
	}
}

bool SoundPlayer::isPlaying() const
{
	return d->player && d->player->state() == QMediaPlayer::PlayingState;
}
