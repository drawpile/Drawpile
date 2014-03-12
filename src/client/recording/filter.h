/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/
#ifndef REC_FILTER_H
#define REC_FILTER_H

#include "recording/index.h"

namespace protocol {
	class MessagePtr;
	class Message;
}

class QFileDevice;

namespace recording {

class Reader;

class Filter
{
public:
	Filter();

	//! Filter: expunge undone actions
	void setExpungeUndos(bool expunge) { _expunge_undos = expunge; }
	bool expungeUndos() const { return _expunge_undos; }

	//! Filter: remove chat messages
	void setRemoveChat(bool removechat) { _remove_chat = removechat; }
	bool removeChat() const { return _remove_chat; }

	//! Filter: remove users who do not do anything
	void setRemoveLookyloos(bool remove) { _remove_lookyloos = remove; }
	bool removeLookyloos() const { return _remove_lookyloos; }

	//! Filter: remove delays
	void setRemoveDelays(bool remove) { _remove_delays = remove; }
	bool removeDelays() const { return _remove_delays; }

	//! Filter: remove laser pointer trails
	void setRemoveLasers(bool remove) { _remove_lasers = remove; }
	bool removeLasers() const { return _remove_lasers; }

	//! Filter: remove markers
	void setRemoveMarkers(bool remove) { _remove_markers = remove; }
	bool removeMarkers() const { return _remove_markers; }

	//! Filter: remove selected entries
	void setSilenceVector(const IndexVector &entries) { _silence = entries; }
	const IndexVector &silenceVector() const { return _silence; }

	//! Get the error message
	const QString &errorString() const { return _errormsg; }

	/**
	 * @brief Perform filtering.
	 *
	 * If filtering fails, use errorMessage() to get the reason why.
	 *
	 * @return true on success
	 */
	bool filterRecording(QFileDevice *inputfile, QFileDevice *outputfile);


private:
	QString _errormsg;

	bool _expunge_undos;
	bool _remove_chat;
	bool _remove_lookyloos;
	bool _remove_delays;
	bool _remove_lasers;
	bool _remove_markers;
	IndexVector _silence;
};

}

#endif
