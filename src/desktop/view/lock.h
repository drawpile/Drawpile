// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_VIEW_LOCK_H
#define DESKTOP_VIEW_LOCK_H
#include <QObject>

namespace view {

class Lock : public QObject {
	Q_OBJECT
public:
	enum class Reason : unsigned int {
		None = 0,
		Reset = 1 << 0,
		Canvas = 1 << 1,
		User = 1 << 2,
		LayerLocked = 1 << 3,
		LayerGroup = 1 << 4,
		LayerCensored = 1 << 5,
		LayerHidden = 1 << 6,
		LayerHiddenInFrame = 1 << 7,
		Tool = 1 << 8,
		OutOfSpace = 1 << 9,
	};
	Q_ENUM(Reason)

	bool hasReason(Reason reason) const { return m_reasons.testFlag(reason); }
	void setReasons(QFlags<Reason> reasons);
	const QString description() const { return m_description; }

signals:
	void reasonsChanged(QFlags<Reason> reasons);
	void descriptionChanged(const QString &description);

private:
	QString buildDescription() const;

	QFlags<Reason> m_reasons;
	QString m_description;
};

}

#endif
