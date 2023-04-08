// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OPCOMMANDS_H
#define OPCOMMANDS_H

class QString;
class QJsonArray;
class QJsonObject;

namespace server {

class Client;

/**
 * @brief Handle a server command sent by the given client
 * @param client
 * @param command
 */
void handleClientServerCommand(Client *client, const QString &command, const QJsonArray &args, const QJsonObject &kwargs);

}

#endif
