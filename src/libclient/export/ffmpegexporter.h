// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FFMPEGEXPORTER_H
#define FFMPEGEXPORTER_H

#include <QProcess>
#include <QByteArray>

#include "libclient/export/videoexporter.h"

class FfmpegExporter final : public VideoExporter
{
	Q_OBJECT
public:
	explicit FfmpegExporter(
		Format format, const QString &ffmpegPath, const QString &filename,
		const QString &customArguments, QObject *parent = nullptr);

	//! Get the arguments that are always given to the encoder process
	static QStringList getCommonArguments(int fps);
	static QStringList getDefaultMp4Arguments();
	static QStringList getDefaultWebmArguments();

private slots:
	void processError(QProcess::ProcessError error);
	void bytesWritten(qint64 bytes);
	void processFinished(int exitCode, QProcess::ExitStatus exitStatus);

protected:
	void initExporter() override;
	void writeFrame(const QImage &image, int repeat) override;
	void shutdownExporter() override;

private:
	Format m_format;
	QString m_ffmpegPath;
	QString m_filename;
	QString m_customArguments;

	QProcess *m_encoder;
	QByteArray m_writebuffer;
	qint64 m_written;
	int m_repeats;
};

#endif // FFMPEGEXPORTER_H
