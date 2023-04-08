// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IMAGESERIESEXPORTER_H
#define IMAGESERIESEXPORTER_H

#include "libclient/export/videoexporter.h"

class ImageSeriesExporter final : public VideoExporter
{
	Q_OBJECT
public:
	ImageSeriesExporter(QObject *parent = nullptr);

	void setOutputPath(const QString &path) { _path = path; }
	void setFilePattern(const QString &pattern) { _filepattern = pattern; }
	void setFormat(const QString &format) { _format = format.toLatin1(); }

protected:
	void initExporter() override;
	void writeFrame(const QImage &image, int repeat) override;
	void shutdownExporter() override;
	bool variableSizeSupported() override { return true; }

private:
	QString _path;
	QString _filepattern;
	QByteArray _format;
};

#endif // IMAGESERIESEXPORTER_H
