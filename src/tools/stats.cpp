/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#include "stats.h"
#include "../shared/record/reader.h"

#include <cstdio>

using namespace recording;

struct MessageCount {
	QString name;
	unsigned int count;
	unsigned int totalLength;

	MessageCount() : count(0), totalLength(0) {}
};

bool printMessageFrequency(const QString &filename)
{
	// Open recording
	Reader reader(filename);
	Compatibility compat = reader.open();

	switch(compat) {
	case NOT_DPREC:
		fprintf(stderr, "Input file is not a Drawpile recording!\n");
		return false;
	case CANNOT_READ:
		fprintf(stderr, "Unable to read input file: %s\n", qPrintable(reader.errorString()));
		return false;

	case INCOMPATIBLE: // As long as the message envelope format is the same, we don't care abotu the message content
	case COMPATIBLE:
	case MINOR_INCOMPATIBILITY:
	case UNKNOWN_COMPATIBILITY:
		// OK to proceed
		break;
	}

	// Count message types
	MessageCount counts[256];
	unsigned int invalidCount = 0;
	unsigned int totalCount = 0;
	unsigned int totalLength = 0;

	bool notEof = true;
	do {
		MessageRecord mr = reader.readNext();
		switch(mr.status) {
		case MessageRecord::OK: {
			MessageCount &mc = counts[int(mr.message->type())];
			if(mc.name.isNull())
				mc.name = mr.message->messageName();
			mc.count++;
			totalCount++;
			mc.totalLength += mr.message->length();
			totalLength += mr.message->length();
			delete mr.message;
			break;
			}
		case MessageRecord::INVALID: {
			MessageCount &mc = counts[mr.error.type];
			invalidCount++;
			mc.count++;
			totalCount++;
			mc.totalLength += mr.error.len;
			totalLength += mr.error.len;
			break;
		}
		case MessageRecord::END_OF_RECORDING:
			notEof = false;
			break;
		}
	} while(notEof);

	// Print frequency table

	for(int i=0;i<256;++i) {
		if(counts[i].count==0)
			continue;

		printf("%.2x %-17s %-9d %-9d (%.2f%%)\n",
			i,
			counts[i].name.isEmpty() ? "(unknown)" : qPrintable(counts[i].name),
			counts[i].count,
			counts[i].totalLength,
			counts[i].totalLength/double(totalLength)*100
		);
	}
	printf("Total count: %d\n", totalCount);
	printf("Invalid messages: %d\n", invalidCount);
	printf("Total length: %d (%.2f MB)\n", totalLength, totalLength/(1024.0*1024.0));

	return true;
}
