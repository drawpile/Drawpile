// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_IO_CLIPBOARD_H
#define LIBCLIENT_IO_CLIPBOARD_H
#include "libclient/utils/transformquad.h"
#include <QImage>
#include <QMimeData>
#include <QRect>

namespace io {
namespace clip {

struct TransformState {
	TransformQuad quad;
	QRect srcBounds;
	float opacity;
	int blendMode;
	int handleMode;
	bool constrain;
	bool center;

	bool isNull() const { return quad.isNull(); }
};

const QMimeData *mimeData();

bool hasImage();

QImage getImageFrom(const QMimeData *data);
QImage getSourceImageFrom(const QMimeData *data);
TransformState getTransformStateFrom(const QMimeData *data);
QByteArray getLiquifyDataFrom(const QMimeData *data);

// Edits the clipboard data and commits it on destruction.
class EditableMimeData final {
	Q_DISABLE_COPY_MOVE(EditableMimeData)
public:
	EditableMimeData();
	~EditableMimeData();

	void setData(const QString &mimeType, const QByteArray &data);
	void setImageData(const QImage &img);
	void setSourceImage(const QImage &img);
	void setTransformState(const TransformState &state);
	void setLiquifyData(const QByteArray &data);

private:
	void setPngData(const QString &mimeType, const QImage &img);

	QMimeData *m_data;
};

}
}

#endif
