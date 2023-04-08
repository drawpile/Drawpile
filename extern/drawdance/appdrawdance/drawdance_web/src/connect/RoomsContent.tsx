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
import {
  faDoorClosed,
  faExclamationTriangle,
  faLock,
  faSignInAlt,
} from "@fortawesome/free-solid-svg-icons";
import { FontAwesomeIcon } from "@fortawesome/react-fontawesome";
import React from "react";
import { Setter } from "../Common";
import { Drawdance, Room } from "../Drawdance";
import { ConnectDialogMode } from "./ConnectCommon";

interface RoomsContentProps {
  drawdance: Drawdance;
  setMode: Setter<ConnectDialogMode>;
  clientIdConnecting: number | undefined;
  sessionTitle: string | undefined;
  rooms: Room[];
  children: any;
}

const CLOSED_FLAG = (
  <FontAwesomeIcon
    icon={faDoorClosed}
    className="text-secondary"
    title="Closed"
  />
);

const PASSWORD_FLAG = (
  <FontAwesomeIcon
    icon={faLock}
    className="text-primary"
    title="Password-Protected"
  />
);

const NSFM_FLAG = (
  <FontAwesomeIcon
    icon={faExclamationTriangle}
    className="text-danger"
    title="Age-Restricted Content"
  />
);

function getTableRow(
  room: Room,
  selectedRoomId: string | undefined,
  setSelectedRoomId: Setter<string | undefined>,
  join: () => void
): React.ReactElement {
  const classes = ["cursor-pointer"];
  if (room.id === selectedRoomId) {
    classes.push(room.closed ? "table-secondary" : "table-primary");
  }
  return (
    <tr
      key={room.id}
      className={classes.join(" ")}
      onClick={() => setSelectedRoomId(room.id)}
      onDoubleClick={join}
    >
      <td className="align-middle">{room.title}</td>
      <td className="align-middle">{room.founder}</td>
      <td className="align-middle">{room.users}</td>
      <td className="align-middle">
        {room.closed ? CLOSED_FLAG : undefined}{" "}
        {room.password ? PASSWORD_FLAG : undefined}{" "}
        {room.nsfm ? NSFM_FLAG : undefined}
      </td>
    </tr>
  );
}

function getTableBody(
  rooms: Room[],
  selectedRoomId: string | undefined,
  setSelectedRoomId: Setter<string | undefined>,
  join: () => void
): React.ReactElement | React.ReactElement[] {
  if (rooms.length === 0) {
    return (
      <tr>
        <td colSpan={4}>No rooms running right now.</td>
      </tr>
    );
  } else {
    return rooms.map((room) =>
      getTableRow(room, selectedRoomId, setSelectedRoomId, join)
    );
  }
}

function RoomsContent(props: RoomsContentProps): React.ReactElement {
  const {
    drawdance,
    setMode,
    clientIdConnecting,
    sessionTitle,
    rooms,
    children,
  } = props;

  const [selectedRoomId, setSelectedRoomId] = React.useState<string>();

  const selectedRoom =
    selectedRoomId && rooms.find((room) => room.id === selectedRoomId);
  const joinDisabled = !selectedRoom || selectedRoom.closed;
  const join = () => {
    if (!joinDisabled) {
      setMode("JOINING");
      drawdance.publish("LOGIN_PROVIDE_ROOM", {
        client_id: clientIdConnecting,
        room_id: selectedRoomId,
      });
    }
  };

  return (
    <div className="card">
      <div className="card-header">
        <h2 className="card-title">{sessionTitle || "Select Room"}</h2>
      </div>
      <div id="rooms-table-height-limiter" className="table-responsive">
        <table className="table table-hover mt-0 mb-0">
          <thead>
            <tr className="table-active">
              <th>Title</th>
              <th>Started by</th>
              <th>Users</th>
              <th>Flags</th>
            </tr>
          </thead>
          <tbody>
            {getTableBody(rooms, selectedRoomId, setSelectedRoomId, join)}
          </tbody>
        </table>
      </div>
      <div className="card-footer">
        <div className="btn-group">
          <button
            type="button"
            className="btn btn-primary"
            disabled={joinDisabled}
            onClick={join}
          >
            <FontAwesomeIcon icon={faSignInAlt} />
            &nbsp; Join
          </button>
          {children}
        </div>
      </div>
    </div>
  );
}

export default RoomsContent;
