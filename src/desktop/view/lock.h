// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_VIEW_LOCK_H
#define DESKTOP_VIEW_LOCK_H
#include <QObject>

namespace view {

class Lock : public QObject {
	Q_OBJECT
public:
	enum class Reason : unsigned int {
		None = 0u,
		Reset = 1u << 0u,
		Canvas = 1u << 1u,
		User = 1u << 2u,
		LayerLocked = 1u << 3u,
		LayerGroup = 1u << 4u,
		LayerCensoredRemote = 1u << 5u,
		LayerCensoredLocal = 1u << 6u,
		LayerHidden = 1u << 7u,
		LayerHiddenInFrame = 1u << 8u,
		NoLayer = 1u << 9u,
		Tool = 1u << 10u,
		OutOfSpace = 1u << 11u,
		NoSelection = 1u << 12u,
	};
	Q_ENUM(Reason)

	explicit Lock(QObject *parent = nullptr);

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
