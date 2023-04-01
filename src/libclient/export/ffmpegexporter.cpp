// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDebug>
#include <QImage>
#include <QBuffer>
#include <QSettings>

#include "libclient/export/ffmpegexporter.h"
#include "libshared/util/qtcompat.h"

FfmpegExporter::FfmpegExporter(
	Format format, const QString &filename, const QString &customArguments,
	QObject *parent)
	: VideoExporter(parent)
	, m_format{format}
	, m_filename{filename}
	, m_customArguments{customArguments}
	, m_encoder{nullptr}
{
}

QStringList FfmpegExporter::getCommonArguments(int fps)
{
	QStringList args;

	// Image input (via pipe)
	args << "-f" << "image2pipe" << "-c:v" << "bmp" << "-i" << "-";

	// Framerate
	args << "-r" << QString::number(fps);

	return args;
}

QStringList FfmpegExporter::getDefaultMp4Arguments()
{
	return {"-c:v", "libx264", "-pix_fmt", "yuv420p", "-an"};
}

QStringList FfmpegExporter::getDefaultWebmArguments()
{
	return {"-c:v", "libvpx", "-crf", "15", "-b:v", "1M", "-an"};
}

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
// Copied from Qt source code (LGPL licensed):
static QStringList splitCommand(QStringView command)
{
	QStringList args;
	QString tmp;
	int quoteCount = 0;
	bool inQuote = false;

	// handle quoting. tokens can be surrounded by double quotes
	// "hello world". three consecutive double quotes represent
	// the quote character itself.
	for (int i = 0; i < command.size(); ++i) {
		if (command.at(i) == QLatin1Char('"')) {
			++quoteCount;
			if (quoteCount == 3) {
				// third consecutive quote
				quoteCount = 0;
				tmp += command.at(i);
			}
			continue;
		}
		if (quoteCount) {
			if (quoteCount == 1)
				inQuote = !inQuote;
			quoteCount = 0;
		}
		if (!inQuote && command.at(i).isSpace()) {
			if (!tmp.isEmpty()) {
				args += tmp;
				tmp.clear();
			}
		} else {
			tmp += command.at(i);
		}
	}
	if (!tmp.isEmpty())
		args += tmp;

	return args;
}
#else
static inline QStringList splitCommand(QStringView command)
{
	return QProcess::splitCommand(command);
}
#endif

void FfmpegExporter::initExporter()
{
	Q_ASSERT(m_encoder == nullptr);

	QStringList args;
	args.append(getCommonArguments(fps()));

	if(m_format == FFMPEG_MP4) {
		args.append(getDefaultMp4Arguments());
	} else if(m_format == FFMPEG_WEBM) {
		args.append(getDefaultWebmArguments());
	} else {
		args.append(splitCommand(m_customArguments));
	}

	// Output file (overwrite)
	args.append({"-y", m_filename});

	m_encoder = new QProcess(this);
	m_encoder->setProcessChannelMode(QProcess::ForwardedChannels);

	connect(m_encoder, &QProcess::errorOccurred, this, &FfmpegExporter::processError);
	connect(m_encoder, &QProcess::bytesWritten, this, &FfmpegExporter::bytesWritten);
	connect(m_encoder, &QProcess::started, this, &FfmpegExporter::exporterReady);
	connect(m_encoder, SIGNAL(finished(int)), this, SIGNAL(exporterFinished()));

	qDebug() << "Encoding:" << "ffmpeg" << args;

	m_encoder->start("ffmpeg", args);
}

void FfmpegExporter::processError(QProcess::ProcessError error)
{
	qWarning() << "Ffmpeg error:" << error;
	switch(error) {
	case QProcess::FailedToStart:
		emit exporterError(tr("Couldn't start ffmpeg!"));
		break;
	case QProcess::Crashed:
		emit exporterError(tr("Ffmpeg crashed!"));
		break;
	default:
		emit exporterError(tr("Ffmpeg process error"));
		break;
	}
}

void FfmpegExporter::writeFrame(const QImage &image, int repeat)
{
	qInfo("Writing frame (repeat %d)", repeat);
	if(!m_writebuffer.isEmpty()) {
		qWarning("FfmpegExporter: tried to write frame while not yet ready! (%lld of %lld of buffered bytes written", m_written, compat::cast<long long>(m_writebuffer.length()));
		return;
	}

	{
		QBuffer buf(&m_writebuffer);
		buf.open(QIODevice::ReadWrite);
		image.save(&buf, "BMP");
	}

	m_written = 0;
	m_repeats = repeat;

	bytesWritten(0);
}

void FfmpegExporter::bytesWritten(qint64 bytes)
{
	if(bytes < 0) {
		qWarning("FfmpegExporter: write error occurred!");
		return;
	}

	if(m_encoder->bytesToWrite() > 0) {
		// Wait until the QProcess has finished emptying its buffer
		// before writing more.
		return;
	}

	const qint64 bufsize = m_writebuffer.size();
	Q_ASSERT(m_written <= bufsize);

	if(m_written == bufsize) {
		--m_repeats;
		m_written = 0;

		if(m_repeats<=0) {
			m_writebuffer.clear();
			emit exporterReady();
		} else {
			bytesWritten(0);
		}

	} else {
		m_written += m_encoder->write(m_writebuffer.constData() + m_written, bufsize - m_written);
	}
}

void FfmpegExporter::shutdownExporter()
{
	m_encoder->closeWriteChannel();
}

bool FfmpegExporter::checkIsFfmpegAvailable()
{
	QProcess p;
	p.start("ffmpeg", QStringList());
	const bool found = p.waitForStarted();
	p.waitForFinished();

	return found;
}

