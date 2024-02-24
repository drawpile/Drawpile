// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INITSYS_H
#define INITSYS_H

#include <QString>
#include <QList>

/**
 * Integration with the init system.
 *
 * Currently two backends are provided:
 *
 * - dummy (no integration)
 * - systemd
 */
namespace initsys {

/**
 * @brief What this init system is called
 */
QString name();

/**
 * @brief Send the "server ready" notification
 */
void notifyReady();

/**
 * @brief Send a freeform status update
 * @param status
 */
void notifyStatus(const QString &status);

/**
 * @brief Get file descriptors passed by the init system
 *
 * Currently, we expect either 0 or 1 inet socket.
 * If a socket is passed, we use that instead of opening
 * one ourselves.
 *
 * @return list of socket file descriptors
 */
QList<int> getListenFds();

}

#endif // INITSYS_H
