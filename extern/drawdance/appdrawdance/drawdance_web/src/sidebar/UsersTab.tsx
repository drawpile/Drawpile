/*
 * Copyright (c) 2022 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
import React from "react";
import { User } from "../Common";

interface UsersTabProps {
  connected: boolean;
  users: Map<number, User>;
  active: boolean;
}

function isConnected(user: User) {
  return user.connected;
}

function compareUsers({ name: a }: User, { name: b }: User): number {
  return a < b ? -1 : a > b ? 1 : 0;
}

function getUserListItems({
  connected,
  users,
}: UsersTabProps): React.ReactElement | React.ReactElement[] {
  if (!connected) {
    return (
      <li className="list-group-item list-group-item-secondary">
        Not connected.
      </li>
    );
  } else if (users.size === 0) {
    return (
      <li className="list-group-item list-group-item-secondary">No users.</li>
    );
  } else {
    const userList = Array.from(users.values()).filter(isConnected);
    userList.sort(compareUsers);
    return userList.map(({ contextId, name }: User): React.ReactElement => {
      return (
        <li key={contextId} className="list-group-item">
          {name}
        </li>
      );
    });
  }
}

function UsersTab(props: UsersTabProps): React.ReactElement {
  const { active } = props;
  return (
    <div className={active ? "overflow-auto" : "d-none"}>
      <ul className="list-group">{getUserListItems(props)}</ul>
    </div>
  );
}

export default UsersTab;
