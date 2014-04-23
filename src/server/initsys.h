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
#ifndef INITSYS_H
#define INITSYS_H

#include <QString>
#include "../shared/util/logger.h"

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
 * @brief Set log printing function
 *
 */
void setInitSysLogger();

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
