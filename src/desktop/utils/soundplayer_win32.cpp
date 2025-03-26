// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/atomic.h>
#include <dpimpex/wav.h>
}
#include "desktop/utils/soundplayer.h"
#include <Mmsystem.h>
#include <QRunnable>
#include <QThreadPool>
#include <windows.h>

typedef BOOL (*WinmmPlaySoundFn)(LPCSTR pszSound, HMODULE hmod, DWORD fdwSound);

namespace {

static bool winmmLoadAttempted;
static WinmmPlaySoundFn winmmPlaySound;

class SoundPlayerRunnable : public QRunnable {
public:
	SoundPlayerRunnable(DP_Atomic *playing, const QString &path, int volume)
		: m_playing(playing)
		, m_path(path)
		, m_volume(volume)
	{
	}

	void run() override
	{
		unsigned char *wav =
			DP_wav_read_from_path(qUtf8Printable(m_path), m_volume, nullptr);
		if(wav) {
			winmmPlaySound(
				reinterpret_cast<LPCSTR>(wav), nullptr,
				SND_MEMORY | SND_NODEFAULT | SND_SYNC);
			DP_free(wav);
		} else {
			qWarning(
				"Error playing sound '%s': %s", qUtf8Printable(m_path),
				qUtf8Printable(DP_error()));
		}
		DP_atomic_dec(m_playing);
	}

private:
	DP_Atomic *m_playing;
	QString m_path;
	int m_volume;
};

}

struct SoundPlayer::Private {
	DP_Atomic playing = DP_ATOMIC_INIT(0);
};

SoundPlayer::SoundPlayer()
	: d(new Private)
{
}

SoundPlayer::~SoundPlayer()
{
	if(winmmPlaySound && isPlaying()) {
		winmmPlaySound(nullptr, nullptr, SND_NODEFAULT);
	}
	delete d;
}

void SoundPlayer::playSound(const QString &path, int volume)
{
	// As far as I can tell, PlaySound is just busted on 32 bit Windows. This
	// may be due to trying to load it dynamically or something, but even just
	// trying to play back a normal WAV file leads to the program crashing. The
	// reason we switched to PlaySound was because QtMultimedia was crashing on
	// some Windows 7 systems. So we seemingly just get a choice of who gets
	// sound and I guess it's not gonna be 32 bit Windows.
#if defined(Q_OS_WIN) && !defined(_WIN64)
	Q_UNUSED(path);
	Q_UNUSED(volume);
#else
	if(!path.isEmpty()) {
		DP_ATOMIC_DECLARE_STATIC_SPIN_LOCK(lock);
		if(!winmmPlaySound && !winmmLoadAttempted) {
			DP_atomic_lock(&lock);
			if(!winmmPlaySound && !winmmLoadAttempted) {
				winmmLoadAttempted = true;
				HMODULE module = LoadLibraryA("Winmm.dll");
				if(module) {
					FARPROC proc = GetProcAddress(module, "PlaySoundA");
					if(proc) {
						winmmPlaySound =
							reinterpret_cast<WinmmPlaySoundFn>(proc);
					} else {
						qWarning("Failed to load PlaySoundA");
						FreeLibrary(module);
					}
				} else {
					qWarning("Failed to load winmm.dll");
				}
			}
			DP_atomic_unlock(&lock);
		}

		if(winmmPlaySound) {
			// Windows 7 crashes if we try to play more than one sound at once.
			// Newer Windowses instead queue them up one after another, which we
			// also don't really want, it can cause an overly long chain. We
			// instead just skip playing the sound if there's one going already.
			LONG64 playing = InterlockedIncrement64(&d->playing);
			if(playing == 1) {
				QThreadPool::globalInstance()->start(
					new SoundPlayerRunnable(&d->playing, path, volume));
			} else {
				InterlockedDecrement64(&d->playing);
			}
		}
	}
#endif
}

bool SoundPlayer::isPlaying() const
{
	return DP_atomic_get(&d->playing) > 0;
}
