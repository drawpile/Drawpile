// SPDX-License-Identifier: GPL-3.0-or-later

#include <QObject>
#include <QTimer>
#include <QVariant>

class DebounceTimer final : public QObject {
	Q_OBJECT
public:
	explicit DebounceTimer(int delayMs, QObject *parent = nullptr);

	void setInt(int value);

signals:
	void intChanged(int value);

protected:
	void timerEvent(QTimerEvent *) override;

private:
	enum class Type { NONE, INT };
    Type m_type;
	int m_delayMs;
	int m_timerId;
	QVariant m_value;
};
