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
#include <QStringList>
#include <QMetaType>
#include <tuple>

namespace server {

enum class JsonApiMethod {
	Get,    // Get info about a resource
	Create, // Create a new resource
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
		InternalError=505,
		ConnectionError=-1
	};

	Status status;
	QJsonDocument body;
};

//! A convenience function to generate a standard error message
JsonApiResult JsonApiErrorResult(JsonApiResult::Status status, const QString &message);
inline JsonApiResult JsonApiNotFound() { return JsonApiErrorResult(JsonApiResult::NotFound, QStringLiteral("Not found")); }
inline JsonApiResult JsonApiBadMethod() { return JsonApiErrorResult(JsonApiResult::BadRequest, QStringLiteral("Unsupported method")); /* TODO: correct error type */ }

//! A convenience function that returns the fist path element and the remaining path
std::tuple<QString, QStringList> popApiPath(const QStringList &path);

}

Q_DECLARE_METATYPE(server::JsonApiMethod)
Q_DECLARE_METATYPE(server::JsonApiResult)

#endif
