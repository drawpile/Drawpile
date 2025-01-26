// SPDX-License-Identifier: GPL-3.0-or-later

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
		Forbidden=403,
		NotFound=404,
		Conflict=409,
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

int parseRequestInt(
	const QJsonObject &request, const QString &key, int defaultValue,
	int errorValue);

}

Q_DECLARE_METATYPE(server::JsonApiMethod)
Q_DECLARE_METATYPE(server::JsonApiResult)

#endif
