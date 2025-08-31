// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/soundplayer.h"
#include <QFile>
#include <QHash>
#include <QLoggingCategory>
#include <limits>
#include <miniaudio.h>

Q_LOGGING_CATEGORY(lcDpSoundplayer, "net.drawpile.soundplayer", QtWarningMsg)

namespace {
namespace vfs_qt {

static ma_result
onOpenQt(const QString &path, ma_uint32 openMode, ma_vfs_file *pFile)
{
	QIODevice::OpenMode mode;
	if(openMode & MA_OPEN_MODE_READ) {
		if(openMode & MA_OPEN_MODE_WRITE) {
			mode = QIODevice::ReadWrite;
		} else {
			mode = QIODevice::ReadOnly;
		}
	} else if(openMode & MA_OPEN_MODE_WRITE) {
		mode = QIODevice::WriteOnly;
	} else {
		qCWarning(
			lcDpSoundplayer, "Invalid open mode %u",
			static_cast<unsigned int>(openMode));
		return MA_INVALID_ARGS;
	}

	QFile *qfile = new QFile(path);
	if(!qfile->open(mode)) {
		qCWarning(
			lcDpSoundplayer, "Failed to open '%s': %s", qUtf8Printable(path),
			qUtf8Printable(qfile->errorString()));
		delete qfile;
		return MA_DOES_NOT_EXIST;
	}

	*pFile = qfile;
	return MA_SUCCESS;
}

static ma_result onOpen(
	ma_vfs *pVFS, const char *pFilePath, ma_uint32 openMode, ma_vfs_file *pFile)
{
	Q_UNUSED(pVFS);
	return onOpenQt(QString::fromUtf8(pFilePath), openMode, pFile);
}

static ma_result onOpenW(
	ma_vfs *pVFS, const wchar_t *pFilePath, ma_uint32 openMode,
	ma_vfs_file *pFile)
{
	Q_UNUSED(pVFS);
	return onOpenQt(QString::fromWCharArray(pFilePath), openMode, pFile);
}

static ma_result onClose(ma_vfs *pVFS, ma_vfs_file file)
{
	Q_UNUSED(pVFS);
	QFile *qfile = static_cast<QFile *>(file);
	if(qfile) {
		qfile->close();
		delete qfile;
	}
	return MA_SUCCESS;
}

static ma_result onRead(
	ma_vfs *pVFS, ma_vfs_file file, void *pDst, size_t sizeInBytes,
	size_t *pBytesRead)
{
	Q_UNUSED(pVFS);
	QFile *qfile = static_cast<QFile *>(file);
	if(!qfile) {
		qWarning("onRead called by miniaudio with a null file");
		return MA_INVALID_ARGS;
	}

	qint64 read = qfile->read(
		static_cast<char *>(pDst),
		qint64(qMin(sizeInBytes, size_t(std::numeric_limits<qint64>::max()))));
	if(read < 0) {
		qCDebug(lcDpSoundplayer, "Read error %d", int(qfile->error()));
		return MA_IO_ERROR;
	}

	*pBytesRead = size_t(read);
	return MA_SUCCESS;
}

static ma_result onWrite(
	ma_vfs *pVFS, ma_vfs_file file, const void *pSrc, size_t sizeInBytes,
	size_t *pBytesWritten)
{
	Q_UNUSED(pVFS);
	Q_UNUSED(file);
	Q_UNUSED(pSrc);
	Q_UNUSED(sizeInBytes);
	Q_UNUSED(pBytesWritten);
	qCWarning(lcDpSoundplayer, "Unimplemented onWrite called by miniaudio");
	return MA_NOT_IMPLEMENTED;
}

static ma_result
onSeek(ma_vfs *pVFS, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin)
{
	Q_UNUSED(pVFS);
	QFile *qfile = static_cast<QFile *>(file);
	if(!qfile) {
		qWarning("onSeek called by miniaudio with a null file");
		return MA_INVALID_ARGS;
	}

	bool ok;
	switch(origin) {
	case ma_seek_origin_start:
		ok = qfile->seek(offset);
		break;
	case ma_seek_origin_current:
		if(offset > 0) {
			ok = qfile->skip(offset);
		} else if(offset < 0) {
			ok = qfile->seek(qfile->pos() + offset);
		} else {
			ok = true;
		}
		break;
	case ma_seek_origin_end:
		// Not supposed to be used by decoders according to docs.
		qCWarning(
			lcDpSoundplayer,
			"Unimplemented onSeek from end called by minaudio");
		return MA_NOT_IMPLEMENTED;
	default:
		qCWarning(lcDpSoundplayer, "Unknown seek origin %d", int(origin));
		return MA_INVALID_ARGS;
	}

	if(!ok) {
		qCWarning(
			lcDpSoundplayer, "Seek error: %s",
			qUtf8Printable(qfile->errorString()));
		return MA_BAD_SEEK;
	}

	return MA_SUCCESS;
}

static ma_result onTell(ma_vfs *pVFS, ma_vfs_file file, ma_int64 *pCursor)
{
	Q_UNUSED(pVFS);
	QFile *qfile = static_cast<QFile *>(file);
	if(!qfile) {
		qWarning("onTell called by miniaudio with a null file");
		return MA_INVALID_ARGS;
	}

	*pCursor = qfile->pos();
	return MA_SUCCESS;
}

static ma_result onInfo(ma_vfs *pVFS, ma_vfs_file file, ma_file_info *pInfo)
{
	Q_UNUSED(pVFS);
	QFile *qfile = static_cast<QFile *>(file);
	if(!qfile) {
		qWarning("onInfo called by miniaudio with a null file");
		return MA_INVALID_ARGS;
	}

	pInfo->sizeInBytes = qfile->size();
	return MA_SUCCESS;
}

}
}

struct SoundPlayer::Private {
	ma_log log;
	ma_engine engine;
	ma_vfs_callbacks vfs_callbacks = {
		vfs_qt::onOpen,	 vfs_qt::onOpenW, vfs_qt::onClose, vfs_qt::onRead,
		vfs_qt::onWrite, vfs_qt::onSeek,  vfs_qt::onTell,  vfs_qt::onInfo,
	};
	QString backendName;
	QHash<QString, ma_sound *> sounds;
	bool initialized = false;
	bool available = false;

	static int logMessageLength(const char *message)
	{
		// Miniaudio generates log messages with newlines at the end, leading to
		// double newlines in the Qt log. Chomp them off the end.
		size_t len = strlen(message);
		while(len > 0 && message[len - 1] == '\n') {
			--len;
		}
		return int(len);
	}

	static void logCallback(void *user, ma_uint32 level, const char *message)
	{
		Q_UNUSED(user);
		if(message) {
			switch(level) {
			case MA_LOG_LEVEL_DEBUG:
				qCDebug(
					lcDpSoundplayer, "miniaudio: %.*s",
					logMessageLength(message), message);
				break;
			case MA_LOG_LEVEL_INFO:
				qCInfo(
					lcDpSoundplayer, "miniaudio: %.*s",
					logMessageLength(message), message);
				break;
			case MA_LOG_LEVEL_WARNING:
				qCWarning(
					lcDpSoundplayer, "miniaudio: %.*s",
					logMessageLength(message), message);
				break;
			default:
				qCCritical(
					lcDpSoundplayer, "miniaudio: %.*s",
					logMessageLength(message), message);
				break;
			}
		}
	}

	bool init()
	{
		if(!initialized) {
			initialized = true;

			ma_engine_config config = ma_engine_config_init();
			config.pResourceManagerVFS =
				reinterpret_cast<ma_vfs *>(&vfs_callbacks);

			ma_result result = ma_log_init(nullptr, &log);
			if(result == MA_SUCCESS) {
				result = ma_log_register_callback(
					&log, ma_log_callback_init(logCallback, nullptr));
				if(result == MA_SUCCESS) {
					config.pLog = &log;
				} else {
					qCWarning(
						lcDpSoundplayer,
						"Error %d registering log callback: %s", int(result),
						ma_result_description(result));
				}
			} else {
				qCWarning(
					lcDpSoundplayer, "Error %d initializing logging: %s",
					int(result), ma_result_description(result));
			}

			result = ma_engine_init(&config, &engine);
			if(result == MA_SUCCESS) {
				available = true;
				ma_context *ctx =
					ma_device_get_context(ma_engine_get_device(&engine));
				backendName =
					ctx ? QString::fromUtf8(ma_get_backend_name(ctx->backend))
						: QStringLiteral("Invalid");
				qCDebug(
					lcDpSoundplayer, "miniaudio backend: %s",
					qUtf8Printable(backendName));
			} else {
				qCWarning(
					lcDpSoundplayer, "Error %d initializing audio engine: %s",
					int(result), ma_result_description(result));
			}
		}
		return available;
	}

	void dispose()
	{
		if(available) {
			for(ma_sound *sound : sounds) {
				if(sound) {
					ma_sound_uninit(sound);
					delete sound;
				}
			}
			sounds.clear();
			ma_engine_uninit(&engine);
			ma_log_uninit(&log);
			initialized = false;
			available = false;
			backendName.clear();
		}
	}

	ma_result openSound(const QString &path, ma_sound *sound)
	{
		return ma_sound_init_from_file(
			&engine, qUtf8Printable(path),
			MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr,
			nullptr, sound);
	}

	ma_sound *getSound(const QString &path)
	{
		Q_ASSERT(available);

		QHash<QString, ma_sound *>::iterator it = sounds.find(path);
		if(it != sounds.end()) {
			return it.value();
		}

		ma_sound *sound = new ma_sound;
		ma_result result = openSound(path, sound);
		if(result != MA_SUCCESS) {
			delete sound;
			sound = nullptr;
			qCWarning(
				lcDpSoundplayer, "Error %d loading sound '%s': %s", int(result),
				qUtf8Printable(path), ma_result_description(result));
		}
		sounds.insert(path, sound);
		return sound;
	}
};

SoundPlayer::SoundPlayer()
	: d(new Private)
{
}

SoundPlayer::~SoundPlayer()
{
	d->dispose();
	delete d;
}

void SoundPlayer::playSound(const QString &path, int volume)
{
	if(!path.isEmpty() && d->init()) {
		ma_sound *sound = d->getSound(path);
		if(sound) {
			ma_sound_stop(sound);
			ma_sound_seek_to_pcm_frame(sound, 0);
			ma_sound_set_volume(
				sound, qBound(0.0f, float(volume) / 100.0f, 1.0f));
			ma_result result = ma_sound_start(sound);
			if(result != MA_SUCCESS) {
				qCWarning(
					lcDpSoundplayer, "Error %d playing sound '%s': %s",
					int(result), qUtf8Printable(path),
					ma_result_description(result));
			}
		}
	}
}

bool SoundPlayer::isPlaying() const
{
	for(ma_sound *sound : d->sounds) {
		if(sound && ma_sound_is_playing(sound)) {
			return true;
		}
	}
	return false;
}

QString SoundPlayer::getBackendName() const
{
	return QStringLiteral("Miniaudio %1 (%2)")
		.arg(
			MA_VERSION_STRING, d->initialized
								   ? d->backendName
								   : QStringLiteral("not initialized"));
}
