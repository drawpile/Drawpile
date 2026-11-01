// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_IO_CLIPBOARD_H
#define LIBCLIENT_IO_CLIPBOARD_H
#include <QImage>
#include <QMimeData>

namespace io {
namespace clip {

const QMimeData *mimeData();
bool hasImage();
QImage getImageFrom(const QMimeData *data);

// Edits the clipboard data and commits it on destruction.
class EditableMimeData final {
	Q_DISABLE_COPY_MOVE(EditableMimeData)
public:
	EditableMimeData();
	~EditableMimeData();

	void setData(const QString &mimeType, const QByteArray &data);
	void setImageData(const QImage &img);

private:
	QMimeData *m_data;
};

}
}

#endif
