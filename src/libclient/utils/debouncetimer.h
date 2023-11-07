// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_DEBOUNCETIMER_H
#define LIBCLIENT_UTILS_DEBOUNCETIMER_H
#include <QObject>
#include <QTimer>
#include <QVariant>

class DebounceTimer final : public QObject {
	Q_OBJECT
public:
	explicit DebounceTimer(int delayMs, QObject *parent = nullptr);

	void setNone();
	void setInt(int value);

signals:
	void noneChanged();
	void intChanged(int value);

protected:
	void timerEvent(QTimerEvent *) override;

private:
	enum class Type { None, Int };
	Type m_type;
	int m_delayMs;
	int m_timerId;
	QVariant m_value;

	void restartTimer();
};

#endif
