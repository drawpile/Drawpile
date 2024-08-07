#include <QCursor>
#include <QGuiApplication>

namespace utils {

class ScopedOverrideCursor {
public:
	ScopedOverrideCursor()
		: ScopedOverrideCursor(QCursor(Qt::WaitCursor))
	{
	}

	ScopedOverrideCursor(const QCursor &cursor)
	{
		QGuiApplication::setOverrideCursor(cursor);
	}

	~ScopedOverrideCursor() { QGuiApplication::restoreOverrideCursor(); }
};

}
