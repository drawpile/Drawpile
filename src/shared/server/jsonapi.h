/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016 Calle Laakkonen

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

#ifndef DP_SERVER_JSONAPI_H
#define DP_SERVER_JSONAPI_H

#include <QJsonDocument>

namespace server {

enum class JsonApiMethod {
	Get,    // Get info about a resource
	Update, // Change resource values
	Delete  // Delete a resource
};

/**
 * @brief Result of a JSON API call
 */
struct JsonApiResult {
	enum Status {
		Ok=200,
		BadRequest=400,
		NotFound=404,
		InternalError=505
	};

	Status status;
	QJsonDocument body;
};

//! A convenience function to generate a standard error message
JsonApiResult JsonApiErrorResult(JsonApiResult::Status status, const QString &message);

}

#endif
