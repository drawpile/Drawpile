/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018-2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "renderer.h"

#include "../libclient/canvas/statetracker.h"
#include "../libclient/canvas/layerlist.h"
#include "../libclient/canvas/aclfilter.h"
#include "../libclient/core/layerstack.h"
#include "../libclient/ora/orawriter.h"
#include "../libshared/record/reader.h"

#include <QImageWriter>
#include <QElapsedTimer>
#include <QPainter>

struct ExportState {
	QSize lastSize;
	int index;
};

QImage resizeImage(const QImage &img, const QSize &maxSize, bool fixedSize)
{
	if(fixedSize && img.size() != maxSize) {
		const QSize scaledSize = img.size().scaled(maxSize, Qt::KeepAspectRatio);
		QImage scaled(maxSize, QImage::Format_ARGB32_Premultiplied);
		scaled.fill(Qt::black);

		QPainter painter(&scaled);
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
		painter.drawImage(
			QRect(
				QPoint((maxSize.width() - scaledSize.width()) / 2, (maxSize.height() - scaledSize.height()) / 2),
				scaledSize
			),
			img
			);
		return scaled;

	} else if (img.width() > maxSize.width() || img.height() > maxSize.height()) {
		return img.scaled(maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

	} else {
		return img;
	}
}

bool saveImage(const DrawpileCmdSettings &settings, const paintcore::LayerStack &layers, ExportState &state)
{
	if(layers.size().isEmpty() || layers.layerCount()==0) {
		// The layer stack has no size until the first resize command.
		// Trying to export before it is not a fatal error.
		if(settings.verbose)
			fprintf(stderr, "[I] Image is empty, not saving anything.\n");
		return true;
	}

	QString filename = settings.outputFilePattern;

	// Perform pattern subsitutions:
	// :idx: <-- image index number
	filename.replace(":idx:", QString::number(state.index));

	if(settings.verbose)
		fprintf(stderr, "[I] Writing %s...\n", qPrintable(filename));

	QString error;
	bool ok;
	if(filename.endsWith(".ora", Qt::CaseInsensitive)) {
		// Special case: Save as OpenRaster with all the layers intact
		// ORAs are not resized.
		ok = openraster::saveOpenRaster(filename, &layers, &error);

	} else {
		QImage flat = layers.toFlatImage(settings.mergeAnnotations, true);

		if(settings.fixedSize && state.lastSize.isEmpty())
			state.lastSize = flat.size();

		if(!state.lastSize.isEmpty())
			flat = resizeImage(flat, state.lastSize, settings.fixedSize);

		QImageWriter writer(filename);
		ok = writer.write(flat);
		if(!ok)
			error = writer.errorString();
	}

	if(!ok)
		fprintf(stderr, "[E] %s: %s\n", qPrintable(filename), qPrintable(error));

	++state.index;

	return ok;
}

QString prettyDuration(qint64 duration)
{
	const double msecs = duration / 1000000;
	if(msecs < 1000)
		return QStringLiteral("%1 ms").arg(msecs, 0, 'f', 1);
	const double secs = msecs / 1000;
	if(secs<60)
		return QStringLiteral("%1 s").arg(secs, 0, 'f', 2);
	return QStringLiteral("%1 m %2s").arg(secs/60, 0, 'f', 0).arg(fmod(secs, 60), 0, 'f', 2);
}

bool renderDrawpileRecording(const DrawpileCmdSettings &settings)
{
	// Open recording file
	recording::Reader reader(settings.inputFilename);
	recording::Compatibility compat = reader.open();

	if(compat == recording::CANNOT_READ) {
		fprintf(stderr, "[E] %s", qPrintable(reader.errorString()));
		return false;
	}

	if(compat != recording::COMPATIBLE && compat != recording::MINOR_INCOMPATIBILITY) {
		fprintf(stderr, "[E] Recording not compatible\n");
		return false;
	}

	// Initialize the paint engine
	paintcore::LayerStack image;
	canvas::LayerListModel layermodel;
	canvas::StateTracker statetracker(&image, &layermodel, 1);
	canvas::AclFilter aclfilter;

	aclfilter.reset(1, false);

	// Benchmarking
	QElapsedTimer renderTime;
	QElapsedTimer saveTime;
	QElapsedTimer totalTime;
	qint64 totalRenderTime = 0;
	qint64 totalSaveTime = 0;
	totalTime.start();

	// Prepare image exporter
	ExportState exportState {
		settings.maxSize,
		1
	};
	int exportCounter = 0;

	// Read and execute commands
	recording::MessageRecord record;
	do {
		const qint64 offset = reader.filePosition();
		record = reader.readNext();

		if(record.status == recording::MessageRecord::OK) {
			if(settings.acl && !aclfilter.filterMessage(*record.message)) {
				if(settings.verbose)
					fprintf(stderr, "[A] Filtered message %s from %d (idx %d @ %llx)",
						qPrintable(record.message->messageName()),
						record.message->contextId(),
						reader.currentIndex(),
						offset
						);
			}

			if(record.message->isCommand()) {
				renderTime.start();
				statetracker.receiveCommand(protocol::MessagePtr::fromNullable(record.message));
				totalRenderTime += renderTime.nsecsElapsed();
			}

			// Save images
			if(settings.exportEveryN > 0) {
				switch(settings.exportEveryMode) {
				case ExportEvery::Message:
					++exportCounter;
					break;
				case ExportEvery::Sequence:
					if(record.message->type() == protocol::MSG_UNDOPOINT)
						++exportCounter;
					break;
				}

				if(exportCounter >= settings.exportEveryN) {
					exportCounter = 0;
					saveTime.start();
					if(!saveImage(settings, image, exportState))
						return false;
					totalSaveTime += saveTime.nsecsElapsed();
				}
			}

		} else if(record.status == recording::MessageRecord::INVALID) {
			fprintf(stderr, "[E] Invalid message type %d at index %d, offset 0x%llx",
					record.invalid_type,
					reader.currentIndex(),
					offset
				   );
			return false;
		}
	} while(record.status != recording::MessageRecord::END_OF_RECORDING);

	fprintf(stderr, "[I] Total processing time: %s\n", qPrintable(prettyDuration(totalTime.nsecsElapsed())));
	fprintf(stderr, "[I] Cumulative render time: %s\n", qPrintable(prettyDuration(totalRenderTime)));

	// Save the final result
	saveTime.start();
	if(!saveImage(settings, image, exportState))
		return false;
	totalSaveTime += saveTime.nsecsElapsed();

	fprintf(stderr, "[I] Cumulative saving time: %s\n", qPrintable(prettyDuration(totalSaveTime)));

	return true;
}

