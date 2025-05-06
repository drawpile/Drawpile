// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/canvas_state.h>
}
#include "libclient/import/loadresult.h"
#include <QCoreApplication>

namespace impex {

QString getLoadResultMessage(DP_LoadResult result)
{
	// The messages were originally in the MainWindow class, so we'll keep the
	// context the same as to not cause extra work for translators.
	constexpr char CONTEXT[] = "MainWindow";
	switch(result) {
	case DP_LOAD_RESULT_SUCCESS:
		return QCoreApplication::translate(CONTEXT, "Success.");
	case DP_LOAD_RESULT_BAD_ARGUMENTS:
		return QCoreApplication::translate(
			CONTEXT, "Bad arguments, this is probably a bug in Drawpile.");
	case DP_LOAD_RESULT_UNKNOWN_FORMAT:
		return QCoreApplication::translate(CONTEXT, "Unsupported format.");
	case DP_LOAD_RESULT_OPEN_ERROR:
		return QCoreApplication::translate(
			CONTEXT, "Couldn't open file for reading.");
	case DP_LOAD_RESULT_READ_ERROR:
		return QCoreApplication::translate(CONTEXT, "Error reading file.");
	case DP_LOAD_RESULT_BAD_MIMETYPE:
		return QCoreApplication::translate(
			CONTEXT, "File content doesn't match its type.");
	case DP_LOAD_RESULT_RECORDING_INCOMPATIBLE:
		return QCoreApplication::translate(CONTEXT, "Incompatible recording.");
	case DP_LOAD_RESULT_UNSUPPORTED_PSD_BITS_PER_CHANNEL:
		return QCoreApplication::translate(
			CONTEXT,
			"Unsupported bits per channel. Only 8 bits are supported.");
	case DP_LOAD_RESULT_UNSUPPORTED_PSD_COLOR_MODE:
		return QCoreApplication::translate(
			CONTEXT, "Unsupported color mode. Only RGB/RGBA is supported.");
	case DP_LOAD_RESULT_BAD_DIMENSIONS:
		return QCoreApplication::translate(
				   CONTEXT, "Image too large, it must be a most %1 pixels in "
							"width or height and have at most %2 pixels total.")
			.arg(DP_CANVAS_STATE_MAX_DIMENSION)
			.arg(DP_CANVAS_STATE_MAX_PIXELS);
	case DP_LOAD_RESULT_INTERNAL_ERROR:
		return QCoreApplication::translate(
			CONTEXT, "Internal error, this is probably a bug.");
	}
	qWarning("Unhandled load result %d in %s", int(result), __func__);
	return QCoreApplication::translate(CONTEXT, "Unknown error.");
}

bool shouldIncludeLoadResultDpError(DP_LoadResult result)
{
	switch(result) {
	case DP_LOAD_RESULT_SUCCESS:
	case DP_LOAD_RESULT_BAD_ARGUMENTS:
	case DP_LOAD_RESULT_UNKNOWN_FORMAT:
	case DP_LOAD_RESULT_BAD_MIMETYPE:
	case DP_LOAD_RESULT_RECORDING_INCOMPATIBLE:
	case DP_LOAD_RESULT_UNSUPPORTED_PSD_BITS_PER_CHANNEL:
	case DP_LOAD_RESULT_UNSUPPORTED_PSD_COLOR_MODE:
	case DP_LOAD_RESULT_BAD_DIMENSIONS:
	case DP_LOAD_RESULT_INTERNAL_ERROR:
		return false;
	case DP_LOAD_RESULT_OPEN_ERROR:
	case DP_LOAD_RESULT_READ_ERROR:
		return true;
	}
	qWarning("Unhandled load result %d in %s", int(result), __func__);
	return false;
}

}
